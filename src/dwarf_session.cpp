/**
 * dwarf_session.cpp - DWARF session implementation using libdwarf
 *
 * Provides access to DWARF debug information in ELF and Mach-O binaries.
 * Uses libdwarf for parsing and traversal.
 */

#include <dwarfsql/dwarf_session.hpp>

#include <cstring>
#include <unordered_map>
#include <algorithm>

#ifdef DWARFSQL_HAS_LIBDWARF
#include <fcntl.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#endif

namespace dwarfsql {

// ============================================================================
// Helper functions (internal)
// ============================================================================

#ifdef DWARFSQL_HAS_LIBDWARF

namespace {

// Get string attribute from DIE
std::string get_die_string(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return "";
    }

    char* str = nullptr;
    if (dwarf_formstring(at, &str, &err) != DW_DLV_OK) {
        dwarf_dealloc_attribute(at);
        return "";
    }

    std::string result(str);
    dwarf_dealloc_attribute(at);
    return result;
}

// Get unsigned attribute from DIE
uint64_t get_die_unsigned(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr, uint64_t default_val = 0) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return default_val;
    }

    Dwarf_Unsigned val;
    if (dwarf_formudata(at, &val, &err) != DW_DLV_OK) {
        // Try as address
        Dwarf_Addr addr;
        if (dwarf_formaddr(at, &addr, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(at);
            return default_val;
        }
        dwarf_dealloc_attribute(at);
        return addr;
    }

    dwarf_dealloc_attribute(at);
    return val;
}

// Get signed attribute from DIE
int64_t get_die_signed(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr, int64_t default_val = -1) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return default_val;
    }

    Dwarf_Signed val;
    if (dwarf_formsdata(at, &val, &err) != DW_DLV_OK) {
        // Try unsigned
        Dwarf_Unsigned uval;
        if (dwarf_formudata(at, &uval, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(at);
            return default_val;
        }
        dwarf_dealloc_attribute(at);
        return static_cast<int64_t>(uval);
    }

    dwarf_dealloc_attribute(at);
    return val;
}

// Check if DIE has attribute
bool has_die_attr(Dwarf_Die die, Dwarf_Half attr) {
    Dwarf_Bool has_attr = 0;
    Dwarf_Error err = nullptr;
    if (dwarf_hasattr(die, attr, &has_attr, &err) != DW_DLV_OK) {
        return false;
    }
    return has_attr != 0;
}

// Get flag attribute from DIE
bool get_die_flag(Dwarf_Die die, Dwarf_Half attr) {
    Dwarf_Bool flag = 0;
    Dwarf_Error err = nullptr;

    Dwarf_Attribute at;
    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return false;
    }

    if (dwarf_formflag(at, &flag, &err) != DW_DLV_OK) {
        dwarf_dealloc_attribute(at);
        return false;
    }

    dwarf_dealloc_attribute(at);
    return flag != 0;
}

// Get DIE offset
uint64_t get_die_offset(Dwarf_Die die) {
    Dwarf_Off off;
    Dwarf_Error err = nullptr;
    if (dwarf_dieoffset(die, &off, &err) != DW_DLV_OK) {
        return 0;
    }
    return off;
}

// Get DIE tag
int get_die_tag(Dwarf_Die die) {
    Dwarf_Half tag;
    Dwarf_Error err = nullptr;
    if (dwarf_tag(die, &tag, &err) != DW_DLV_OK) {
        return 0;
    }
    return tag;
}

// Get referenced DIE offset (for DW_AT_type, DW_AT_abstract_origin, etc.)
uint64_t get_die_ref(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return 0;
    }

    Dwarf_Off off;
    // Try CU-relative ref first, then global ref
    if (dwarf_formref(at, &off, &err) != DW_DLV_OK) {
        if (dwarf_global_formref(at, &off, &err) != DW_DLV_OK) {
            dwarf_dealloc_attribute(at);
            return 0;
        }
    }

    dwarf_dealloc_attribute(at);
    return off;
}

// Get high_pc considering both forms (address and offset from low_pc)
uint64_t get_high_pc(Dwarf_Debug dbg, Dwarf_Die die, uint64_t low_pc) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, DW_AT_high_pc, &at, &err) != DW_DLV_OK) {
        return 0;
    }

    Dwarf_Half form;
    if (dwarf_whatform(at, &form, &err) != DW_DLV_OK) {
        dwarf_dealloc_attribute(at);
        return 0;
    }

    uint64_t result = 0;

    // Check if it's an address or a constant (offset)
    if (form == DW_FORM_addr) {
        Dwarf_Addr addr;
        if (dwarf_formaddr(at, &addr, &err) == DW_DLV_OK) {
            result = addr;
        }
    } else {
        // It's a constant offset from low_pc
        Dwarf_Unsigned val;
        if (dwarf_formudata(at, &val, &err) == DW_DLV_OK) {
            result = low_pc + val;
        }
    }

    dwarf_dealloc_attribute(at);
    return result;
}

// Get location expression as string
std::string get_location_string(Dwarf_Debug dbg, Dwarf_Die die, Dwarf_Half attr) {
    Dwarf_Attribute at;
    Dwarf_Error err = nullptr;

    if (dwarf_attr(die, attr, &at, &err) != DW_DLV_OK) {
        return "";
    }

    Dwarf_Half form;
    if (dwarf_whatform(at, &form, &err) != DW_DLV_OK) {
        dwarf_dealloc_attribute(at);
        return "";
    }

    std::string result;

    // Handle exprloc form
    if (form == DW_FORM_exprloc || form == DW_FORM_block1 ||
        form == DW_FORM_block2 || form == DW_FORM_block4 || form == DW_FORM_block) {
        Dwarf_Unsigned len;
        Dwarf_Ptr block;
        if (dwarf_formexprloc(at, &len, &block, &err) == DW_DLV_OK) {
            // Convert to hex string for now
            const uint8_t* data = static_cast<const uint8_t*>(block);
            char buf[8];
            for (Dwarf_Unsigned i = 0; i < len && i < 32; ++i) {
                snprintf(buf, sizeof(buf), "%02x", data[i]);
                result += buf;
            }
            if (len > 32) {
                result += "...";
            }
        }
    } else if (form == DW_FORM_sec_offset) {
        // Location list - just indicate it exists
        result = "[loclist]";
    }

    dwarf_dealloc_attribute(at);
    return result;
}

// Get type name by following DW_AT_type reference
std::string get_type_name(Dwarf_Debug dbg, Dwarf_Die die) {
    uint64_t type_off = get_die_ref(dbg, die, DW_AT_type);
    if (type_off == 0) {
        return "void";
    }

    Dwarf_Die type_die;
    Dwarf_Bool is_info = true;
    Dwarf_Error err = nullptr;

    if (dwarf_offdie_b(dbg, type_off, is_info, &type_die, &err) != DW_DLV_OK) {
        return "<unknown>";
    }

    std::string name = get_die_string(dbg, type_die, DW_AT_name);
    int tag = get_die_tag(type_die);

    // Handle modifiers
    std::string prefix, suffix;
    while (name.empty()) {
        switch (tag) {
            case DW_TAG_pointer_type:
                suffix = "*" + suffix;
                break;
            case DW_TAG_reference_type:
                suffix = "&" + suffix;
                break;
            case DW_TAG_rvalue_reference_type:
                suffix = "&&" + suffix;
                break;
            case DW_TAG_const_type:
                prefix = "const " + prefix;
                break;
            case DW_TAG_volatile_type:
                prefix = "volatile " + prefix;
                break;
            case DW_TAG_restrict_type:
                prefix = "restrict " + prefix;
                break;
            case DW_TAG_array_type:
                suffix = "[]" + suffix;
                break;
            default:
                dwarf_dealloc_die(type_die);
                return prefix + "<anonymous>" + suffix;
        }

        // Follow the chain
        type_off = get_die_ref(dbg, type_die, DW_AT_type);
        dwarf_dealloc_die(type_die);

        if (type_off == 0) {
            return prefix + "void" + suffix;
        }

        if (dwarf_offdie_b(dbg, type_off, is_info, &type_die, &err) != DW_DLV_OK) {
            return prefix + "<unknown>" + suffix;
        }

        name = get_die_string(dbg, type_die, DW_AT_name);
        tag = get_die_tag(type_die);
    }

    dwarf_dealloc_die(type_die);
    return prefix + name + suffix;
}

// Access specifier to string
std::string access_to_string(int access) {
    switch (access) {
        case DW_ACCESS_public: return "public";
        case DW_ACCESS_protected: return "protected";
        case DW_ACCESS_private: return "private";
        default: return "private";  // C++ default
    }
}

// Recursive DIE traversal helper
void traverse_die(Dwarf_Debug dbg, Dwarf_Die die, int depth,
                  std::function<void(Dwarf_Die, int)> callback) {
    callback(die, depth);

    Dwarf_Die child;
    Dwarf_Error err = nullptr;

    if (dwarf_child(die, &child, &err) == DW_DLV_OK) {
        traverse_die(dbg, child, depth + 1, callback);

        Dwarf_Die sibling;
        while (dwarf_siblingof_b(dbg, child, true, &sibling, &err) == DW_DLV_OK) {
            dwarf_dealloc_die(child);
            child = sibling;
            traverse_die(dbg, child, depth + 1, callback);
        }
        dwarf_dealloc_die(child);
    }
}

} // anonymous namespace

#endif // DWARFSQL_HAS_LIBDWARF

// ============================================================================
// DwarfSession implementation
// ============================================================================

DwarfSession::~DwarfSession() {
    close();
}

DwarfSession::DwarfSession(DwarfSession&& other) noexcept
    : dbg_(other.dbg_)
    , fd_(other.fd_)
    , is_open_(other.is_open_)
    , path_(std::move(other.path_))
    , last_error_(std::move(other.last_error_))
{
    other.dbg_ = nullptr;
    other.fd_ = -1;
    other.is_open_ = false;
}

DwarfSession& DwarfSession::operator=(DwarfSession&& other) noexcept {
    if (this != &other) {
        close();
        dbg_ = other.dbg_;
        fd_ = other.fd_;
        is_open_ = other.is_open_;
        path_ = std::move(other.path_);
        last_error_ = std::move(other.last_error_);
        other.dbg_ = nullptr;
        other.fd_ = -1;
        other.is_open_ = false;
    }
    return *this;
}

bool DwarfSession::open(const std::string& path) {
#ifdef DWARFSQL_HAS_LIBDWARF
    close();

    fd_ = ::open(path.c_str(), O_RDONLY);
    if (fd_ < 0) {
        last_error_ = "Failed to open file: " + path;
        return false;
    }

    Dwarf_Error err = nullptr;
    unsigned int group_number = DW_GROUPNUMBER_ANY;

    int res = dwarf_init_b(fd_, DW_DLC_READ, group_number, nullptr, nullptr, &dbg_, &err);

    if (res == DW_DLV_NO_ENTRY) {
        last_error_ = "No DWARF debug info found in: " + path;
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    if (res != DW_DLV_OK) {
        last_error_ = "Failed to initialize DWARF: ";
        if (err) {
            last_error_ += dwarf_errmsg(err);
            dwarf_dealloc_error(dbg_, err);
        }
        ::close(fd_);
        fd_ = -1;
        return false;
    }

    path_ = path;
    is_open_ = true;
    return true;
#else
    last_error_ = "libdwarf not available";
    return false;
#endif
}

void DwarfSession::close() {
#ifdef DWARFSQL_HAS_LIBDWARF
    if (dbg_) {
        Dwarf_Error err = nullptr;
        dwarf_finish(dbg_, &err);
        dbg_ = nullptr;
    }
    if (fd_ >= 0) {
#ifdef _WIN32
        _close(fd_);
#else
        ::close(fd_);
#endif
        fd_ = -1;
    }
#endif
    is_open_ = false;
    path_.clear();
}

std::vector<CompilationUnit> DwarfSession::get_compilation_units() const {
    std::vector<CompilationUnit> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        CompilationUnit cu;
        cu.offset = get_die_offset(cu_die);
        cu.name = get_die_string(dbg_, cu_die, DW_AT_name);
        cu.comp_dir = get_die_string(dbg_, cu_die, DW_AT_comp_dir);
        cu.producer = get_die_string(dbg_, cu_die, DW_AT_producer);
        cu.language = static_cast<int>(get_die_unsigned(dbg_, cu_die, DW_AT_language, 0));
        cu.low_pc = get_die_unsigned(dbg_, cu_die, DW_AT_low_pc, 0);
        cu.high_pc = get_high_pc(dbg_, cu_die, cu.low_pc);

        result.push_back(cu);
        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_functions(int64_t cu_filter) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        // Skip if filtering by CU and this isn't the one
        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);
            if (tag != DW_TAG_subprogram) return;

            DieInfo info;
            info.offset = get_die_offset(die);
            info.tag = tag;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.linkage_name = get_die_string(dbg_, die, DW_AT_linkage_name);
            if (info.linkage_name.empty()) {
                info.linkage_name = get_die_string(dbg_, die, DW_AT_MIPS_linkage_name);
            }
            info.low_pc = get_die_unsigned(dbg_, die, DW_AT_low_pc, 0);
            info.high_pc = get_high_pc(dbg_, die, info.low_pc);
            info.decl_line = static_cast<int>(get_die_signed(dbg_, die, DW_AT_decl_line, 0));
            info.is_external = get_die_flag(die, DW_AT_external);
            info.is_declaration = get_die_flag(die, DW_AT_declaration);

            // Check for inline
            Dwarf_Attribute at;
            if (dwarf_attr(die, DW_AT_inline, &at, &err) == DW_DLV_OK) {
                Dwarf_Unsigned val;
                if (dwarf_formudata(at, &val, &err) == DW_DLV_OK) {
                    info.is_declaration = (val == DW_INL_declared_inlined || val == DW_INL_declared_not_inlined);
                }
                dwarf_dealloc_attribute(at);
            }

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_variables(int64_t cu_filter, int64_t func_filter) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;
    uint64_t current_func = 0;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            // Track current function
            if (tag == DW_TAG_subprogram) {
                current_func = get_die_offset(die);
            }

            if (tag != DW_TAG_variable && tag != DW_TAG_formal_parameter) return;

            // Filter by function if requested
            if (func_filter >= 0 && current_func != static_cast<uint64_t>(func_filter)) {
                return;
            }

            DieInfo info;
            info.offset = get_die_offset(die);
            info.tag = tag;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.decl_line = static_cast<int>(get_die_signed(dbg_, die, DW_AT_decl_line, 0));
            info.is_external = get_die_flag(die, DW_AT_external);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_types(int64_t cu_filter) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            // Type tags we care about
            if (tag != DW_TAG_base_type &&
                tag != DW_TAG_typedef &&
                tag != DW_TAG_pointer_type &&
                tag != DW_TAG_reference_type &&
                tag != DW_TAG_rvalue_reference_type &&
                tag != DW_TAG_const_type &&
                tag != DW_TAG_volatile_type &&
                tag != DW_TAG_array_type) {
                return;
            }

            DieInfo info;
            info.offset = get_die_offset(die);
            info.tag = tag;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.byte_size = get_die_signed(dbg_, die, DW_AT_byte_size, -1);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_structs(int64_t cu_filter) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag != DW_TAG_structure_type &&
                tag != DW_TAG_class_type &&
                tag != DW_TAG_union_type) {
                return;
            }

            DieInfo info;
            info.offset = get_die_offset(die);
            info.tag = tag;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.byte_size = get_die_signed(dbg_, die, DW_AT_byte_size, -1);
            info.is_declaration = get_die_flag(die, DW_AT_declaration);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_struct_members(uint64_t struct_offset) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Die struct_die;
    Dwarf_Bool is_info = true;

    if (dwarf_offdie_b(dbg_, struct_offset, is_info, &struct_die, &err) != DW_DLV_OK) {
        return result;
    }

    Dwarf_Die child;
    if (dwarf_child(struct_die, &child, &err) == DW_DLV_OK) {
        do {
            int tag = get_die_tag(child);
            if (tag == DW_TAG_member) {
                DieInfo info;
                info.offset = get_die_offset(child);
                info.tag = tag;
                info.name = get_die_string(dbg_, child, DW_AT_name);

                // Get data member location (offset in struct)
                Dwarf_Attribute at;
                if (dwarf_attr(child, DW_AT_data_member_location, &at, &err) == DW_DLV_OK) {
                    Dwarf_Unsigned loc;
                    if (dwarf_formudata(at, &loc, &err) == DW_DLV_OK) {
                        info.low_pc = loc;  // Using low_pc to store offset
                    }
                    dwarf_dealloc_attribute(at);
                }

                // Bit field info
                info.byte_size = get_die_signed(dbg_, child, DW_AT_bit_size, 0);
                info.decl_line = static_cast<int>(get_die_signed(dbg_, child, DW_AT_bit_offset, 0));

                result.push_back(info);
            }

            Dwarf_Die sibling;
            if (dwarf_siblingof_b(dbg_, child, true, &sibling, &err) != DW_DLV_OK) {
                dwarf_dealloc_die(child);
                break;
            }
            dwarf_dealloc_die(child);
            child = sibling;
        } while (true);
    }

    dwarf_dealloc_die(struct_die);
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_enums(int64_t cu_filter) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag != DW_TAG_enumeration_type) return;

            DieInfo info;
            info.offset = get_die_offset(die);
            info.tag = tag;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.byte_size = get_die_signed(dbg_, die, DW_AT_byte_size, -1);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<DieInfo> DwarfSession::get_enum_values(uint64_t enum_offset) const {
    std::vector<DieInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Die enum_die;
    Dwarf_Bool is_info = true;

    if (dwarf_offdie_b(dbg_, enum_offset, is_info, &enum_die, &err) != DW_DLV_OK) {
        return result;
    }

    Dwarf_Die child;
    if (dwarf_child(enum_die, &child, &err) == DW_DLV_OK) {
        do {
            int tag = get_die_tag(child);
            if (tag == DW_TAG_enumerator) {
                DieInfo info;
                info.offset = get_die_offset(child);
                info.tag = tag;
                info.name = get_die_string(dbg_, child, DW_AT_name);
                info.byte_size = get_die_signed(dbg_, child, DW_AT_const_value, 0);

                result.push_back(info);
            }

            Dwarf_Die sibling;
            if (dwarf_siblingof_b(dbg_, child, true, &sibling, &err) != DW_DLV_OK) {
                dwarf_dealloc_die(child);
                break;
            }
            dwarf_dealloc_die(child);
            child = sibling;
        } while (true);
    }

    dwarf_dealloc_die(enum_die);
#endif

    return result;
}

std::vector<LineInfo> DwarfSession::get_line_info(int64_t cu_filter) const {
    std::vector<LineInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t cu_offset = get_die_offset(cu_die);

        if (cu_filter >= 0 && cu_offset != static_cast<uint64_t>(cu_filter)) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        // Get line context
        Dwarf_Line_Context line_context;
        Dwarf_Unsigned line_version;
        Dwarf_Small table_count;

        int res = dwarf_srclines_b(cu_die, &line_version, &table_count, &line_context, &err);
        if (res != DW_DLV_OK) {
            dwarf_dealloc_die(cu_die);
            continue;
        }

        Dwarf_Line* lines;
        Dwarf_Signed line_count;

        res = dwarf_srclines_from_linecontext(line_context, &lines, &line_count, &err);
        if (res == DW_DLV_OK) {
            for (Dwarf_Signed i = 0; i < line_count; ++i) {
                LineInfo info;

                Dwarf_Addr addr;
                if (dwarf_lineaddr(lines[i], &addr, &err) == DW_DLV_OK) {
                    info.address = addr;
                }

                char* filename;
                if (dwarf_linesrc(lines[i], &filename, &err) == DW_DLV_OK) {
                    info.file = filename;
                }

                Dwarf_Unsigned lineno;
                if (dwarf_lineno(lines[i], &lineno, &err) == DW_DLV_OK) {
                    info.line = static_cast<int>(lineno);
                }

                Dwarf_Unsigned col;
                if (dwarf_lineoff_b(lines[i], &col, &err) == DW_DLV_OK) {
                    info.column = static_cast<int>(col);
                }

                Dwarf_Bool is_stmt;
                if (dwarf_linebeginstatement(lines[i], &is_stmt, &err) == DW_DLV_OK) {
                    info.is_stmt = is_stmt != 0;
                }

                Dwarf_Bool bb;
                if (dwarf_lineblock(lines[i], &bb, &err) == DW_DLV_OK) {
                    info.basic_block = bb != 0;
                }

                Dwarf_Bool end_seq;
                if (dwarf_lineendsequence(lines[i], &end_seq, &err) == DW_DLV_OK) {
                    info.end_sequence = end_seq != 0;
                }

                result.push_back(info);
            }
        }

        dwarf_srclines_dealloc_b(line_context);
        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<ParameterInfo> DwarfSession::get_parameters(int64_t func_filter) const {
    std::vector<ParameterInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t current_func = 0;
        int param_index = 0;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_subprogram) {
                current_func = get_die_offset(die);
                param_index = 0;
                return;
            }

            if (tag != DW_TAG_formal_parameter) return;

            if (func_filter >= 0 && current_func != static_cast<uint64_t>(func_filter)) {
                return;
            }

            ParameterInfo info;
            info.offset = get_die_offset(die);
            info.func_offset = current_func;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.type = get_type_name(dbg_, die);
            info.index = param_index++;
            info.location = get_location_string(dbg_, die, DW_AT_location);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<LocalVarInfo> DwarfSession::get_local_variables(int64_t func_filter) const {
    std::vector<LocalVarInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t current_func = 0;
        uint64_t scope_low = 0;
        uint64_t scope_high = 0;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_subprogram) {
                current_func = get_die_offset(die);
                scope_low = get_die_unsigned(dbg_, die, DW_AT_low_pc, 0);
                scope_high = get_high_pc(dbg_, die, scope_low);
                return;
            }

            if (tag == DW_TAG_lexical_block) {
                scope_low = get_die_unsigned(dbg_, die, DW_AT_low_pc, 0);
                scope_high = get_high_pc(dbg_, die, scope_low);
                return;
            }

            if (tag != DW_TAG_variable) return;

            // Skip if at CU level (global variable)
            if (depth <= 1) return;

            if (func_filter >= 0 && current_func != static_cast<uint64_t>(func_filter)) {
                return;
            }

            LocalVarInfo info;
            info.offset = get_die_offset(die);
            info.func_offset = current_func;
            info.name = get_die_string(dbg_, die, DW_AT_name);
            info.type = get_type_name(dbg_, die);
            info.location = get_location_string(dbg_, die, DW_AT_location);
            info.decl_line = static_cast<int>(get_die_signed(dbg_, die, DW_AT_decl_line, 0));
            info.scope_low_pc = scope_low;
            info.scope_high_pc = scope_high;

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<BaseClassInfo> DwarfSession::get_base_classes() const {
    std::vector<BaseClassInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t current_class = 0;
        std::string current_class_name;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_structure_type || tag == DW_TAG_class_type) {
                current_class = get_die_offset(die);
                current_class_name = get_die_string(dbg_, die, DW_AT_name);
                return;
            }

            if (tag != DW_TAG_inheritance) return;

            BaseClassInfo info;
            info.derived_offset = current_class;
            info.derived_name = current_class_name;
            info.base_offset = get_die_ref(dbg_, die, DW_AT_type);

            // Get base class name by following the type reference
            Dwarf_Die base_die;
            if (dwarf_offdie_b(dbg_, info.base_offset, true, &base_die, &err) == DW_DLV_OK) {
                info.base_name = get_die_string(dbg_, base_die, DW_AT_name);
                dwarf_dealloc_die(base_die);
            }

            info.data_member_offset = get_die_signed(dbg_, die, DW_AT_data_member_location, 0);
            info.is_virtual = has_die_attr(die, DW_AT_virtuality);
            info.access = static_cast<int>(get_die_unsigned(dbg_, die, DW_AT_accessibility, DW_ACCESS_private));

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<CallInfo> DwarfSession::get_calls() const {
    std::vector<CallInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t current_func = 0;
        std::string current_func_name;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_subprogram) {
                current_func = get_die_offset(die);
                current_func_name = get_die_string(dbg_, die, DW_AT_name);
                return;
            }

            // DWARF 5 call site
            if (tag != DW_TAG_call_site && tag != DW_TAG_GNU_call_site) return;

            CallInfo info;
            info.caller_offset = current_func;
            info.caller_name = current_func_name;

            // Get callee through DW_AT_call_origin
            uint64_t callee_off = get_die_ref(dbg_, die, DW_AT_call_origin);
            if (callee_off == 0) {
                callee_off = get_die_ref(dbg_, die, DW_AT_abstract_origin);
            }

            if (callee_off != 0) {
                info.callee_offset = callee_off;
                Dwarf_Die callee_die;
                if (dwarf_offdie_b(dbg_, callee_off, true, &callee_die, &err) == DW_DLV_OK) {
                    info.callee_name = get_die_string(dbg_, callee_die, DW_AT_name);
                    dwarf_dealloc_die(callee_die);
                }
            }

            info.call_pc = get_die_unsigned(dbg_, die, DW_AT_call_return_pc, 0);
            if (info.call_pc == 0) {
                info.call_pc = get_die_unsigned(dbg_, die, DW_AT_low_pc, 0);
            }
            info.call_line = static_cast<int>(get_die_signed(dbg_, die, DW_AT_call_line, 0));
            info.is_tail_call = get_die_flag(die, DW_AT_call_tail_call);

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<InlinedCallInfo> DwarfSession::get_inlined_calls() const {
    std::vector<InlinedCallInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t current_func = 0;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_subprogram) {
                current_func = get_die_offset(die);
                return;
            }

            if (tag != DW_TAG_inlined_subroutine) return;

            InlinedCallInfo info;
            info.offset = get_die_offset(die);
            info.abstract_origin = get_die_ref(dbg_, die, DW_AT_abstract_origin);
            info.caller_offset = current_func;

            // Get name from abstract origin
            if (info.abstract_origin != 0) {
                Dwarf_Die origin_die;
                if (dwarf_offdie_b(dbg_, info.abstract_origin, true, &origin_die, &err) == DW_DLV_OK) {
                    info.name = get_die_string(dbg_, origin_die, DW_AT_name);
                    dwarf_dealloc_die(origin_die);
                }
            }

            info.low_pc = get_die_unsigned(dbg_, die, DW_AT_low_pc, 0);
            info.high_pc = get_high_pc(dbg_, die, info.low_pc);
            info.call_line = static_cast<int>(get_die_signed(dbg_, die, DW_AT_call_line, 0));
            info.call_column = static_cast<int>(get_die_signed(dbg_, die, DW_AT_call_column, 0));

            result.push_back(info);
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

std::vector<NamespaceInfo> DwarfSession::get_namespaces() const {
    std::vector<NamespaceInfo> result;

#ifdef DWARFSQL_HAS_LIBDWARF
    if (!is_open_) return result;

    Dwarf_Error err = nullptr;
    Dwarf_Unsigned cu_header_length;
    Dwarf_Half version_stamp;
    Dwarf_Off abbrev_offset;
    Dwarf_Half address_size;
    Dwarf_Half length_size;
    Dwarf_Half extension_size;
    Dwarf_Sig8 type_signature;
    Dwarf_Unsigned typeoffset;
    Dwarf_Unsigned next_cu_header;
    Dwarf_Half header_cu_type;

    bool is_info = true;

    while (dwarf_next_cu_header_d(dbg_, is_info,
                                  &cu_header_length, &version_stamp,
                                  &abbrev_offset, &address_size,
                                  &length_size, &extension_size,
                                  &type_signature, &typeoffset,
                                  &next_cu_header, &header_cu_type,
                                  &err) == DW_DLV_OK) {

        Dwarf_Die cu_die;
        if (dwarf_siblingof_b(dbg_, nullptr, is_info, &cu_die, &err) != DW_DLV_OK) {
            continue;
        }

        uint64_t parent_ns = 0;

        traverse_die(dbg_, cu_die, 0, [&](Dwarf_Die die, int depth) {
            int tag = get_die_tag(die);

            if (tag == DW_TAG_namespace) {
                NamespaceInfo info;
                info.offset = get_die_offset(die);
                info.name = get_die_string(dbg_, die, DW_AT_name);
                info.parent_offset = parent_ns;
                info.is_anonymous = info.name.empty();

                result.push_back(info);
                parent_ns = info.offset;
            }
        });

        dwarf_dealloc_die(cu_die);
    }
#endif

    return result;
}

void DwarfSession::iterate_dies(int tag_filter, std::function<void(const DieInfo&)> callback) const {
    // Implementation moved to individual getters for better control
}

} // namespace dwarfsql
