#pragma once

#include <cstdint>
#include <exception>
#include <string>

namespace duckdb_httpserver {

// Used to have an easy to read control flow
struct HttpHandlerException: public std::exception {
	int status;
	std::string message;

	HttpHandlerException(int status, const std::string& message) : message(message), status(status) {}
};

// Statistics associated to the SQL query execution
struct QueryExecStats {
	float elapsed_sec;
	int64_t read_bytes;
	int64_t read_rows;
};

} // namespace duckdb_httpserver
