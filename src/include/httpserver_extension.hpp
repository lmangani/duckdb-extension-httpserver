#pragma once

#include "duckdb.hpp"
#include "duckdb/common/file_system.hpp"

namespace duckdb {

struct HttpserverExtension: public Extension {
public:
	void Load(DuckDB &db) override;
	std::string Name() override;
	std::string Version() const override;
};

void HttpServerStart(DatabaseInstance& db, string_t host, int32_t port);
void HttpServerStop();

} // namespace duckdb
