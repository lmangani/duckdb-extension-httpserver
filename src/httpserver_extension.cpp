#define DUCKDB_EXTENSION_MAIN
#include "httpserver_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/atomic.hpp"
#include "duckdb/common/exception/http_exception.hpp"
#include "duckdb/common/allocator.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

#include <thread>
#include <memory>
#include <cstdlib>

namespace duckdb {

struct HttpServerState {
    std::unique_ptr<duckdb_httplib_openssl::Server> server;
    std::unique_ptr<std::thread> server_thread;
    std::atomic<bool> is_running;
    DatabaseInstance* db_instance;
    unique_ptr<Allocator> allocator;

    HttpServerState() : is_running(false), db_instance(nullptr) {}
};

static HttpServerState global_state;

static void HandleQuery(const string& query, duckdb_httplib_openssl::Response& res) {
    try {
        if (!global_state.db_instance) {
            throw IOException("Database instance not initialized");
        }

        Connection con(*global_state.db_instance);
        auto result = con.Query(query);

        if (result->HasError()) {
            res.status = 400;
            res.set_content(result->GetError(), "text/plain");
            return;
        }

        res.set_content(result->ToString(), "text/plain");
    } catch (const Exception& ex) {
        res.status = 400;
        res.set_content(ex.what(), "text/plain");
    }
}

void HttpServerStart(DatabaseInstance& db, string_t host, int32_t port) {
    if (global_state.is_running) {
        throw IOException("HTTP server is already running");
    }

    global_state.db_instance = &db;
    global_state.server = make_uniq<duckdb_httplib_openssl::Server>();
    global_state.is_running = true;

    // Create a new allocator for the server thread
    global_state.allocator = make_uniq<Allocator>();

    // Handle GET requests
    global_state.server->Get("/query", [](const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
        if (!req.has_param("q")) {
            res.status = 400;
            res.set_content("Missing query parameter 'q'", "text/plain");
            return;
        }

        auto query = req.get_param_value("q");
        HandleQuery(query, res);
    });

    // Handle POST requests
    global_state.server->Post("/query", [](const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
        if (req.body.empty()) {
            res.status = 400;
            res.set_content("Empty query body", "text/plain");
            return;
        }
        HandleQuery(req.body, res);
    });

    // Health check endpoint
    global_state.server->Get("/health", [](const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
        res.set_content("OK", "text/plain");
    });

    string host_str = host.GetString();
    global_state.server_thread = make_uniq<std::thread>([host_str, port]() {
        if (!global_state.server->listen(host_str.c_str(), port)) {
            global_state.is_running = false;
            throw IOException("Failed to start HTTP server on " + host_str + ":" + std::to_string(port));
        }
    });
}

void HttpServerStop() {
    if (global_state.is_running) {
        global_state.server->stop();
        if (global_state.server_thread && global_state.server_thread->joinable()) {
            global_state.server_thread->join();
        }
        global_state.server.reset();
        global_state.server_thread.reset();
        global_state.db_instance = nullptr;
        global_state.is_running = false;

        // Reset the allocator
        global_state.allocator.reset();
    }
}

static void HttpServerCleanup() {
    HttpServerStop();
}

static void LoadInternal(DatabaseInstance &instance) {
    auto httpserve_start = ScalarFunction("httpserve_start",
                                        {LogicalType::VARCHAR, LogicalType::INTEGER},
                                        LogicalType::VARCHAR,
                                        [&](DataChunk &args, ExpressionState &state, Vector &result) {
        auto &host_vector = args.data[0];
        auto &port_vector = args.data[1];

        UnaryExecutor::Execute<string_t, string_t>(
            host_vector, result, args.size(),
            [&](string_t host) {
                auto port = ((int32_t*)port_vector.GetData())[0];
                HttpServerStart(instance, host, port);
                return StringVector::AddString(result, "HTTP server started on " + host.GetString() + ":" + std::to_string(port));
            });
    });

    auto httpserve_stop = ScalarFunction("httpserve_stop",
                                       {},
                                       LogicalType::VARCHAR,
                                       [](DataChunk &args, ExpressionState &state, Vector &result) {
        HttpServerStop();
        result.SetValue(0, Value("HTTP server stopped"));
    });

    ExtensionUtil::RegisterFunction(instance, httpserve_start);
    ExtensionUtil::RegisterFunction(instance, httpserve_stop);

    // Register the cleanup function to be called at exit
    std::atexit(HttpServerCleanup);

}

void HttpserverExtension::Load(DuckDB &db) {
    LoadInternal(*db.instance);
}

std::string HttpserverExtension::Name() {
    return "httpserver";
}

std::string HttpserverExtension::Version() const {
#ifdef EXT_VERSION_HTTPSERVER
    return EXT_VERSION_HTTPSERVER;
#else
    return "";
#endif
}

} // namespace duckdb

extern "C" {
DUCKDB_EXTENSION_API void httpserver_init(duckdb::DatabaseInstance &db) {
    duckdb::DuckDB db_wrapper(db);
    db_wrapper.LoadExtension<duckdb::HttpserverExtension>();
}

DUCKDB_EXTENSION_API const char *httpserver_version() {
    return duckdb::DuckDB::LibraryVersion();
}
}
