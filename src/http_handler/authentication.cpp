#include "httpserver_extension/http_handler/common.hpp"
#include "httpserver_extension/state.hpp"
#include <string>
#include <vector>

#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.hpp"

namespace duckdb_httpserver {

// Base64 decoding function
static std::string base64_decode(const std::string &in) {
    std::string out;
    std::vector<int> T(256, -1);
    for (int i = 0; i < 64; i++)
        T["ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;

    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

// Check authentication
void CheckAuthentication(const duckdb_httplib_openssl::Request& req) {
    if (global_state.auth_token.empty()) {
        return; // No authentication required if no token is set
    }

    // Check for X-API-Key header
    auto api_key = req.get_header_value("X-API-Key");
    if (!api_key.empty() && api_key == global_state.auth_token) {
        return;
    }

    // Check for Basic Auth
    auto auth = req.get_header_value("Authorization");
    if (!auth.empty() && auth.compare(0, 6, "Basic ") == 0) {
        std::string decoded_auth = base64_decode(auth.substr(6));
        if (decoded_auth == global_state.auth_token) {
            return;
        }
    }

    throw HttpHandlerException(401, "Unauthorized");
}

} // namespace duckdb_httpserver
