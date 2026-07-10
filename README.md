# dwarfsql

SQL interface to DWARF debug information via SQLite virtual tables.

## Overview

Query DWARF debug information from ELF (Linux) and Mach-O (macOS) binaries using SQL. dwarfsql exposes compilation units, functions, variables, types, structures, enumerations, and line number information through virtual tables.

## Quick Start

```bash
# Query functions in a binary
dwarfsql a.out "SELECT name, low_pc, high_pc FROM functions LIMIT 10"

# Interactive mode
dwarfsql a.out -i

# HTTP REST server
dwarfsql a.out --http 8080

# MCP server (for Claude Desktop / AI tools)
dwarfsql a.out --mcp 9000
```

## Tables

| Table | Description |
|-------|-------------|
| `compilation_units` | Source files (compilation units) |
| `functions` | Function symbols with addresses |
| `variables` | Global and local variables |
| `types` | Type definitions |
| `structs` | Structure/class/union definitions |
| `struct_members` | Structure member fields |
| `enums` | Enumeration definitions |
| `enum_values` | Enumeration constant values |
| `line_info` | Source line to address mapping |
| `parameters` | Function parameters with type and location |
| `local_variables` | Local variables scoped to functions |
| `base_classes` | Class inheritance relationships (C++) |
| `calls` | Function call sites (DWARF 5) |
| `inlined_calls` | Inlined subroutine instances |
| `namespaces` | C++ namespace definitions |

## Example Queries

### Find largest functions
```sql
SELECT name, (high_pc - low_pc) AS size
FROM functions
WHERE high_pc > 0
ORDER BY size DESC
LIMIT 10;
```

### List source files
```sql
SELECT name, comp_dir, producer
FROM compilation_units;
```

### Get struct layouts
```sql
SELECT s.name AS struct_name, m.name AS member, m.type, m.offset
FROM structs s
JOIN struct_members m ON s.id = m.struct_id
ORDER BY s.name, m.offset;
```

### Map address to source
```sql
SELECT file, line
FROM line_info
WHERE address <= 0x401234
ORDER BY address DESC
LIMIT 1;
```

## Using dwarfsql with an AI agent

dwarfsql is a plain SQL CLI — it does **not** embed or run its own AI agent. To let an
external agent or LLM (Claude, Copilot, or any assistant) drive dwarfsql, point it at the
tool two ways:

- **As a system prompt:** feed [`prompts/dwarfsql_agent.md`](prompts/dwarfsql_agent.md) to
  your model as its system/instruction prompt. It documents the full SQL schema, every
  table and column, and worked query patterns, so the model can translate
  natural-language questions into dwarfsql SQL and run them via `-q` / `--http` / `--mcp`.
- **Over MCP:** start `dwarfsql <binary> --mcp` and connect any MCP client (see MCP Server
  below). The client's own model does the reasoning; dwarfsql exposes the `dwarfsql_query`
  tool that executes SQL against the DWARF info.

## Building

### Prerequisites

- CMake 3.20+
- C++17 compiler
- libdwarf (DWARF parsing library)

**Ubuntu/Debian:**
```bash
apt install libdwarf-dev libelf-dev
```

**macOS:**
```bash
brew install libdwarf
```

**Fedora:**
```bash
dnf install libdwarf-devel elfutils-libelf-devel
```

### Build

```bash
# From a parent project root
cmake -B build -DBUILD_WITH_DWARFSQL=ON
cmake --build build
```

### Standalone Build

```bash
cd dwarfsql
cmake -B build
cmake --build build
```

## CLI Options

```
Usage:
  dwarfsql <binary> "<query>"       Execute query and exit
  dwarfsql <binary> -i              Interactive mode
  dwarfsql <binary> --http [port]   Start HTTP REST server (default: 8080)
  dwarfsql <binary> --mcp [port]    Start MCP server (default: random 9000-9999)

Options:
  -i, --interactive   Interactive REPL mode
  -q, --query <sql>   Execute query
  --http [port]       Start HTTP REST server
  --mcp [port]        Start MCP server (Model Context Protocol)
  --bind <addr>       Bind address (default: 127.0.0.1)
  --token <token>     Authentication token
  -v, --verbose       Verbose output
  -h, --help          Show help
```

## HTTP REST API

When started with `--http`, dwarfsql exposes a REST API:

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Welcome message |
| `/help` | GET | API documentation |
| `/query` | POST | Execute SQL (body = raw SQL) |
| `/status` | GET | Health check |
| `/shutdown` | POST | Stop server |

Example:
```bash
curl -X POST http://localhost:8080/query -d "SELECT name FROM functions LIMIT 5"
```

Response format — all `/query` responses use the canonical script envelope (single statement = array of one entry):
```json
{
  "success": true,
  "statement_count": <N>,
  "results": [
    { "statement_index": 0, "success": true, "columns": [...], "rows": [...],
      "row_count": <N>, "elapsed_ms": <ms>, "error": null }
  ],
  "row_count_total": <N>,
  "elapsed_ms_total": <ms>,
  "first_error_index": null
}
```

Bodies can be multi-statement (semicolon-separated); each `results[i]` has its own `columns`/`rows`/`row_count`/`error`. Fail-fast is the default; pass `?continue_on_error=1` to run every statement regardless of earlier failures.

## MCP Server

When started with `--mcp`, dwarfsql provides an MCP server for integration with AI tools like Claude Desktop.

```bash
# Start MCP server
dwarfsql a.out --mcp 9000
```

Add to Claude Desktop config:
```json
{
  "mcpServers": {
    "dwarfsql": {
      "url": "http://127.0.0.1:9000/sse"
    }
  }
}
```

Available MCP tools:
- `dwarfsql_query` - Execute SQL queries directly

## REPL Commands

```
.tables         List all tables
.schema <table> Show table schema
.info           Show database info
.clear          Clear session
.quit / .exit   Exit
.help           Show help
```

## The xsql family

dwarfsql is part of a family of tools that expose different binary-analysis and
debug-information platforms through the **same** SQL surface, all built on the
shared [libxsql](https://github.com/0xeb/libxsql) virtual-table framework. A
query you learn against one tool largely carries over to the others.

**Reverse-engineering platforms**
- **[idasql](https://github.com/allthingsida/idasql)** — IDA Pro databases as SQL.
- **[bnsql](https://github.com/0xeb/bnsql)** — Binary Ninja databases as SQL.
- **[ghidrasql](https://github.com/0xeb/ghidrasql)** — Ghidra databases as SQL.

**Debug info & compiler data**
- **[pdbsql](https://github.com/0xeb/pdbsql)** — Windows PDB symbol files as SQL.
- **[clangsql](https://github.com/0xeb/clangsql)** — Clang AST as SQL.

**Core**
- **[libxsql](https://github.com/0xeb/libxsql)** — the C++ SQLite virtual-table
  framework every tool above is built on.

## Author

Elias Bachaalany ([@0xeb](https://github.com/0xeb))

## License and Terms of Use

In short: you may read, build, evaluate, benchmark, package, and use unmodified dwarfsql, including commercially, if you preserve notices and follow the license terms. You may fork or patch it to prepare bug fixes, optimizations, features, tests, or documentation improvements for contribution back within the license's contribution-purpose rules.

You may not maintain a divergent private fork, port, rebrand, clone, API-compatible replacement, competing implementation, or use dwarfsql as AI input to recreate or improve a derivative implementation without prior written permission from Elias Bachaalany. Independent implementations that are not copied from, materially derived from, or substantially informed by dwarfsql in the license's defined sense are not prohibited.

Permission requests: open a GitHub issue at [0xeb/dwarfsql/issues](https://github.com/0xeb/dwarfsql/issues).

If dwarfsql materially informs a distributed project, preserve the human origin: credit dwarfsql and Elias Bachaalany visibly in your README/docs and in About/credits UI when applicable. The license includes an examples/FAQ section for common allowed and permission-required uses. Third-party dependencies (libxsql, libdwarf/libelf, and their transitive dependencies) remain under their own licenses.

See the full [Human-Origin Source License v1.0](LICENSE).
