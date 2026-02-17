#pragma once

/**
 * DwarfsqlHTTPServer - HTTP REST server for DWARFSQL REPL
 *
 * Thin wrapper over xsql::thinclient::http_query_server.
 * Preserves the existing API for backward compatibility.
 */

#ifdef DWARFSQL_HAS_HTTP

#include <xsql/thinclient/http_query_server.hpp>

#include <string>
#include <functional>
#include <memory>

namespace dwarfsql {

// Callback for handling SQL queries
using HTTPQueryCallback = std::function<std::string(const std::string& sql)>;

class DwarfsqlHTTPServer {
public:
    DwarfsqlHTTPServer() = default;
    ~DwarfsqlHTTPServer() { stop(); }

    // Non-copyable
    DwarfsqlHTTPServer(const DwarfsqlHTTPServer&) = delete;
    DwarfsqlHTTPServer& operator=(const DwarfsqlHTTPServer&) = delete;

    int start(int port, HTTPQueryCallback query_cb,
              const std::string& bind_addr = "127.0.0.1",
              bool use_queue = false);

    void run_until_stopped();
    void stop();

    bool is_running() const { return impl_ && impl_->is_running(); }
    int port() const { return impl_ ? impl_->port() : 0; }
    std::string url() const { return impl_ ? impl_->url() : ""; }

    void set_interrupt_check(std::function<bool()> check);

private:
    std::unique_ptr<xsql::thinclient::http_query_server> impl_;
};

std::string format_http_info(int port);
std::string format_http_status(int port, bool running);

} // namespace dwarfsql

#endif // DWARFSQL_HAS_HTTP
