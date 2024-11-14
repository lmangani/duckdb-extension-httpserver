#include "discovery.hpp"
#include "mbedtls_wrapper.hpp"
#include "duckdb/common/common.hpp"
#include <chrono>
#include <iostream>

namespace duckdb {

// Static member definition
std::unique_ptr<PeerDiscovery> PeerDiscovery::instance = nullptr;

// Constructor implementation
PeerDiscovery::PeerDiscovery(DatabaseInstance& database) : db(database) {
    initTables();
}

void PeerDiscovery::Initialize(DatabaseInstance& db) {
    instance = std::unique_ptr<PeerDiscovery>(new PeerDiscovery(db));
}

void PeerDiscovery::initTables() {
    Connection conn(db);
    conn.Query("CREATE TABLE IF NOT EXISTS peers ("
               "hash VARCHAR, peer_id VARCHAR, name VARCHAR, endpoint VARCHAR,"
               "source_address VARCHAR, ttl BIGINT, metadata VARCHAR,"
               "registered_at TIMESTAMP, PRIMARY KEY (hash, peer_id))");
    
    conn.Query("CREATE INDEX IF NOT EXISTS idx_peers_ttl ON peers(registered_at, ttl)");
}

std::string PeerDiscovery::generateDeterministicId(const std::string& name, const std::string& endpoint) {
    std::string combined = name + ":" + endpoint;
    hash_bytes hash;
    duckdb_mbedtls::MbedTlsWrapper::ComputeSha256Hash(combined.c_str(), combined.length(), (char*)hash);
    
    std::string result;
    for (int i = 0; i < 16; i++) {
        char buf[3];
        snprintf(buf, sizeof(buf), "%02x", hash[i]);
        result += buf;
    }
    return result;
}


void PeerDiscovery::registerPeer(const std::string& hash, const PeerData& data) {
   Connection conn(db);
   std::string peerId = generateDeterministicId(data.name, data.endpoint);
   
   auto stmt = conn.Prepare(
       "INSERT INTO peers (hash, peer_id, name, endpoint, source_address, ttl, metadata, registered_at) "
       "VALUES ($1, $2, $3, $4, $5, $6, $7, CURRENT_TIMESTAMP)");
   
   vector<Value> params;
   params.push_back(Value(hash));
   params.push_back(Value(peerId));
   params.push_back(Value(data.name));
   params.push_back(Value(data.endpoint));
   params.push_back(Value(data.sourceAddress));
   params.push_back(Value::BIGINT(data.ttl));
   params.push_back(Value(data.metadata));
   
   stmt->Execute(params);
}

std::unique_ptr<MaterializedQueryResult> PeerDiscovery::getPeers(const std::string& hash, bool ndjson) {
    Connection conn(db);

    // Prepare the statement
    auto stmt = conn.Prepare(
       "SELECT name, endpoint, source_address AS sourceAddress, "
       "peer_id AS peerId, metadata, ttl, "
       "strftime(registered_at, '%Y-%m-%d %H:%M:%S') AS registered_at "
       "FROM peers WHERE hash = $1");

    // Execute the statement with the parameter
    vector<Value> params = {Value(hash)};
    auto result = stmt->Execute(params);

    auto materialized_result = dynamic_cast<MaterializedQueryResult*>(result.get());
    if (!materialized_result) {
        throw std::runtime_error("Failed to retrieve materialized result.");
    }

    return std::unique_ptr<MaterializedQueryResult>(
        static_cast<MaterializedQueryResult*>(result.release()));
}

void PeerDiscovery::removePeer(const std::string& hash, const std::string& peerId) {
    Connection conn(db);
    auto stmt = conn.Prepare("DELETE FROM peers WHERE hash = $1 AND peer_id = $2");
    stmt->Execute(hash, peerId);
}

void PeerDiscovery::updateHeartbeat(const std::string& hash, const std::string& peerId) {
    Connection conn(db);
    auto stmt = conn.Prepare("UPDATE peers SET registered_at = CURRENT_TIMESTAMP "
                           "WHERE hash = $1 AND peer_id = $2");
    stmt->Execute(hash, peerId);
}

void PeerDiscovery::cleanupExpired() {
    Connection conn(db);
    conn.Query("DELETE FROM peers WHERE EXTRACT(EPOCH FROM "
               "(CURRENT_TIMESTAMP - registered_at)) >= ttl");
}

} // namespace duckdb
