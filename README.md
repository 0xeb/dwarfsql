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

# Server mode (TCP)
dwarfsql a.out --server 17199

# HTTP REST server
dwarfsql a.out --http 8080

# MCP server (for Claude Desktop / AI tools)
dwarfsql a.out --mcp 9000

# Remote client
dwarfsql --remote localhost:17199 -q "SELECT * FROM compilation_units"
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

## AI Agent Mode

With AI agent support, you can query in natural language:

```
dwarfsql> Find all functions that start with 'init'
dwarfsql> What are the largest structures?
dwarfsql> Show me the enum values for error codes
```

### Prerequisites for AI Features

The AI agent requires one of these CLI tools installed and authenticated:

| Provider | CLI Tool | Install | Login |
|----------|----------|---------|-------|
| Claude (default) | [Claude Code](https://docs.anthropic.com/en/docs/claude-code) | `npm install -g @anthropic-ai/claude-code` | Run `claude`, then `/login` |
| GitHub Copilot | [Copilot CLI](https://github.com/features/copilot/cli/) | `npm install -g @github/copilot` | Run `copilot`, then `/login` |

**Important:** You must be logged in before using AI features.

### Provider Configuration

```
.agent provider claude    # or copilot
.agent byok enable
.agent byok key sk-your-key
```

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
# From monorepo root
cmake -B build -DBUILD_WITH_DWARFSQL=ON
cmake --build build

# With AI agent support
cmake -B build -DBUILD_WITH_DWARFSQL=ON -DDWARFSQL_WITH_AI_AGENT=ON
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
  dwarfsql <binary> --server [port] Start TCP server (default: 17199)
  dwarfsql <binary> --http [port]   Start HTTP REST server (default: 8080)
  dwarfsql <binary> --mcp [port]    Start MCP server (default: random 9000-9999)
  dwarfsql --remote host:port -q    Remote query
  dwarfsql --remote host:port -i    Remote interactive

Options:
  -i, --interactive   Interactive REPL mode
  -q, --query <sql>   Execute query
  --server [port]     Start TCP server mode
  --http [port]       Start HTTP REST server
  --mcp [port]        Start MCP server (Model Context Protocol)
  --bind <addr>       Bind address (default: 127.0.0.1)
  --remote host:port  Connect to remote server
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

Response format:
```json
{"success": true, "columns": [...], "rows": [[...]], "row_count": N}
```

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
- `dwarfsql_agent` - Ask natural language questions (requires AI agent build)

## REPL Commands

```
.tables         List all tables
.schema <table> Show table schema
.info           Show database info
.clear          Clear session
.quit / .exit   Exit
.help           Show help

.agent help     AI agent commands
.agent provider Show/set provider
.agent byok     BYOK configuration
```

## Author

Elias Bachaalany ([@0xeb](https://github.com/0xeb))

## License

MIT License - Copyright (c) 2025 Elias Bachaalany
