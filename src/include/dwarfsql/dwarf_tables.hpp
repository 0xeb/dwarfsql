#pragma once

/**
 * DWARF virtual tables for SQLite
 *
 * Exposes DWARF debug information through SQLite virtual tables:
 * - compilation_units
 * - functions
 * - variables
 * - types
 * - structs
 * - struct_members
 * - enums
 * - enum_values
 * - line_info
 * - parameters
 * - local_variables
 * - base_classes
 * - calls
 * - inlined_calls
 * - namespaces
 */

#include <xsql/database.hpp>
#include "dwarf_session.hpp"

namespace dwarfsql {

/**
 * Register all DWARF virtual tables with a database
 * @param db Database to register tables with
 * @param session DWARF session providing data
 */
void register_tables(xsql::Database& db, DwarfSession& session);

// ============================================================================
// Row structures for each table
// ============================================================================

struct CompilationUnitRow {
    int64_t id;
    std::string name;
    std::string comp_dir;
    std::string producer;
    int language;
    int64_t low_pc;
    int64_t high_pc;
};

struct FunctionRow {
    int64_t id;
    int64_t cu_id;
    std::string name;
    std::string linkage_name;
    int64_t low_pc;
    int64_t high_pc;
    std::string return_type;
    bool is_external;
    bool is_declaration;
    bool is_inline;
    int line;
};

struct VariableRow {
    int64_t id;
    int64_t cu_id;
    int64_t func_id;  // -1 for global
    std::string name;
    std::string type;
    std::string location;
    bool is_parameter;
    int line;
};

struct TypeRow {
    int64_t id;
    int64_t cu_id;
    std::string name;
    int tag;
    int64_t byte_size;
};

struct StructRow {
    int64_t id;
    int64_t cu_id;
    std::string name;
    std::string kind;  // "struct", "class", "union"
    int64_t byte_size;
    bool is_declaration;
};

struct StructMemberRow {
    int64_t id;
    int64_t struct_id;
    std::string name;
    std::string type;
    int64_t offset;
    int bit_offset;
    int bit_size;
};

struct EnumRow {
    int64_t id;
    int64_t cu_id;
    std::string name;
    int64_t byte_size;
};

struct EnumValueRow {
    int64_t id;
    int64_t enum_id;
    std::string name;
    int64_t value;
};

struct LineInfoRow {
    int64_t address;
    std::string file;
    int line;
    int column;
    bool is_stmt;
    bool basic_block;
    bool end_sequence;
};

struct ParameterRow {
    int64_t id;
    int64_t func_id;
    std::string name;
    std::string type;
    int index;
    std::string location;
};

struct LocalVariableRow {
    int64_t id;
    int64_t func_id;
    std::string name;
    std::string type;
    std::string location;
    int line;
    int64_t scope_low_pc;
    int64_t scope_high_pc;
};

struct BaseClassRow {
    int64_t derived_id;
    std::string derived_name;
    int64_t base_id;
    std::string base_name;
    int64_t offset;
    bool is_virtual;
    std::string access;  // "public", "protected", "private"
};

struct CallRow {
    int64_t caller_id;
    std::string caller_name;
    int64_t callee_id;
    std::string callee_name;
    int64_t call_pc;
    int call_line;
    bool is_tail_call;
};

struct InlinedCallRow {
    int64_t id;
    int64_t abstract_origin;
    std::string name;
    int64_t caller_id;
    int64_t low_pc;
    int64_t high_pc;
    int call_line;
    int call_column;
};

struct NamespaceRow {
    int64_t id;
    std::string name;
    int64_t parent_id;
    bool is_anonymous;
};

} // namespace dwarfsql
