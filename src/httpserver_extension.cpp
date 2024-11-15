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
#include <chrono>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

// Include yyjson for JSON handling
#include "yyjson.hpp"

#include <thread>
#include <memory>
#include <cstdlib>
#include "play.h"

using namespace duckdb_yyjson; // NOLINT

namespace duckdb {

struct HttpServerState {
    std::unique_ptr<duckdb_httplib_openssl::Server> server;
    std::unique_ptr<std::thread> server_thread;
    std::atomic<bool> is_running;
    DatabaseInstance* db_instance;
    unique_ptr<Allocator> allocator;
    std::string auth_token;

    HttpServerState() : is_running(false), db_instance(nullptr) {}
};

static HttpServerState global_state;

    std::string GetColumnType(MaterializedQueryResult &result, idx_t column) {
        if (result.RowCount() == 0) {
            return "String";
        }
        switch (result.types[column].id()) {
            case LogicalTypeId::FLOAT:
                return "Float";
            case LogicalTypeId::DOUBLE:
                return "Double";
            case LogicalTypeId::INTEGER:
                return "Int32";
            case LogicalTypeId::BIGINT:
                return "Int64";
            case LogicalTypeId::UINTEGER:
                return "UInt32";
            case LogicalTypeId::UBIGINT:
                return "UInt64";
            case LogicalTypeId::VARCHAR:
                return "String";
            case LogicalTypeId::TIME:
                return "DateTime";
            case LogicalTypeId::DATE:
                return "Date";
            case LogicalTypeId::TIMESTAMP:
                return "DateTime";
            case LogicalTypeId::BOOLEAN:
                return "Int8";
            default:
                return "String";
        }
        return "String";
    }

    struct ReqStats {
        float elapsed_sec;
        int64_t read_bytes;
        int64_t read_rows;
    };



// Convert the query result to JSON format
static std::string ConvertResultToJSON(MaterializedQueryResult &result, ReqStats &req_stats) {
    auto doc = yyjson_mut_doc_new(nullptr);
    auto root = yyjson_mut_obj(doc);
    yyjson_mut_doc_set_root(doc, root);
    // Add meta information
    auto meta_array = yyjson_mut_arr(doc);
    for (idx_t col = 0; col < result.ColumnCount(); ++col) {
        auto column_obj = yyjson_mut_obj(doc);
        yyjson_mut_obj_add_str(doc, column_obj, "name", result.ColumnName(col).c_str());
        yyjson_mut_arr_append(meta_array, column_obj);
        std::string tp(GetColumnType(result, col));
        yyjson_mut_obj_add_strcpy(doc, column_obj, "type", tp.c_str());
    }
    yyjson_mut_obj_add_val(doc, root, "meta", meta_array);

    // Add data
    auto data_array = yyjson_mut_arr(doc);
    for (idx_t row = 0; row < result.RowCount(); ++row) {
        auto row_array = yyjson_mut_arr(doc);
        for (idx_t col = 0; col < result.ColumnCount(); ++col) {
            Value value = result.GetValue(col, row);
            if (value.IsNull()) {
                yyjson_mut_arr_append(row_array, yyjson_mut_null(doc));
            } else {
                std::string value_str = value.ToString();
                yyjson_mut_arr_append(row_array, yyjson_mut_strncpy(doc, value_str.c_str(), value_str.length()));
            }
        }
        yyjson_mut_arr_append(data_array, row_array);
    }
    yyjson_mut_obj_add_val(doc, root, "data", data_array);

    // Add row count
    yyjson_mut_obj_add_int(doc, root, "rows", result.RowCount());
    //"statistics":{"elapsed":0.00031403,"rows_read":1,"bytes_read":0}}
    auto stat_obj = yyjson_mut_obj_add_obj(doc, root, "statistics");
    yyjson_mut_obj_add_real(doc, stat_obj, "elapsed", req_stats.elapsed_sec);
    yyjson_mut_obj_add_int(doc, stat_obj, "rows_read", req_stats.read_rows);
    yyjson_mut_obj_add_int(doc, stat_obj, "bytes_read", req_stats.read_bytes);
    // Write to string
    auto data = yyjson_mut_write(doc, 0, nullptr);
    if (!data) {
        yyjson_mut_doc_free(doc);
        throw InternalException("Failed to render the result as JSON, yyjson failed");
    }

    std::string json_output(data);
    free(data);
    yyjson_mut_doc_free(doc);
    return json_output;
}

// New: Base64 decoding function
std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// Auth Check
bool IsAuthenticated(const duckdb_httplib_openssl::Request& req) {
    if (global_state.auth_token.empty()) {
        return true; // No authentication required if no token is set
    }

    // Check for X-API-Key header
    auto api_key = req.get_header_value("X-API-Key");
    if (!api_key.empty() && api_key == global_state.auth_token) {
        return true;
    }

    // Check for Basic Auth
    auto auth = req.get_header_value("Authorization");
    if (!auth.empty() && auth.compare(0, 6, "Basic ") == 0) {
        std::string decoded_auth = base64_decode(auth.substr(6));
        if (decoded_auth == global_state.auth_token) {
            return true;
        }
    }

    return false;
}


// Convert the query result to NDJSON (JSONEachRow) format
static std::string ConvertResultToNDJSON(MaterializedQueryResult &result) {
    std::string ndjson_output;

    for (idx_t row = 0; row < result.RowCount(); ++row) {
        // Create a new JSON document for each row
        auto doc = yyjson_mut_doc_new(nullptr);
        auto root = yyjson_mut_obj(doc);
        yyjson_mut_doc_set_root(doc, root);

        for (idx_t col = 0; col < result.ColumnCount(); ++col) {
            Value value = result.GetValue(col, row);
            const char* column_name = result.ColumnName(col).c_str();

            // Handle null values and add them to the JSON object
            if (value.IsNull()) {
                yyjson_mut_obj_add_null(doc, root, column_name);
            } else {
                // Convert value to string and add it to the JSON object
                std::string value_str = value.ToString();
                yyjson_mut_obj_add_strncpy(doc, root, column_name, value_str.c_str(), value_str.length());
            }
        }

        char *json_line = yyjson_mut_write(doc, 0, nullptr);
        if (!json_line) {
            yyjson_mut_doc_free(doc);
            throw InternalException("Failed to render a row as JSON, yyjson failed");
        }

        ndjson_output += json_line;
        ndjson_output += "\n";

        // Free allocated memory for this row
        free(json_line);
        yyjson_mut_doc_free(doc);
    }

    return ndjson_output;
}

static void HandleQuery(const string& query, duckdb_httplib_openssl::Response& res) {
    try {
        if (!global_state.db_instance) {
            throw IOException("Database instance not initialized");
        }

        Connection con(*global_state.db_instance);
        const auto& start = std::chrono::system_clock::now();
        auto result = con.Query(query);
        const auto end = std::chrono::system_clock::now();

        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        ReqStats req_stats{
            static_cast<float>(elapsed.count()) / 1000,
            0,
            0
        };

        if (result->HasError()) {
            res.status = 400;
            res.set_content(result->GetError(), "text/plain");
            return;
        }

        // Convert result to JSON
        std::string json_output = ConvertResultToJSON(*result, req_stats);
        res.set_content(json_output, "application/json");
    } catch (const Exception& ex) {
        res.status = 400;
        res.set_content(ex.what(), "text/plain");
    }
}


// Handle both GET and POST requests
void HandleHttpRequest(const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
    std::string query;

    // Check authentication
    if (!IsAuthenticated(req)) {
        res.status = 401;
        res.set_content("Unauthorized", "text/plain");
        return;
    }

    // CORS allow
    res.set_header("Access-Control-Allow-Origin", "*");
    res.set_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS, PUT");
    res.set_header("Access-Control-Allow-Headers", "*");
    res.set_header("Access-Control-Allow-Credentials", "true");
    res.set_header("Access-Control-Max-Age", "86400");

    // Handle preflight OPTIONS request
    if (req.method == "OPTIONS") {
        res.status = 204;  // No content
        return;
    }

    // Check if the query is in the URL parameters
    if (req.has_param("query")) {
        query = req.get_param_value("query");
    }
    else if (req.has_param("q")) {
        query = req.get_param_value("q");
    }
    // If not in URL, and it's a POST request, check the body
    else if (req.method == "POST" && !req.body.empty()) {
        query = req.body;
    }
    // If no query found, return an error
    else {
        res.status = 200;
        res.set_content(playContent, "text/html");
        return;
    }

    // Set default format to JSONCompact
    std::string format = "JSONEachRow";

    // Check for format in URL parameter or header
    if (req.has_param("default_format")) {
        format = req.get_param_value("default_format");
    } else if (req.has_header("X-ClickHouse-Format")) {
        format = req.get_header_value("X-ClickHouse-Format");
    } else if (req.has_header("format")) {
        format = req.get_header_value("format");
    }

    try {
        if (!global_state.db_instance) {
            throw IOException("Database instance not initialized");
        }

        Connection con(*global_state.db_instance);
        auto start = std::chrono::system_clock::now();
        auto result = con.Query(query);
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        if (result->HasError()) {
            res.status = 500;
            res.set_content(result->GetError(), "text/plain");
            return;
        }


        ReqStats stats{
            static_cast<float>(elapsed.count()) / 1000,
            0,
            0
        };

        // Format Options
        if (format == "JSONEachRow") {
            std::string json_output = ConvertResultToNDJSON(*result);
            res.set_content(json_output, "application/x-ndjson");
        } else if (format == "JSONCompact") {
            std::string json_output = ConvertResultToJSON(*result, stats);
            res.set_content(json_output, "application/json");
        } else {
            // Default to NDJSON for DuckDB's own queries
            std::string json_output = ConvertResultToNDJSON(*result);
            res.set_content(json_output, "application/x-ndjson");
        }
        
    } catch (const Exception& ex) {
        res.status = 500;
        std::string error_message = "Code: 59, e.displayText() = DB::Exception: " + std::string(ex.what());
        res.set_content(error_message, "text/plain");
    }
}

void HttpServerStart(DatabaseInstance& db, string_t host, int32_t port, string_t auth = string_t()) {
    if (global_state.is_running) {
        throw IOException("HTTP server is already running");
    }

    global_state.db_instance = &db;
    global_state.server = make_uniq<duckdb_httplib_openssl::Server>();
    global_state.is_running = true;
    global_state.auth_token = auth.GetString();

    // CORS Preflight
    global_state.server->Options("/",
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
    global_state.server->Get("/", HandleHttpRequest);
    global_state.server->Post("/", HandleHttpRequest);

    // Health check endpoint
    global_state.server->Get("/ping", [](const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
        res.set_content("OK", "text/plain");
    });

    string host_str = host.GetString();

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
    ExtensionUtil::RegisterFunction(instance, DuckFlockTableFunction());
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
