#include "httpserver_extension/http_handler/authentication.hpp"
#include "httpserver_extension/http_handler/bindings.hpp"
#include "httpserver_extension/http_handler/common.hpp"
#include "httpserver_extension/http_handler/handler.hpp"
#include "httpserver_extension/http_handler/playground.hpp"
#include "httpserver_extension/http_handler/response_serializer.hpp"
#include "httpserver_extension/state.hpp"
#include "duckdb.hpp"
#include "yyjson.hpp"

#include <string>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

using namespace duckdb;
using namespace duckdb_yyjson;

namespace duckdb_httpserver {

// Handle both GET and POST requests
void HttpHandler(const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res) {
    try {
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

        CheckAuthentication(req);

		auto queryApiParameters = ExtractQueryApiParameters(req);

		if (!queryApiParameters.sqlQueryOpt.has_value()) {
			res.status = 200;
			res.set_content(reinterpret_cast<char const*>(playgroundContent), sizeof(playgroundContent), "text/html");
			return;
		}

        if (!global_state.db_instance) {
            throw IOException("Database instance not initialized");
        }

        auto start = std::chrono::system_clock::now();
        auto result = ExecuteQuery(req, queryApiParameters);
        auto end = std::chrono::system_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        QueryExecStats stats {
            static_cast<float>(elapsed.count()) / 1000,
            0,
            0
        };

		// Format output
		if (queryApiParameters.outputFormat == OutputFormat::Ndjson) {
			std::string json_output = ConvertResultToNDJSON(*result);
        	res.set_content(json_output, "application/x-ndjson");
		}
		else {
			auto json_output = ConvertResultToJSON(*result, stats);
        	res.set_content(json_output, "application/json");
		}
    }
    catch (const HttpHandlerException& ex) {
        res.status = ex.status;
        res.set_content(ex.message, "text/plain");
    }
    catch (const std::exception& ex) {
        res.status = 500;
        std::string error_message = "Code: 59, e.displayText() = DB::Exception: " + std::string(ex.what());
        res.set_content(error_message, "text/plain");
    }
}

// Execute query (optionally using a prepared statement)
std::unique_ptr<MaterializedQueryResult> ExecuteQuery(
	const duckdb_httplib_openssl::Request& req,
	const QueryApiParameters& queryApiParameters
) {
	Connection con(*global_state.db_instance);
	std::unique_ptr<MaterializedQueryResult> result;
	auto query = queryApiParameters.sqlQueryOpt.value();

	auto use_prepared_stmt =
		queryApiParameters.sqlParametersOpt.has_value() &&
		queryApiParameters.sqlParametersOpt.value().empty() == false;

    if (use_prepared_stmt) {
    	auto prepared_stmt = con.Prepare(query);
		if (prepared_stmt->HasError()) {
			throw HttpHandlerException(500, prepared_stmt->GetError());
		}

		auto named_values = queryApiParameters.sqlParametersOpt.value();

		auto prepared_stmt_result = prepared_stmt->Execute(named_values);
		D_ASSERT(prepared_stmt_result->type == QueryResultType::STREAM_RESULT);
		result = unique_ptr_cast<QueryResult, StreamQueryResult>(std::move(prepared_stmt_result))->Materialize();
    } else {
		result = con.Query(query);
    }

	if (result->HasError()) {
		throw HttpHandlerException(500, result->GetError());
	}

	return result;
}

QueryApiParameters ExtractQueryApiParameters(const duckdb_httplib_openssl::Request& req) {
	if (req.method == "POST" && req.has_header("Content-Type") && req.get_header_value("Content-Type") == "application/json") {
		return ExtractQueryApiParametersComplex(req);
	}
	else {
		return QueryApiParameters {
			ExtractSqlQuerySimple(req),
			std::nullopt,
            ExtractOutputFormatSimple(req),
		};
	}
}

std::optional<std::string> ExtractSqlQuerySimple(const duckdb_httplib_openssl::Request& req) {
    // Check if the query is in the URL parameters
    if (req.has_param("query")) {
        return req.get_param_value("query");
    }
    else if (req.has_param("q")) {
        return req.get_param_value("q");
    }

    // If not in URL, and it's a POST request, check the body
    else if (req.method == "POST" && !req.body.empty()) {
        return req.body;
    }

    return std::nullopt;
}

OutputFormat ExtractOutputFormatSimple(const duckdb_httplib_openssl::Request& req) {
    // Check for format in URL parameter or header
    if (req.has_param("default_format")) {
        return ParseOutputFormat(req.get_param_value("default_format"));
    }
    else if (req.has_header("X-ClickHouse-Format")) {
        return ParseOutputFormat(req.get_header_value("X-ClickHouse-Format"));
    }
    else if (req.has_header("format")) {
        return ParseOutputFormat(req.get_header_value("format"));
    }
    else {
    	return OutputFormat::Ndjson;
    }
}

OutputFormat ParseOutputFormat(const std::string& formatStr) {
    if (formatStr == "JSONEachRow" || formatStr == "ndjson" || formatStr == "jsonl") {
    	return OutputFormat::Ndjson;
    }
    else if (formatStr == "JSONCompact") {
    	return OutputFormat::Json;
    }
    else {
    	throw HttpHandlerException(400, "Unknown format");
    }
}

QueryApiParameters ExtractQueryApiParametersComplex(const duckdb_httplib_openssl::Request& req) {
	yyjson_doc *bodyDoc = nullptr;

	try {
		auto bodyJson = req.body;
		auto bodyJsonCStr = bodyJson.c_str();
		bodyDoc = yyjson_read(bodyJsonCStr, strlen(bodyJsonCStr), 0);

		return ExtractQueryApiParametersComplexImpl(bodyDoc);
	}
	catch (const std::exception& exception) {
		yyjson_doc_free(bodyDoc);
		throw;
	}
}

QueryApiParameters ExtractQueryApiParametersComplexImpl(yyjson_doc* bodyDoc) {
	if (!bodyDoc) {
		throw HttpHandlerException(400, "Unable to parse the request body");
	}

    auto bodyRoot = yyjson_doc_get_root(bodyDoc);
    if (!yyjson_is_obj(bodyRoot)) {
    	throw HttpHandlerException(400, "The request body must be an object");
    }

	auto queryVal = yyjson_obj_get(bodyRoot, "query");
	if (!queryVal || !yyjson_is_str(queryVal)) {
		throw HttpHandlerException(400, "The `query` field must be a string");
	}

	auto formatVal = yyjson_obj_get(bodyRoot, "format");
	if (!formatVal || !yyjson_is_str(formatVal)) {
		throw HttpHandlerException(400, "The `format` field must be a string");
	}

	return QueryApiParameters {
		std::string(yyjson_get_str(queryVal)),
		ExtractQueryParameters(yyjson_obj_get(bodyRoot, "parameters")),
		ParseOutputFormat(std::string(yyjson_get_str(formatVal))),
	};
}

} // namespace duckdb_httpserver
