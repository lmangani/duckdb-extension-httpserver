#pragma once

#include "duckdb.hpp"
#include "duckdb/common/allocator.hpp"
#include <chrono>
#include <thread>
#include <memory>
#include <cstdlib>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

struct State {
    std::unique_ptr<duckdb_httplib_openssl::Server> server;
    std::unique_ptr<std::thread> server_thread;
    std::atomic<bool> is_running;
    duckdb::DatabaseInstance* db_instance;
    std::unique_ptr<duckdb::Allocator> allocator;
    std::string auth_token;

    State() : is_running(false), db_instance(nullptr) {}
};

extern State global_state;

} // namespace duckdb_httpserver
