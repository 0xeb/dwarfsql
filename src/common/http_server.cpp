#include "http_server.hpp"

#ifdef DWARFSQL_HAS_HTTP

namespace dwarfsql {

static const char* HTTP_HELP_TEXT = R"(DWARFSQL HTTP REST API
======================

SQL interface for DWARF debug information via HTTP.

Endpoints:
  GET  /         - Welcome message
  GET  /help     - This documentation
  POST /query    - Execute SQL (body = raw SQL, response = JSON)
  GET  /status   - Server health check
  POST /shutdown - Stop server

Tables:
  compilation_units   - Compilation units (source files)
  functions           - Function symbols with addresses
  variables           - Variables (global and local)
  types               - Type definitions
  structs             - Structure/class/union definitions
  struct_members      - Structure member fields
  enums               - Enumeration definitions
  enum_values         - Enumeration values
  line_info           - Source line to address mapping
  parameters          - Function parameters
  local_variables     - Local variables
  base_classes        - Class inheritance
  inlined_calls       - Inlined function calls
  namespaces          - Namespace definitions

Response Format:
  Success: {"success": true, "columns": [...], "rows": [[...]], "row_count": N}
  Error:   {"success": false, "error": "message"}

Example:
  curl http://localhost:<port>/help
  curl -X POST http://localhost:<port>/query -d "SELECT name FROM functions LIMIT 5"
)";

int DwarfsqlHTTPServer::start(int port, HTTPQueryCallback query_cb,
                              const std::string& bind_addr, bool use_queue) {
    if (impl_ && impl_->is_running()) {
        return impl_->port();
    }

    xsql::thinclient::http_query_server_config config;
    config.tool_name = "dwarfsql";
    config.help_text = HTTP_HELP_TEXT;
    config.port = port;
    config.bind_address = bind_addr;
    config.query_fn = std::move(query_cb);
    config.use_queue = use_queue;
    config.status_fn = []() {
        return xsql::json{{"mode", "repl"}};
    };

    impl_ = std::make_unique<xsql::thinclient::http_query_server>(config);
    return impl_->start();
}

void DwarfsqlHTTPServer::run_until_stopped() {
    if (impl_) impl_->run_until_stopped();
}

void DwarfsqlHTTPServer::stop() {
    if (impl_) {
        impl_->stop();
        impl_.reset();
    }
}

void DwarfsqlHTTPServer::set_interrupt_check(std::function<bool()> check) {
    if (impl_) impl_->set_interrupt_check(std::move(check));
}

std::string format_http_info(int port) {
    return xsql::thinclient::format_http_info("dwarfsql", port);
}

std::string format_http_status(int port, bool running) {
    return xsql::thinclient::format_http_status(port, running);
}

} // namespace dwarfsql

#endif // DWARFSQL_HAS_HTTP
