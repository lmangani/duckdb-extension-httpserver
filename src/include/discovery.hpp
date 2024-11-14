#pragma once

#include "duckdb.hpp"
#include "duckdb/common/types.hpp"
#include <memory>
#include <string>

namespace duckdb {

using hash_bytes = uint8_t[32];

struct PeerData {
    std::string name;
    std::string endpoint;
    std::string sourceAddress;
    int64_t ttl;
    std::string metadata;
};

class PeerDiscovery {
    static std::unique_ptr<PeerDiscovery> instance;
    DatabaseInstance& db;

    void initTables();
    PeerDiscovery(DatabaseInstance& database);

public:
    static void Initialize(DatabaseInstance& db);
    static PeerDiscovery& Instance() { return *instance; }
    static std::string generateDeterministicId(const std::string& name, const std::string& endpoint);
    
    void registerPeer(const std::string& hash, const PeerData& data);
    std::unique_ptr<MaterializedQueryResult> getPeers(const std::string& hash, bool ndjson = false);
    void removePeer(const std::string& hash, const std::string& peerId);
    void updateHeartbeat(const std::string& hash, const std::string& peerId);
    void cleanupExpired();
};

} // namespace duckdb
