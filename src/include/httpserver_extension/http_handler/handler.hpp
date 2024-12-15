#pragma once

#include "duckdb.hpp"
#include "yyjson.hpp"
#include <optional>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

enum class OutputFormat {
	Ndjson,
	Json,
};

struct QueryApiParameters {
	std::optional<std::string> sqlQueryOpt;
	std::optional<duckdb::case_insensitive_map_t<duckdb::BoundParameterData>> sqlParametersOpt;
	OutputFormat outputFormat;
};

std::unique_ptr<duckdb::MaterializedQueryResult> ExecuteQuery(
	const duckdb_httplib_openssl::Request& req,
	const QueryApiParameters& queryApiParameters
);

QueryApiParameters ExtractQueryApiParameters(const duckdb_httplib_openssl::Request& req);

QueryApiParameters ExtractQueryApiParametersComplex(const duckdb_httplib_openssl::Request& req);

QueryApiParameters ExtractQueryApiParametersComplexImpl(duckdb_yyjson::yyjson_doc* bodyDoc);

std::optional<std::string> ExtractSqlQuerySimple(const duckdb_httplib_openssl::Request& req);

OutputFormat ExtractOutputFormatSimple(const duckdb_httplib_openssl::Request& req);

OutputFormat ParseOutputFormat(const std::string& formatStr);

} // namespace duckdb_httpserver
