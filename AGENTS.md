# AGENTS.md - AI Agent Development Guidelines for dwarfsql

## Project Overview

dwarfsql is a SQL interface to DWARF debug information, providing virtual tables backed by libdwarf parsing of ELF and Mach-O binaries.

## Repository Structure

```
dwarfsql/
├── CMakeLists.txt           # Main build configuration
├── README.md                # User documentation
├── AGENTS.md                # This file
├── src/
│   ├── cli/
│   │   └── main.cpp         # CLI entry point
│   ├── common/
│   │   ├── agent_settings.hpp    # AI agent configuration
│   │   ├── ai_agent.hpp/cpp      # AI agent wrapper
│   │   ├── dwarfsql_commands.hpp # Command handlers
│   │   ├── http_server.hpp/cpp   # HTTP REST server
│   │   └── mcp_server.hpp/cpp    # MCP server
│   ├── include/dwarfsql/
│   │   ├── dwarfsql.hpp     # Main public header
│   │   ├── dwarf_session.hpp # DWARF session management
│   │   └── dwarf_tables.hpp  # Virtual table definitions
│   └── dwarf_tables.cpp     # Table implementations
├── prompts/
│   └── dwarfsql_agent.md    # AI agent system prompt
└── external/                # Submodules (libxsql, libagents)
```

## Build Commands

```bash
# Configure (debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug -DDWARFSQL_WITH_AI_AGENT=ON

# Configure (release)
cmake -B build -DCMAKE_BUILD_TYPE=Release -DDWARFSQL_WITH_AI_AGENT=ON

# Build
cmake --build build

# Install
cmake --install build --prefix /usr/local
```

## Development Commands

```bash
# Run with verbose output
./build/bin/dwarfsql binary.elf -i -v

# Check a specific binary
./build/bin/dwarfsql /usr/bin/ls "SELECT COUNT(*) FROM functions"

# Start HTTP server
./build/bin/dwarfsql binary.elf --http 8080

# Start MCP server
./build/bin/dwarfsql binary.elf --mcp 9000
```

## Coding Style

- **C++ Standard**: C++17
- **Naming**: PascalCase for classes, snake_case for functions/variables
- **Headers**: Use `#pragma once`
- **Includes**: Standard headers first, then third-party, then project
- **RAII**: Use RAII for resource management (libdwarf handles, file descriptors)
- **Error Handling**: Use return values and `last_error()` patterns

## Virtual Table Pattern

All tables follow this pattern:
```cpp
db.register_cached_table(
    xsql::CachedTableBuilder<RowType>("table_name")
        .column("col1", [](const RowType& r) { return r.field1; })
        .column("col2", [](const RowType& r) { return r.field2; })
        .generator([&session]() {
            std::vector<RowType> rows;
            // Populate from DWARF session
            return rows;
        })
        .build()
);
```

## Adding a New Table

1. Define row struct in `dwarf_tables.hpp`
2. Add session method in `dwarf_session.hpp`
3. Implement session method in `dwarf_tables.cpp`
4. Register table in `register_tables()`
5. Update agent prompt in `prompts/dwarfsql_agent.md`

## Platform Notes

### Linux
- Uses libdwarf from system package
- ELF binaries with DWARF sections
- May need libelf for some operations

### macOS
- Install libdwarf via Homebrew
- Mach-O binaries with DWARF (dSYM bundles)
- Framework paths may differ

### Windows
- Limited support (DWARF from MinGW/Cygwin)
- PE/COFF with DWARF sections
- Consider using PDB instead (pdbsql)

## Commit Guidelines

- Use conventional commits: `feat:`, `fix:`, `chore:`, `docs:`
- Keep commits focused and atomic
- Reference issues when applicable

## Security Notes

- Never commit API keys (use byok.env, gitignored)
- Settings stored in user home directory
- Input validation for file paths
- No network access except in AI agent mode
