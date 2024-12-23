#pragma once

#include "duckdb.hpp"

namespace duckdb {

class HttpserverExtension : public Extension {
public:
	void Load(DuckDB &db) override;
	std::string Name() override;
        std::string Version() const override;
};

// Static server state declarations
struct HttpServerState;
void HttpServerStart(DatabaseInstance& db, string_t host, int32_t port);
void HttpServerStop();

} // namespace duckdb
