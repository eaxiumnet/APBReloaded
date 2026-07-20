#pragma once
// APBTicket.h — C4: HMAC-SHA256 ticket service (pure C++17, no platform headers)
// Format: base64url(payload_json).hmac_sha256_hex
// Payload fields: account, character, faction, district, jti, issued_utc, expiry_secs
#include "APBCrypto.h"
#include <string>
#include <unordered_set>
#include <chrono>
#include <sstream>
#include <algorithm>

namespace apb {

struct TicketClaims {
    std::string account;
    std::string character;
    std::string faction;
    std::string district;
    std::string jti;          // filled on issue; must be present on verify
    int64_t     issued_utc   = 0;
    int32_t     expiry_secs  = 90;
};

class TicketService {
public:
    static TicketService& Global() {
        static TicketService inst;
        return inst;
    }

    // Issue a compact "payload.sig" token.
    std::string IssueTicket(const TicketClaims& req);

    // Verify signature + expiry + jti-not-yet-consumed. Fills 'out' on success.
    bool VerifyTicket(const std::string& token, TicketClaims& out) const;

    // One-use: marks jti consumed. Returns false if already consumed or unknown.
    bool ConsumeJti(const std::string& jti);

    // Replace the HMAC key (for testing). Default = random at construction.
    void SetSecret(const std::string& hex_secret) { secret_hex_ = hex_secret; }

private:
    TicketService();

    std::string secret_hex_;
    std::unordered_set<std::string> consumed_jtis_;

    // helpers
    static std::string b64url_encode(const std::string& s);
    static std::string b64url_decode(const std::string& s);
    static std::string build_payload(const TicketClaims& c);
    static bool        parse_payload(const std::string& json, TicketClaims& out);
    std::string        sign(const std::string& payload) const;
    bool               verify_sig(const std::string& payload,
                                  const std::string& sig) const;
};

} // namespace apb
