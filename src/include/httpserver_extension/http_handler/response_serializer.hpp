#pragma once

#include "httpserver_extension/http_handler/common.hpp"
#include "duckdb.hpp"

namespace duckdb_httpserver {

std::string ConvertResultToJSON(duckdb::MaterializedQueryResult& result, QueryExecStats& stats);
std::string ConvertResultToNDJSON(duckdb::MaterializedQueryResult& result);

} // namespace duckdb_httpserver
