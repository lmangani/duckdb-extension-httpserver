#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

// HTTP handler
void HttpHandler(const duckdb_httplib_openssl::Request& req, duckdb_httplib_openssl::Response& res);

} // namespace duckdb_httpserver
