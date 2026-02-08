#pragma once

/**
 * DWARF session management
 *
 * RAII wrapper for libdwarf operations. Handles:
 * - Opening/closing DWARF debug info from ELF/Mach-O files
 * - Compilation unit enumeration
 * - DIE (Debug Information Entry) traversal
 */

#include <string>
#include <memory>
#include <vector>
#include <functional>

#ifdef DWARFSQL_HAS_LIBDWARF
#include <libdwarf/libdwarf.h>
#include <libdwarf/dwarf.h>
#endif

namespace dwarfsql {

/**
 * Information extracted from a DIE
 */
struct DieInfo {
    uint64_t offset = 0;
    int tag = 0;
    std::string name;
    std::string linkage_name;
    uint64_t low_pc = 0;
    uint64_t high_pc = 0;
    int64_t byte_size = -1;
    int decl_file = -1;
    int decl_line = -1;
    bool is_external = false;
    bool is_declaration = false;
};

/**
 * Compilation unit information
 */
struct CompilationUnit {
    uint64_t offset = 0;
    std::string name;
    std::string comp_dir;
    std::string producer;
    int language = 0;
    uint64_t low_pc = 0;
    uint64_t high_pc = 0;
};

/**
 * Line number information
 */
struct LineInfo {
    uint64_t address = 0;
    std::string file;
    int line = 0;
    int column = 0;
    bool is_stmt = false;
    bool basic_block = false;
    bool end_sequence = false;
};

/**
 * Function parameter information
 */
struct ParameterInfo {
    uint64_t offset = 0;
    uint64_t func_offset = 0;
    std::string name;
    std::string type;
    int index = 0;
    std::string location;
};

/**
 * Local variable information
 */
struct LocalVarInfo {
    uint64_t offset = 0;
    uint64_t func_offset = 0;
    std::string name;
    std::string type;
    std::string location;
    int decl_line = 0;
    uint64_t scope_low_pc = 0;
    uint64_t scope_high_pc = 0;
};

/**
 * Base class relationship
 */
struct BaseClassInfo {
    uint64_t derived_offset = 0;
    std::string derived_name;
    uint64_t base_offset = 0;
    std::string base_name;
    int64_t data_member_offset = 0;
    bool is_virtual = false;
    int access = 0;  // 1=public, 2=protected, 3=private
};

/**
 * Function call site information (DWARF 5)
 */
struct CallInfo {
    uint64_t caller_offset = 0;
    std::string caller_name;
    uint64_t callee_offset = 0;
    std::string callee_name;
    uint64_t call_pc = 0;
    int call_line = 0;
    bool is_tail_call = false;
};

/**
 * Inlined subroutine information
 */
struct InlinedCallInfo {
    uint64_t offset = 0;
    uint64_t abstract_origin = 0;
    std::string name;
    uint64_t caller_offset = 0;
    uint64_t low_pc = 0;
    uint64_t high_pc = 0;
    int call_line = 0;
    int call_column = 0;
};

/**
 * Namespace information
 */
struct NamespaceInfo {
    uint64_t offset = 0;
    std::string name;
    uint64_t parent_offset = 0;  // 0 for global namespace
    bool is_anonymous = false;
};

/**
 * DWARF session - manages access to debug info in a binary
 */
class DwarfSession {
public:
    DwarfSession() = default;
    ~DwarfSession();

    // Non-copyable
    DwarfSession(const DwarfSession&) = delete;
    DwarfSession& operator=(const DwarfSession&) = delete;

    // Movable
    DwarfSession(DwarfSession&& other) noexcept;
    DwarfSession& operator=(DwarfSession&& other) noexcept;

    /**
     * Open a binary file containing DWARF debug info
     * @param path Path to ELF or Mach-O binary
     * @return true on success
     */
    bool open(const std::string& path);

    /**
     * Close the current session
     */
    void close();

    /**
     * Check if session is open
     */
    bool is_open() const { return is_open_; }

    /**
     * Get the file path
     */
    const std::string& path() const { return path_; }

    /**
     * Enumerate all compilation units
     */
    std::vector<CompilationUnit> get_compilation_units() const;

    /**
     * Enumerate functions
     * @param cu_filter Optional CU offset filter (-1 for all)
     */
    std::vector<DieInfo> get_functions(int64_t cu_filter = -1) const;

    /**
     * Enumerate variables
     * @param cu_filter Optional CU offset filter (-1 for all)
     * @param func_filter Optional function offset filter (-1 for all)
     */
    std::vector<DieInfo> get_variables(int64_t cu_filter = -1, int64_t func_filter = -1) const;

    /**
     * Enumerate types (base types, typedefs, etc.)
     * @param cu_filter Optional CU offset filter (-1 for all)
     */
    std::vector<DieInfo> get_types(int64_t cu_filter = -1) const;

    /**
     * Enumerate structures/classes/unions
     * @param cu_filter Optional CU offset filter (-1 for all)
     */
    std::vector<DieInfo> get_structs(int64_t cu_filter = -1) const;

    /**
     * Get members of a struct/class/union
     * @param struct_offset DIE offset of the struct
     */
    std::vector<DieInfo> get_struct_members(uint64_t struct_offset) const;

    /**
     * Enumerate enums
     * @param cu_filter Optional CU offset filter (-1 for all)
     */
    std::vector<DieInfo> get_enums(int64_t cu_filter = -1) const;

    /**
     * Get enum values
     * @param enum_offset DIE offset of the enum
     */
    std::vector<DieInfo> get_enum_values(uint64_t enum_offset) const;

    /**
     * Get line number information
     * @param cu_filter Optional CU offset filter (-1 for all)
     */
    std::vector<LineInfo> get_line_info(int64_t cu_filter = -1) const;

    /**
     * Get function parameters
     * @param func_filter Optional function offset filter (-1 for all)
     */
    std::vector<ParameterInfo> get_parameters(int64_t func_filter = -1) const;

    /**
     * Get local variables
     * @param func_filter Optional function offset filter (-1 for all)
     */
    std::vector<LocalVarInfo> get_local_variables(int64_t func_filter = -1) const;

    /**
     * Get base class relationships
     */
    std::vector<BaseClassInfo> get_base_classes() const;

    /**
     * Get function call sites (DWARF 5)
     */
    std::vector<CallInfo> get_calls() const;

    /**
     * Get inlined subroutines
     */
    std::vector<InlinedCallInfo> get_inlined_calls() const;

    /**
     * Get namespaces
     */
    std::vector<NamespaceInfo> get_namespaces() const;

    /**
     * Get error message from last failed operation
     */
    const std::string& last_error() const { return last_error_; }

private:
#ifdef DWARFSQL_HAS_LIBDWARF
    Dwarf_Debug dbg_ = nullptr;
#else
    void* dbg_ = nullptr;
#endif
    int fd_ = -1;
    bool is_open_ = false;
    std::string path_;
    std::string last_error_;

    // Helper methods
    void iterate_dies(int tag_filter, std::function<void(const DieInfo&)> callback) const;
};

} // namespace dwarfsql
