/**
 * DWARF virtual table implementations
 *
 * Registers SQLite virtual tables backed by DWARF debug information.
 * DwarfSession implementation is in dwarf_session.cpp
 */

#include <dwarfsql/dwarf_tables.hpp>
#include <dwarfsql/dwarf_session.hpp>
#include <xsql/vtable.hpp>

namespace dwarfsql {

// ============================================================================
// Virtual table registration
// ============================================================================

void register_tables(xsql::Database& db, DwarfSession& session) {
    // compilation_units table
    db.register_cached_table(
        xsql::CachedTableBuilder<CompilationUnitRow>("compilation_units")
            .column_int64("id", [](const CompilationUnitRow& r) { return r.id; })
            .column_text("name", [](const CompilationUnitRow& r) { return r.name; })
            .column_text("comp_dir", [](const CompilationUnitRow& r) { return r.comp_dir; })
            .column_text("producer", [](const CompilationUnitRow& r) { return r.producer; })
            .column_int("language", [](const CompilationUnitRow& r) { return r.language; })
            .column_int64("low_pc", [](const CompilationUnitRow& r) { return r.low_pc; })
            .column_int64("high_pc", [](const CompilationUnitRow& r) { return r.high_pc; })
            .cache_builder([&session](std::vector<CompilationUnitRow>& rows) {
                auto cus = session.get_compilation_units();
                for (const auto& cu : cus) {
                    CompilationUnitRow row;
                    row.id = static_cast<int64_t>(cu.offset);
                    row.name = cu.name;
                    row.comp_dir = cu.comp_dir;
                    row.producer = cu.producer;
                    row.language = cu.language;
                    row.low_pc = static_cast<int64_t>(cu.low_pc);
                    row.high_pc = static_cast<int64_t>(cu.high_pc);
                    rows.push_back(row);
                }
            })
            .build()
    );

    // functions table
    db.register_cached_table(
        xsql::CachedTableBuilder<FunctionRow>("functions")
            .column_int64("id", [](const FunctionRow& r) { return r.id; })
            .column_int64("cu_id", [](const FunctionRow& r) { return r.cu_id; })
            .column_text("name", [](const FunctionRow& r) { return r.name; })
            .column_text("linkage_name", [](const FunctionRow& r) { return r.linkage_name; })
            .column_int64("low_pc", [](const FunctionRow& r) { return r.low_pc; })
            .column_int64("high_pc", [](const FunctionRow& r) { return r.high_pc; })
            .column_text("return_type", [](const FunctionRow& r) { return r.return_type; })
            .column_int("is_external", [](const FunctionRow& r) { return r.is_external ? 1 : 0; })
            .column_int("is_declaration", [](const FunctionRow& r) { return r.is_declaration ? 1 : 0; })
            .column_int("is_inline", [](const FunctionRow& r) { return r.is_inline ? 1 : 0; })
            .column_int("line", [](const FunctionRow& r) { return r.line; })
            .cache_builder([&session](std::vector<FunctionRow>& rows) {
                auto funcs = session.get_functions();
                for (const auto& f : funcs) {
                    FunctionRow row;
                    row.id = static_cast<int64_t>(f.offset);
                    row.cu_id = 0;  // TODO: track CU relationship
                    row.name = f.name;
                    row.linkage_name = f.linkage_name;
                    row.low_pc = static_cast<int64_t>(f.low_pc);
                    row.high_pc = static_cast<int64_t>(f.high_pc);
                    row.is_external = f.is_external;
                    row.is_declaration = f.is_declaration;
                    row.line = f.decl_line;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // variables table
    db.register_cached_table(
        xsql::CachedTableBuilder<VariableRow>("variables")
            .column_int64("id", [](const VariableRow& r) { return r.id; })
            .column_int64("cu_id", [](const VariableRow& r) { return r.cu_id; })
            .column_int64("func_id", [](const VariableRow& r) { return r.func_id; })
            .column_text("name", [](const VariableRow& r) { return r.name; })
            .column_text("type", [](const VariableRow& r) { return r.type; })
            .column_text("location", [](const VariableRow& r) { return r.location; })
            .column_int("is_parameter", [](const VariableRow& r) { return r.is_parameter ? 1 : 0; })
            .column_int("line", [](const VariableRow& r) { return r.line; })
            .cache_builder([&session](std::vector<VariableRow>& rows) {
                auto vars = session.get_variables();
                for (const auto& v : vars) {
                    VariableRow row;
                    row.id = static_cast<int64_t>(v.offset);
                    row.name = v.name;
                    row.line = v.decl_line;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // types table
    db.register_cached_table(
        xsql::CachedTableBuilder<TypeRow>("types")
            .column_int64("id", [](const TypeRow& r) { return r.id; })
            .column_int64("cu_id", [](const TypeRow& r) { return r.cu_id; })
            .column_text("name", [](const TypeRow& r) { return r.name; })
            .column_int("tag", [](const TypeRow& r) { return r.tag; })
            .column_int64("byte_size", [](const TypeRow& r) { return r.byte_size; })
            .cache_builder([&session](std::vector<TypeRow>& rows) {
                auto types = session.get_types();
                for (const auto& t : types) {
                    TypeRow row;
                    row.id = static_cast<int64_t>(t.offset);
                    row.name = t.name;
                    row.tag = t.tag;
                    row.byte_size = t.byte_size;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // structs table
    db.register_cached_table(
        xsql::CachedTableBuilder<StructRow>("structs")
            .column_int64("id", [](const StructRow& r) { return r.id; })
            .column_int64("cu_id", [](const StructRow& r) { return r.cu_id; })
            .column_text("name", [](const StructRow& r) { return r.name; })
            .column_text("kind", [](const StructRow& r) { return r.kind; })
            .column_int64("byte_size", [](const StructRow& r) { return r.byte_size; })
            .column_int("is_declaration", [](const StructRow& r) { return r.is_declaration ? 1 : 0; })
            .cache_builder([&session](std::vector<StructRow>& rows) {
                auto structs = session.get_structs();
                for (const auto& s : structs) {
                    StructRow row;
                    row.id = static_cast<int64_t>(s.offset);
                    row.name = s.name;
                    row.byte_size = s.byte_size;
                    row.is_declaration = s.is_declaration;
                    // Determine kind from tag
                    switch (s.tag) {
                        case 0x13: row.kind = "struct"; break;  // DW_TAG_structure_type
                        case 0x02: row.kind = "class"; break;   // DW_TAG_class_type
                        case 0x17: row.kind = "union"; break;   // DW_TAG_union_type
                        default: row.kind = "struct"; break;
                    }
                    rows.push_back(row);
                }
            })
            .build()
    );

    // struct_members table
    db.register_cached_table(
        xsql::CachedTableBuilder<StructMemberRow>("struct_members")
            .column_int64("id", [](const StructMemberRow& r) { return r.id; })
            .column_int64("struct_id", [](const StructMemberRow& r) { return r.struct_id; })
            .column_text("name", [](const StructMemberRow& r) { return r.name; })
            .column_text("type", [](const StructMemberRow& r) { return r.type; })
            .column_int64("offset", [](const StructMemberRow& r) { return r.offset; })
            .column_int("bit_offset", [](const StructMemberRow& r) { return r.bit_offset; })
            .column_int("bit_size", [](const StructMemberRow& r) { return r.bit_size; })
            .cache_builder([&session](std::vector<StructMemberRow>& rows) {
                auto structs = session.get_structs();
                for (const auto& s : structs) {
                    if (s.is_declaration) continue;
                    auto members = session.get_struct_members(s.offset);
                    for (const auto& m : members) {
                        StructMemberRow row;
                        row.id = static_cast<int64_t>(m.offset);
                        row.struct_id = static_cast<int64_t>(s.offset);
                        row.name = m.name;
                        row.offset = static_cast<int64_t>(m.low_pc);
                        row.bit_offset = m.decl_line;
                        row.bit_size = static_cast<int>(m.byte_size);
                        rows.push_back(row);
                    }
                }
            })
            .build()
    );

    // enums table
    db.register_cached_table(
        xsql::CachedTableBuilder<EnumRow>("enums")
            .column_int64("id", [](const EnumRow& r) { return r.id; })
            .column_int64("cu_id", [](const EnumRow& r) { return r.cu_id; })
            .column_text("name", [](const EnumRow& r) { return r.name; })
            .column_int64("byte_size", [](const EnumRow& r) { return r.byte_size; })
            .cache_builder([&session](std::vector<EnumRow>& rows) {
                auto enums = session.get_enums();
                for (const auto& e : enums) {
                    EnumRow row;
                    row.id = static_cast<int64_t>(e.offset);
                    row.name = e.name;
                    row.byte_size = e.byte_size;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // enum_values table
    db.register_cached_table(
        xsql::CachedTableBuilder<EnumValueRow>("enum_values")
            .column_int64("id", [](const EnumValueRow& r) { return r.id; })
            .column_int64("enum_id", [](const EnumValueRow& r) { return r.enum_id; })
            .column_text("name", [](const EnumValueRow& r) { return r.name; })
            .column_int64("value", [](const EnumValueRow& r) { return r.value; })
            .cache_builder([&session](std::vector<EnumValueRow>& rows) {
                auto enums = session.get_enums();
                for (const auto& e : enums) {
                    auto values = session.get_enum_values(e.offset);
                    for (const auto& v : values) {
                        EnumValueRow row;
                        row.id = static_cast<int64_t>(v.offset);
                        row.enum_id = static_cast<int64_t>(e.offset);
                        row.name = v.name;
                        row.value = v.byte_size;  // const_value stored in byte_size
                        rows.push_back(row);
                    }
                }
            })
            .build()
    );

    // line_info table
    db.register_cached_table(
        xsql::CachedTableBuilder<LineInfoRow>("line_info")
            .column_int64("address", [](const LineInfoRow& r) { return r.address; })
            .column_text("file", [](const LineInfoRow& r) { return r.file; })
            .column_int("line", [](const LineInfoRow& r) { return r.line; })
            .column_int("column", [](const LineInfoRow& r) { return r.column; })
            .column_int("is_stmt", [](const LineInfoRow& r) { return r.is_stmt ? 1 : 0; })
            .column_int("basic_block", [](const LineInfoRow& r) { return r.basic_block ? 1 : 0; })
            .column_int("end_sequence", [](const LineInfoRow& r) { return r.end_sequence ? 1 : 0; })
            .cache_builder([&session](std::vector<LineInfoRow>& rows) {
                auto lines = session.get_line_info();
                for (const auto& l : lines) {
                    LineInfoRow row;
                    row.address = static_cast<int64_t>(l.address);
                    row.file = l.file;
                    row.line = l.line;
                    row.column = l.column;
                    row.is_stmt = l.is_stmt;
                    row.basic_block = l.basic_block;
                    row.end_sequence = l.end_sequence;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // parameters table
    db.register_cached_table(
        xsql::CachedTableBuilder<ParameterRow>("parameters")
            .column_int64("id", [](const ParameterRow& r) { return r.id; })
            .column_int64("func_id", [](const ParameterRow& r) { return r.func_id; })
            .column_text("name", [](const ParameterRow& r) { return r.name; })
            .column_text("type", [](const ParameterRow& r) { return r.type; })
            .column_int("param_index", [](const ParameterRow& r) { return r.index; })
            .column_text("location", [](const ParameterRow& r) { return r.location; })
            .cache_builder([&session](std::vector<ParameterRow>& rows) {
                auto params = session.get_parameters();
                for (const auto& p : params) {
                    ParameterRow row;
                    row.id = static_cast<int64_t>(p.offset);
                    row.func_id = static_cast<int64_t>(p.func_offset);
                    row.name = p.name;
                    row.type = p.type;
                    row.index = p.index;
                    row.location = p.location;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // local_variables table
    db.register_cached_table(
        xsql::CachedTableBuilder<LocalVariableRow>("local_variables")
            .column_int64("id", [](const LocalVariableRow& r) { return r.id; })
            .column_int64("func_id", [](const LocalVariableRow& r) { return r.func_id; })
            .column_text("name", [](const LocalVariableRow& r) { return r.name; })
            .column_text("type", [](const LocalVariableRow& r) { return r.type; })
            .column_text("location", [](const LocalVariableRow& r) { return r.location; })
            .column_int("line", [](const LocalVariableRow& r) { return r.line; })
            .column_int64("scope_low_pc", [](const LocalVariableRow& r) { return r.scope_low_pc; })
            .column_int64("scope_high_pc", [](const LocalVariableRow& r) { return r.scope_high_pc; })
            .cache_builder([&session](std::vector<LocalVariableRow>& rows) {
                auto vars = session.get_local_variables();
                for (const auto& v : vars) {
                    LocalVariableRow row;
                    row.id = static_cast<int64_t>(v.offset);
                    row.func_id = static_cast<int64_t>(v.func_offset);
                    row.name = v.name;
                    row.type = v.type;
                    row.location = v.location;
                    row.line = v.decl_line;
                    row.scope_low_pc = static_cast<int64_t>(v.scope_low_pc);
                    row.scope_high_pc = static_cast<int64_t>(v.scope_high_pc);
                    rows.push_back(row);
                }
            })
            .build()
    );

    // base_classes table
    db.register_cached_table(
        xsql::CachedTableBuilder<BaseClassRow>("base_classes")
            .column_int64("derived_id", [](const BaseClassRow& r) { return r.derived_id; })
            .column_text("derived_name", [](const BaseClassRow& r) { return r.derived_name; })
            .column_int64("base_id", [](const BaseClassRow& r) { return r.base_id; })
            .column_text("base_name", [](const BaseClassRow& r) { return r.base_name; })
            .column_int64("offset", [](const BaseClassRow& r) { return r.offset; })
            .column_int("is_virtual", [](const BaseClassRow& r) { return r.is_virtual ? 1 : 0; })
            .column_text("access", [](const BaseClassRow& r) { return r.access; })
            .cache_builder([&session](std::vector<BaseClassRow>& rows) {
                auto bases = session.get_base_classes();
                for (const auto& b : bases) {
                    BaseClassRow row;
                    row.derived_id = static_cast<int64_t>(b.derived_offset);
                    row.derived_name = b.derived_name;
                    row.base_id = static_cast<int64_t>(b.base_offset);
                    row.base_name = b.base_name;
                    row.offset = b.data_member_offset;
                    row.is_virtual = b.is_virtual;
                    switch (b.access) {
                        case 1: row.access = "public"; break;
                        case 2: row.access = "protected"; break;
                        case 3: row.access = "private"; break;
                        default: row.access = ""; break;
                    }
                    rows.push_back(row);
                }
            })
            .build()
    );

    // calls table (DWARF 5 call sites)
    db.register_cached_table(
        xsql::CachedTableBuilder<CallRow>("calls")
            .column_int64("caller_id", [](const CallRow& r) { return r.caller_id; })
            .column_text("caller_name", [](const CallRow& r) { return r.caller_name; })
            .column_int64("callee_id", [](const CallRow& r) { return r.callee_id; })
            .column_text("callee_name", [](const CallRow& r) { return r.callee_name; })
            .column_int64("call_pc", [](const CallRow& r) { return r.call_pc; })
            .column_int("call_line", [](const CallRow& r) { return r.call_line; })
            .column_int("is_tail_call", [](const CallRow& r) { return r.is_tail_call ? 1 : 0; })
            .cache_builder([&session](std::vector<CallRow>& rows) {
                auto calls = session.get_calls();
                for (const auto& c : calls) {
                    CallRow row;
                    row.caller_id = static_cast<int64_t>(c.caller_offset);
                    row.caller_name = c.caller_name;
                    row.callee_id = static_cast<int64_t>(c.callee_offset);
                    row.callee_name = c.callee_name;
                    row.call_pc = static_cast<int64_t>(c.call_pc);
                    row.call_line = c.call_line;
                    row.is_tail_call = c.is_tail_call;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // inlined_calls table
    db.register_cached_table(
        xsql::CachedTableBuilder<InlinedCallRow>("inlined_calls")
            .column_int64("id", [](const InlinedCallRow& r) { return r.id; })
            .column_int64("abstract_origin", [](const InlinedCallRow& r) { return r.abstract_origin; })
            .column_text("name", [](const InlinedCallRow& r) { return r.name; })
            .column_int64("caller_id", [](const InlinedCallRow& r) { return r.caller_id; })
            .column_int64("low_pc", [](const InlinedCallRow& r) { return r.low_pc; })
            .column_int64("high_pc", [](const InlinedCallRow& r) { return r.high_pc; })
            .column_int("call_line", [](const InlinedCallRow& r) { return r.call_line; })
            .column_int("call_column", [](const InlinedCallRow& r) { return r.call_column; })
            .cache_builder([&session](std::vector<InlinedCallRow>& rows) {
                auto inlined = session.get_inlined_calls();
                for (const auto& i : inlined) {
                    InlinedCallRow row;
                    row.id = static_cast<int64_t>(i.offset);
                    row.abstract_origin = static_cast<int64_t>(i.abstract_origin);
                    row.name = i.name;
                    row.caller_id = static_cast<int64_t>(i.caller_offset);
                    row.low_pc = static_cast<int64_t>(i.low_pc);
                    row.high_pc = static_cast<int64_t>(i.high_pc);
                    row.call_line = i.call_line;
                    row.call_column = i.call_column;
                    rows.push_back(row);
                }
            })
            .build()
    );

    // namespaces table
    db.register_cached_table(
        xsql::CachedTableBuilder<NamespaceRow>("namespaces")
            .column_int64("id", [](const NamespaceRow& r) { return r.id; })
            .column_text("name", [](const NamespaceRow& r) { return r.name; })
            .column_int64("parent_id", [](const NamespaceRow& r) { return r.parent_id; })
            .column_int("is_anonymous", [](const NamespaceRow& r) { return r.is_anonymous ? 1 : 0; })
            .cache_builder([&session](std::vector<NamespaceRow>& rows) {
                auto namespaces = session.get_namespaces();
                for (const auto& ns : namespaces) {
                    NamespaceRow row;
                    row.id = static_cast<int64_t>(ns.offset);
                    row.name = ns.name;
                    row.parent_id = static_cast<int64_t>(ns.parent_offset);
                    row.is_anonymous = ns.is_anonymous;
                    rows.push_back(row);
                }
            })
            .build()
    );
}

} // namespace dwarfsql
