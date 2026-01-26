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

# Server mode
dwarfsql a.out --server 17199

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

Configure the provider:
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

## Testing

```bash
# Run all tests
ctest --test-dir build --output-on-failure

# Run specific tests
./build/bin/dwarfsql_tests --gtest_filter=Commands*
```

## CLI Options

```
Usage:
  dwarfsql <binary> "<query>"       Execute query and exit
  dwarfsql <binary> -i              Interactive mode
  dwarfsql <binary> --server [port] Start TCP server (default: 17199)
  dwarfsql --remote host:port -q    Remote query
  dwarfsql --remote host:port -i    Remote interactive

Options:
  -i, --interactive   Interactive REPL mode
  -q, --query <sql>   Execute query
  --server [port]     Start server mode
  --remote host:port  Connect to remote server
  --token <token>     Authentication token
  -v, --verbose       Verbose output
  -h, --help          Show help
```

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

## License

MIT License
