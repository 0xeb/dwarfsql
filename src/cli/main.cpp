/**
 * dwarfsql CLI - Query DWARF debug information with SQL
 *
 * Usage:
 *   dwarfsql <binary> "SELECT * FROM functions"    # Query mode
 *   dwarfsql <binary> -i                           # Interactive mode
 *   dwarfsql <binary> --server [port]              # Server mode
 *   dwarfsql <binary> --http [port]                # HTTP REST server
 *   dwarfsql <binary> --mcp [port]                 # MCP server
 *   dwarfsql --remote host:port -q "..."           # Remote client mode
 */

// Windows SDK compatibility - must be before any includes
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <dwarfsql/dwarfsql.hpp>
#include <dwarfsql_commands.hpp>
#include <xsql/database.hpp>
#include <xsql/socket/server.hpp>
#include <xsql/socket/client.hpp>
#include <xsql/socket/protocol.hpp>
#include <xsql/json.hpp>

#ifdef DWARFSQL_HAS_HTTP
#include "http_server.hpp"
#endif

#ifdef DWARFSQL_HAS_AI_AGENT
#include "ai_agent.hpp"
#include "mcp_server.hpp"
#endif

#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <csignal>
#include <iomanip>
#include <mutex>
#include <thread>
#include <chrono>

namespace {

// Global for signal handling
#ifdef DWARFSQL_HAS_AI_AGENT
dwarfsql::AIAgent* g_agent = nullptr;
#endif
volatile sig_atomic_t g_quit_requested = 0;

void signal_handler(int /*sig*/) {
    g_quit_requested = 1;
#ifdef DWARFSQL_HAS_AI_AGENT
    if (g_agent) {
        g_agent->request_quit();
    }
#endif
}

void print_usage() {
    std::cout << "dwarfsql v" << dwarfsql::VERSION << " - SQL interface to DWARF debug information\n\n"
              << "Usage:\n"
              << "  dwarfsql <binary> \"<query>\"       Execute query and exit\n"
              << "  dwarfsql <binary> -i              Interactive mode\n"
              << "  dwarfsql <binary> --server [port] Start TCP server (default port: "
              << dwarfsql::DEFAULT_PORT << ")\n"
              << "  dwarfsql --remote host:port -q \"<query>\"  Remote query\n"
              << "  dwarfsql --remote host:port -i   Remote interactive\n\n"
              << "Options:\n"
              << "  -s, --source <path> Binary file path (alternative to positional)\n"
              << "  -i, --interactive   Interactive REPL mode\n"
              << "  -q, --query <sql>   Execute query\n"
              << "  --server [port]     Start server mode\n"
              << "  --remote host:port  Connect to remote server\n"
#ifdef DWARFSQL_HAS_HTTP
              << "  --http [port]       Start HTTP REST server (default: 8080)\n"
#endif
#ifdef DWARFSQL_HAS_AI_AGENT
              << "  --mcp [port]        Start MCP server (default: random 9000-9999)\n"
#endif
              << "  --bind <addr>       Bind address for server (default: 127.0.0.1)\n"
              << "  --token <token>     Authentication token\n"
              << "  -v, --verbose       Verbose output\n"
              << "  -h, --help          Show this help\n\n"
              << "Tables:\n"
              << "  compilation_units   Compilation units (source files)\n"
              << "  functions           Function symbols\n"
              << "  variables           Variables (global and local)\n"
              << "  types               Type definitions\n"
              << "  structs             Structure/class/union definitions\n"
              << "  struct_members      Structure member fields\n"
              << "  enums               Enumeration definitions\n"
              << "  enum_values         Enumeration values\n"
              << "  line_info           Source line to address mapping\n\n"
              << "Examples:\n"
              << "  dwarfsql a.out \"SELECT name, low_pc FROM functions LIMIT 10\"\n"
              << "  dwarfsql a.out -i\n"
              << "  dwarfsql a.out --server 17199\n"
#ifdef DWARFSQL_HAS_HTTP
              << "  dwarfsql a.out --http 8080\n"
#endif
#ifdef DWARFSQL_HAS_AI_AGENT
              << "  dwarfsql a.out --mcp 9000\n"
#endif
              ;
}

// Table printer for formatted output
class TablePrinter {
public:
    void set_columns(const std::vector<std::string>& cols) {
        columns_ = cols;
        widths_.resize(cols.size(), 0);
        for (size_t i = 0; i < cols.size(); ++i) {
            widths_[i] = cols[i].size();
        }
    }

    void add_row(const std::vector<std::string>& row) {
        for (size_t i = 0; i < row.size() && i < widths_.size(); ++i) {
            widths_[i] = std::max(widths_[i], row[i].size());
        }
        rows_.push_back(row);
    }

    void print(std::ostream& os) const {
        if (columns_.empty()) return;

        // Print header
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) os << " | ";
            os << std::left << std::setw(static_cast<int>(widths_[i])) << columns_[i];
        }
        os << '\n';

        // Print separator
        for (size_t i = 0; i < columns_.size(); ++i) {
            if (i > 0) os << "-+-";
            os << std::string(widths_[i], '-');
        }
        os << '\n';

        // Print rows
        for (const auto& row : rows_) {
            for (size_t i = 0; i < row.size(); ++i) {
                if (i > 0) os << " | ";
                os << std::left << std::setw(static_cast<int>(widths_[i])) << row[i];
            }
            os << '\n';
        }
        os << rows_.size() << " row(s)\n";
    }

    std::string to_string() const {
        std::ostringstream oss;
        print(oss);
        return oss.str();
    }

private:
    std::vector<std::string> columns_;
    std::vector<std::vector<std::string>> rows_;
    std::vector<size_t> widths_;
};

std::string execute_query(xsql::Database& db, const std::string& sql) {
    auto result = db.query(sql);
    if (!result.ok()) {
        return "Error: " + result.error;
    }

    if (result.empty()) {
        return "(no results)";
    }

    TablePrinter printer;
    printer.set_columns(result.columns);

    for (const auto& row : result.rows) {
        printer.add_row(row.values);
    }

    return printer.to_string();
}

std::string execute_query_json(xsql::Database& db, const std::string& sql) {
    auto result = db.query(sql);
    xsql::json j;

    if (result.ok()) {
        j["success"] = true;
        j["columns"] = result.columns;

        auto& rows = j["rows"];
        rows = xsql::json::array();
        for (const auto& row : result.rows) {
            rows.push_back(row.values);
        }
        j["row_count"] = result.rows.size();
    } else {
        j["success"] = false;
        j["error"] = result.error;
    }

    return j.dump();
}

// Convert xsql::Result to xsql::socket::QueryResult for server mode
xsql::socket::QueryResult execute_query_for_server(xsql::Database& db, const std::string& sql) {
    auto result = db.query(sql);

    xsql::socket::QueryResult qr;
    if (!result.ok()) {
        qr.success = false;
        qr.error = result.error;
        return qr;
    }

    qr.success = true;
    qr.columns = result.columns;
    for (const auto& row : result.rows) {
        qr.rows.push_back(row.values);
    }
    return qr;
}

void print_remote_result(const xsql::socket::RemoteResult& qr) {
    if (!qr.success) {
        std::cout << "Error: " << qr.error << "\n";
        return;
    }

    if (qr.rows.empty() && qr.columns.empty()) {
        std::cout << "OK\n";
        return;
    }

    TablePrinter printer;
    printer.set_columns(qr.columns);
    for (const auto& row : qr.rows) {
        printer.add_row(row.values);
    }
    printer.print(std::cout);
}

void run_interactive(xsql::Database& db, const std::string& binary_path, bool verbose) {
    dwarfsql::CommandCallbacks callbacks;
    callbacks.get_tables = [&db]() {
        return "compilation_units\nfunctions\nvariables\ntypes\nstructs\n"
               "struct_members\nenums\nenum_values\nline_info\n"
               "parameters\nlocal_variables\nbase_classes\ncalls\ninlined_calls\nnamespaces";
    };
    callbacks.get_schema = [&db](const std::string& table) {
        auto result = db.query("SELECT sql FROM sqlite_master WHERE name = '" + table + "'");
        if (result.ok() && !result.empty()) {
            return result[0][0];
        }
        return std::string("Table not found: ") + table;
    };
    callbacks.get_info = [&binary_path]() {
        return "DWARFSQL v" + std::string(dwarfsql::VERSION) + "\nBinary: " + binary_path;
    };
    callbacks.clear_session = []() {
#ifdef DWARFSQL_HAS_AI_AGENT
        if (g_agent) {
            g_agent->reset_session();
        }
#endif
        return "Session cleared";
    };

#ifdef DWARFSQL_HAS_AI_AGENT
    auto settings = dwarfsql::LoadAgentSettings();
    auto executor = [&db](const std::string& sql) { return execute_query(db, sql); };
    dwarfsql::AIAgent agent(executor, settings, verbose);
    g_agent = &agent;
    agent.load_byok_from_env();
#else
    (void)verbose;
#endif

    std::cout << "dwarfsql v" << dwarfsql::VERSION << " - Interactive mode\n"
              << "Binary: " << binary_path << "\n"
              << "Type .help for commands, .clear to reset, .quit to exit\n\n";

    std::string line;
    while (!g_quit_requested) {
        std::cout << "dwarfsql> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }

        // Trim whitespace
        size_t start = line.find_first_not_of(" \t");
        if (start == std::string::npos) continue;
        size_t end = line.find_last_not_of(" \t");
        line = line.substr(start, end - start + 1);

        if (line.empty()) continue;

        std::string output;
        auto result = dwarfsql::handle_command(line, callbacks, output);

        if (result == dwarfsql::CommandResult::QUIT) {
            break;
        } else if (result == dwarfsql::CommandResult::HANDLED) {
            if (!output.empty()) {
                std::cout << output << "\n";
            }
        } else {
            // Not a command - execute as query or send to agent
#ifdef DWARFSQL_HAS_AI_AGENT
            if (!dwarfsql::AIAgent::looks_like_sql(line)) {
                // Natural language query
                std::string response = agent.query(line);
                if (!response.empty()) {
                    std::cout << response << std::endl;
                }
            } else {
                std::cout << execute_query(db, line) << "\n";
            }
#else
            std::cout << execute_query(db, line) << "\n";
#endif
        }
    }

#ifdef DWARFSQL_HAS_AI_AGENT
    g_agent = nullptr;
#endif
}

bool parse_host_port(const std::string& spec, std::string& host, int& port) {
    auto colon = spec.rfind(':');
    if (colon == std::string::npos) {
        host = spec;
        port = dwarfsql::DEFAULT_PORT;
        return true;
    }
    host = spec.substr(0, colon);
    try {
        port = std::stoi(spec.substr(colon + 1));
        return port > 0 && port <= 65535;
    } catch (...) {
        return false;
    }
}

//=============================================================================
// HTTP Server Mode
//=============================================================================

#ifdef DWARFSQL_HAS_HTTP
static dwarfsql::DwarfsqlHTTPServer* g_http_server = nullptr;

static void http_signal_handler(int) {
    if (g_http_server) g_http_server->stop();
}

static int run_http_mode(xsql::Database& db, const std::string& binary_path,
                         int port, const std::string& bind_addr) {
    auto query_cb = [&db](const std::string& sql) -> std::string {
        return execute_query_json(db, sql);
    };

    dwarfsql::DwarfsqlHTTPServer server;
    g_http_server = &server;

    int actual_port = server.start(port, query_cb, bind_addr.empty() ? "127.0.0.1" : bind_addr, false);
    if (actual_port < 0) {
        std::cerr << "Error: Failed to start HTTP server\n";
        return 1;
    }

    auto old_handler = std::signal(SIGINT, http_signal_handler);
#ifndef _WIN32
    auto old_term_handler = std::signal(SIGTERM, http_signal_handler);
#endif

    std::cout << dwarfsql::format_http_info(actual_port);
    std::cout << "Binary: " << binary_path << "\n\n";

    // Wait for shutdown
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::signal(SIGINT, old_handler);
#ifndef _WIN32
    std::signal(SIGTERM, old_term_handler);
#endif
    g_http_server = nullptr;
    std::cout << "\nHTTP server stopped.\n";
    return 0;
}
#endif // DWARFSQL_HAS_HTTP

//=============================================================================
// MCP Server Mode
//=============================================================================

#ifdef DWARFSQL_HAS_AI_AGENT
static dwarfsql::DwarfsqlMCPServer* g_mcp_server = nullptr;

static void mcp_signal_handler(int) {
    if (g_mcp_server) g_mcp_server->stop();
}

static int run_mcp_mode(xsql::Database& db, const std::string& binary_path,
                        int port, const std::string& bind_addr, bool verbose) {
    // Create agent for natural language queries
    auto settings = dwarfsql::LoadAgentSettings();
    auto executor = [&db](const std::string& sql) { return execute_query(db, sql); };
    dwarfsql::AIAgent agent(executor, settings, verbose);
    agent.load_byok_from_env();

    // Query callback returns formatted text
    auto query_cb = [&db](const std::string& sql) -> std::string {
        return execute_query(db, sql);
    };

    // Ask callback for natural language
    auto ask_cb = [&agent](const std::string& question) -> std::string {
        return agent.query(question);
    };

    dwarfsql::DwarfsqlMCPServer server;
    g_mcp_server = &server;

    int actual_port = server.start(port, query_cb, ask_cb,
                                    bind_addr.empty() ? "127.0.0.1" : bind_addr, false);
    if (actual_port < 0) {
        std::cerr << "Error: Failed to start MCP server\n";
        return 1;
    }

    auto old_handler = std::signal(SIGINT, mcp_signal_handler);
#ifndef _WIN32
    auto old_term_handler = std::signal(SIGTERM, mcp_signal_handler);
#endif

    std::cout << dwarfsql::format_mcp_info(actual_port, true);
    std::cout << "\nBinary: " << binary_path << "\n";
    std::cout << "Press Ctrl+C to stop.\n\n";

    // Wait for shutdown
    while (server.is_running()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::signal(SIGINT, old_handler);
#ifndef _WIN32
    std::signal(SIGTERM, old_term_handler);
#endif
    g_mcp_server = nullptr;
    std::cout << "\nMCP server stopped.\n";
    return 0;
}
#endif // DWARFSQL_HAS_AI_AGENT

} // anonymous namespace

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    // Parse arguments
    std::string binary_path;
    std::string query;
    std::string remote_host;
    std::string token;
    std::string bind_addr;
    int server_port = 0;
    int http_port = 8080;
    int mcp_port = 0;  // 0 = random
    bool interactive = false;
    bool server_mode = false;
    bool http_mode = false;
    bool mcp_mode = false;
    bool verbose = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            print_usage();
            return 0;
        } else if (arg == "-i" || arg == "--interactive") {
            interactive = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-q" || arg == "--query") {
            if (i + 1 < argc) {
                query = argv[++i];
            }
        } else if (arg == "--server") {
            server_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                server_port = std::stoi(argv[++i]);
            } else {
                server_port = dwarfsql::DEFAULT_PORT;
            }
        } else if (arg == "--remote") {
            if (i + 1 < argc) {
                remote_host = argv[++i];
            }
        } else if (arg == "--token") {
            if (i + 1 < argc) {
                token = argv[++i];
            }
        } else if (arg == "--http") {
            http_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                http_port = std::stoi(argv[++i]);
            }
        } else if (arg == "--mcp") {
            mcp_mode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                mcp_port = std::stoi(argv[++i]);
            }
        } else if (arg == "--bind") {
            if (i + 1 < argc) {
                bind_addr = argv[++i];
            }
        } else if ((arg == "-s" || arg == "--source") && i + 1 < argc) {
            binary_path = argv[++i];
        } else if (arg[0] != '-' && binary_path.empty()) {
            binary_path = arg;
        } else if (arg[0] != '-' && query.empty()) {
            query = arg;
        }
    }

    // Remote mode
    if (!remote_host.empty()) {
        std::string host;
        int port;
        if (!parse_host_port(remote_host, host, port)) {
            std::cerr << "Error: Invalid remote address: " << remote_host << "\n";
            return 1;
        }

        xsql::socket::Client client;
        if (!token.empty()) {
            client.set_auth_token(token);
        }
        if (!client.connect(host, port)) {
            std::cerr << "Error: " << client.error() << "\n";
            return 1;
        }

        if (interactive) {
            std::cout << "dwarfsql - Remote interactive mode\n"
                      << "Connected to: " << host << ":" << port << "\n"
                      << "Type .quit to exit\n\n";

            std::string line;
            while (std::cout << "dwarfsql> " << std::flush && std::getline(std::cin, line)) {
                if (line == ".quit" || line == ".exit") break;
                if (line.empty()) continue;

                auto result = client.query(line);
                print_remote_result(result);
            }
        } else if (!query.empty()) {
            auto result = client.query(query);
            print_remote_result(result);
            return result.success ? 0 : 1;
        }
        return 0;
    }

    // Local mode - need binary path
    if (binary_path.empty()) {
        std::cerr << "Error: Binary path required\n";
        print_usage();
        return 1;
    }

    // Open DWARF session
    dwarfsql::DwarfSession session;
    if (!session.open(binary_path)) {
        std::cerr << "Error: " << session.last_error() << "\n";
        return 1;
    }

    // Create database and register tables
    xsql::Database db;
    dwarfsql::register_tables(db, session);

    // Set up signal handler
    std::signal(SIGINT, signal_handler);
#ifndef _WIN32
    std::signal(SIGTERM, signal_handler);
#endif

    // Server mode
    if (server_mode) {
        xsql::socket::ServerConfig cfg;
        cfg.port = server_port;
        if (!token.empty()) {
            cfg.auth_token = token;
        }

        xsql::socket::Server server(cfg);
        server.set_query_handler([&db](const std::string& sql) {
            return execute_query_for_server(db, sql);
        });

        std::cout << "dwarfsql server listening on port " << server_port << "\n"
                  << "Binary: " << binary_path << "\n"
                  << "Press Ctrl-C to stop.\n";
        server.run();
        return 0;
    }

#ifdef DWARFSQL_HAS_HTTP
    // HTTP server mode
    if (http_mode) {
        return run_http_mode(db, binary_path, http_port, bind_addr);
    }
#else
    if (http_mode) {
        std::cerr << "Error: HTTP mode not available. Rebuild with -DDWARFSQL_WITH_HTTP=ON\n";
        return 1;
    }
#endif

#ifdef DWARFSQL_HAS_AI_AGENT
    // MCP server mode
    if (mcp_mode) {
        return run_mcp_mode(db, binary_path, mcp_port, bind_addr, verbose);
    }
#else
    if (mcp_mode) {
        std::cerr << "Error: MCP mode not available. Rebuild with -DDWARFSQL_WITH_AI_AGENT=ON\n";
        return 1;
    }
#endif

    // Interactive mode
    if (interactive || query.empty()) {
        run_interactive(db, binary_path, verbose);
        return 0;
    }

    // Query mode
    std::cout << execute_query(db, query) << "\n";
    return 0;
}
