#pragma once
#include <cstdint>

namespace duckdb {

struct ReqStats {
	float elapsed_sec;
	uint64_t read_bytes;
	uint64_t read_rows;
};

} // namespace duckdb
