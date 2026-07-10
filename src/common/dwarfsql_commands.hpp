// Copyright (c) 2024-2026 Elias Bachaalany
// SPDX-License-Identifier: LicenseRef-Human-Origin-Source-1.0
//
// This file is licensed under the Human-Origin Source License v1.0.
// See LICENSE.

#pragma once

#include <functional>
#include <string>
#include <sstream>

namespace dwarfsql {

/**
 * Command handler result
 */
enum class CommandResult {
    NOT_HANDLED,  // Not a command, process as query
    HANDLED,      // Command executed successfully
    QUIT          // User requested quit
};

/**
 * Command handler callbacks
 *
 * These callbacks allow different environments (CLI, plugin) to extend
 * command behavior. For example, .clear might:
 *   - Core: Reset the AI agent session
 *   - Plugin: Also clear output window
 */
struct CommandCallbacks {
    std::function<std::string()> get_tables;      // Return table list
    std::function<std::string(const std::string&)> get_schema;  // Return schema for table
    std::function<std::string()> get_info;        // Return database info
    std::function<std::string()> clear_session;   // Clear/reset session (agent, UI, etc.)
};

/**
 * Handle dot commands (.tables, .schema, .help, .quit, etc.)
 *
 * @param input User input line
 * @param callbacks Callbacks to execute commands
 * @param output Output string (filled if command produces output)
 * @return CommandResult indicating how to proceed
 */
inline CommandResult handle_command(
    const std::string& input,
    const CommandCallbacks& callbacks,
    std::string& output)
{
    if (input.empty() || input[0] != '.') {
        return CommandResult::NOT_HANDLED;
    }

    if (input == ".quit" || input == ".exit") {
        return CommandResult::QUIT;
    }

    if (input == ".tables") {
        if (callbacks.get_tables) {
            output = callbacks.get_tables();
        }
        return CommandResult::HANDLED;
    }

    if (input == ".info") {
        if (callbacks.get_info) {
            output = callbacks.get_info();
        }
        return CommandResult::HANDLED;
    }

    if (input == ".clear") {
        if (callbacks.clear_session) {
            output = callbacks.clear_session();
        } else {
            output = "Session cleared";
        }
        return CommandResult::HANDLED;
    }

    if (input == ".help") {
        output = "DWARFSQL Commands:\n"
                 "  .tables         List all tables\n"
                 "  .schema <table> Show table schema\n"
                 "  .info           Show database info\n"
                 "  .clear          Clear/reset session\n"
                 "  .quit / .exit   Exit\n"
                 "  .help           Show this help\n"
                 "\n"
                 "SQL:\n"
                 "  SELECT * FROM functions LIMIT 10;\n"
                 "  SELECT name, low_pc, high_pc FROM functions ORDER BY name;\n"
                 ;
        return CommandResult::HANDLED;
    }

    if (input.rfind(".schema", 0) == 0) {
        std::string table = input.length() > 8 ? input.substr(8) : "";
        // Trim leading whitespace
        size_t start = table.find_first_not_of(" \t");
        if (start != std::string::npos) {
            table = table.substr(start);
            // Trim trailing whitespace
            size_t end = table.find_last_not_of(" \t");
            if (end != std::string::npos) {
                table = table.substr(0, end + 1);
            }
        } else {
            table.clear();
        }

        if (table.empty()) {
            output = "Usage: .schema <table_name>";
        } else if (callbacks.get_schema) {
            output = callbacks.get_schema(table);
        }
        return CommandResult::HANDLED;
    }

    output = "Unknown command: " + input;
    return CommandResult::HANDLED;
}

} // namespace dwarfsql
