#pragma once

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

void CheckAuthentication(const duckdb_httplib_openssl::Request& req);

} // namespace duckdb_httpserver
