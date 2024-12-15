#define DUCKDB_EXTENSION_MAIN
#include "httpserver_extension.hpp"
#include "httpserver_extension/http_handler.hpp"
#include "httpserver_extension/state.hpp"

#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/common/string_util.hpp"
#include "duckdb/function/scalar_function.hpp"
#include "duckdb/main/extension_util.hpp"
#include "duckdb/common/atomic.hpp"
#include "duckdb/common/exception/http_exception.hpp"
#include "duckdb/common/allocator.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <cstdlib>

#ifndef _WIN32
#include <syslog.h>
#endif

namespace duckdb_httpserver {
	duckdb_httpserver::State global_state;
}

using namespace duckdb_httpserver;

namespace duckdb {

void HttpServerStart(DatabaseInstance& db, string_t host, int32_t port, string_t auth = string_t()) {
    if (global_state.is_running) {
        throw IOException("HTTP server is already running");
    }

    global_state.db_instance = &db;
    global_state.server = make_uniq<duckdb_httplib_openssl::Server>();
    global_state.is_running = true;
    global_state.auth_token = auth.GetString();

    // Custom basepath, defaults to root /
    const char* base_path_env = std::getenv("DUCKDB_HTTPSERVER_BASEPATH");
    std::string base_path = "/";

    if (base_path_env && base_path_env[0] == '/' && strlen(base_path_env) > 1) {
        base_path = std::string(base_path_env);
    }

    // CORS Preflight
    global_state.server->Options(base_path,
    [](const duckdb_httplib_openssl::Request& /*req*/, duckdb_httplib_openssl::Response& res) {
        res.set_header("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
        res.set_header("Content-Type", "text/html; charset=utf-8");
        res.set_header("Access-Control-Allow-Headers", "*");
        res.set_header("Access-Control-Allow-Origin", "*");
        res.set_header("Access-Control-Allow-Credentials", "true");
        res.set_header("Connection", "close");
        return duckdb_httplib_openssl::Server::HandlerResponse::Handled;
    });

    // Create a new allocator for the server thread
    global_state.allocator = make_uniq<Allocator>();

    // Handle GET and POST requests
    global_state.server->Get(base_path, HttpHandler);
    global_state.server->Post(base_path, HttpHandler);

    // Health check endpoint
    global_state.server->Get("/ping", [](const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
        res.set_content("OK", "text/plain");
    });

    string host_str = host.GetString();


#ifndef _WIN32
    const char* debug_env = std::getenv("DUCKDB_HTTPSERVER_DEBUG");
    const char* use_syslog = std::getenv("DUCKDB_HTTPSERVER_SYSLOG");

    if (debug_env != nullptr && std::string(debug_env) == "1") {
        global_state.server->set_logger([](const duckdb_httplib_openssl::Request& req, const duckdb_httplib_openssl::Response& res) {
            time_t now_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            char timestr[32];
            strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now_time));
            // Use \r\n for consistent line endings
            fprintf(stdout, "[%s] %s %s - %d - from %s:%d\r\n",
                timestr,
                req.method.c_str(),
                req.path.c_str(),
                res.status,
                req.remote_addr.c_str(),
                req.remote_port);
            fflush(stdout);
        });
    } else if (use_syslog != nullptr && std::string(use_syslog) == "1") {
        openlog("duckdb-httpserver", LOG_PID | LOG_NDELAY, LOG_LOCAL0);
        global_state.server->set_logger([](const duckdb_httplib_openssl::Request& req, const duckdb_httplib_openssl::Response& res) {
            syslog(LOG_INFO, "%s %s - %d - from %s:%d",
                req.method.c_str(),
                req.path.c_str(),
                res.status,
                req.remote_addr.c_str(),
                req.remote_port);
        });
        std::atexit([]() {
            closelog();
        });
    }
#endif

    const char* run_in_same_thread_env = std::getenv("DUCKDB_HTTPSERVER_FOREGROUND");
    bool run_in_same_thread = (run_in_same_thread_env != nullptr && std::string(run_in_same_thread_env) == "1");

    if (run_in_same_thread) {
#ifdef _WIN32
        throw IOException("Foreground mode not yet supported on WIN32 platforms.");
#else
        // POSIX signal handler for SIGINT (Linux/macOS)
        signal(SIGINT, [](int) {
            if (global_state.server) {
                global_state.server->stop();
            }
            global_state.is_running = false; // Update the running state
        });

        // Run the server in the same thread
        if (!global_state.server->listen(host_str.c_str(), port)) {
            global_state.is_running = false;
            throw IOException("Failed to start HTTP server on " + host_str + ":" + std::to_string(port));
        }
#endif

        // The server has stopped (due to CTRL-C or other reasons)
        global_state.is_running = false;
    } else {
        // Run the server in a dedicated thread (default)
        global_state.server_thread = make_uniq<std::thread>([host_str, port]() {
            if (!global_state.server->listen(host_str.c_str(), port)) {
                global_state.is_running = false;
                throw IOException("Failed to start HTTP server on " + host_str + ":" + std::to_string(port));
            }
        });
    }
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
                                        {LogicalType::VARCHAR, LogicalType::INTEGER, LogicalType::VARCHAR},
                                        LogicalType::VARCHAR,
                                        [&](DataChunk &args, ExpressionState &state, Vector &result) {
        auto &host_vector = args.data[0];
        auto &port_vector = args.data[1];
        auto &auth_vector = args.data[2];

        UnaryExecutor::Execute<string_t, string_t>(
            host_vector, result, args.size(),
            [&](string_t host) {
                auto port = ((int32_t*)port_vector.GetData())[0];
                auto auth = ((string_t*)auth_vector.GetData())[0];
                HttpServerStart(instance, host, port, auth);
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
