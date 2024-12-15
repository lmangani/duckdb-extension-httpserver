#pragma once

#include "duckdb.hpp"
#include "yyjson.hpp"

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

duckdb::case_insensitive_map_t<duckdb::BoundParameterData> ExtractQueryParameters(
	duckdb_yyjson::yyjson_val* parametersVal
);

} // namespace duckdb_httpserver
