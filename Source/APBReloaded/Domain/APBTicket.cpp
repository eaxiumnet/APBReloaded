// APBTicket.cpp — C4 implementation
#include "APBTicket.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstring>

namespace apb {

static const char kB64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

std::string TicketService::b64url_encode(const std::string& s) {
    std::string out;
    out.reserve((s.size() * 4 + 2) / 3);
    unsigned buf = 0; int bits = 0;
    for (unsigned char c : s) {
        buf = (buf << 8) | c; bits += 8;
        while (bits >= 6) { bits -= 6; out += kB64Chars[(buf >> bits) & 0x3f]; }
    }
    if (bits > 0) out += kB64Chars[(buf << (6 - bits)) & 0x3f];
    return out;
}

std::string TicketService::b64url_decode(const std::string& s) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        return -1;
    };
    std::string out;
    unsigned buf = 0; int bits = 0;
    for (char c : s) {
        int v = val(c); if (v < 0) continue;
        buf = (buf << 6) | unsigned(v); bits += 6;
        if (bits >= 8) { bits -= 8; out += char((buf >> bits) & 0xff); }
    }
    return out;
}

std::string TicketService::build_payload(const TicketClaims& c) {
    std::ostringstream o;
    o << "{\"account\":\"" << c.account << "\""
      << ",\"character\":\"" << c.character << "\""
      << ",\"faction\":\"" << c.faction << "\""
      << ",\"district\":\"" << c.district << "\""
      << ",\"jti\":\"" << c.jti << "\""
      << ",\"issued\":" << c.issued_utc
      << ",\"expiry\":" << c.expiry_secs
      << "}";
    return o.str();
}

static std::string json_str(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return {};
    pos += needle.size();
    auto end = json.find('"', pos);
    return (end == std::string::npos) ? std::string{} : json.substr(pos, end - pos);
}

static int64_t json_int(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos += needle.size();
    return std::stoll(json.substr(pos));
}

bool TicketService::parse_payload(const std::string& json, TicketClaims& out) {
    out.account     = json_str(json, "account");
    out.character   = json_str(json, "character");
    out.faction     = json_str(json, "faction");
    out.district    = json_str(json, "district");
    out.jti         = json_str(json, "jti");
    out.issued_utc  = json_int(json, "issued");
    out.expiry_secs = int32_t(json_int(json, "expiry"));
    return !out.account.empty() && !out.jti.empty();
}

std::string TicketService::sign(const std::string& payload) const {
    auto key = hex_decode(secret_hex_);
    return hmac_sha256_hex(key.data(), key.size(),
        reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
}

bool TicketService::verify_sig(const std::string& payload,
                               const std::string& sig) const {
    std::string expected = sign(payload);
    if (expected.size() != sig.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < expected.size(); ++i)
        diff |= (unsigned char)(expected[i] ^ sig[i]);
    return diff == 0;
}

TicketService::TicketService() {
    secret_hex_ = random_hex(32);
}

std::string TicketService::IssueTicket(const TicketClaims& req) {
    TicketClaims c = req;
    c.jti = random_hex(16);
    c.issued_utc = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    std::string payload_json = build_payload(c);
    std::string payload_enc  = b64url_encode(payload_json);
    std::string sig          = sign(payload_enc);
    return payload_enc + "." + sig;
}

bool TicketService::VerifyTicket(const std::string& token, TicketClaims& out) const {
    auto dot = token.rfind('.');
    if (dot == std::string::npos) return false;
    std::string payload_enc  = token.substr(0, dot);
    std::string sig          = token.substr(dot + 1);
    if (!verify_sig(payload_enc, sig)) return false;
    std::string payload_json = b64url_decode(payload_enc);
    if (!parse_payload(payload_json, out)) return false;
    int64_t now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    if (now > out.issued_utc + out.expiry_secs) return false;
    if (consumed_jtis_.count(out.jti)) return false;
    return true;
}

bool TicketService::ConsumeJti(const std::string& jti) {
    if (jti.empty() || consumed_jtis_.count(jti)) return false;
    consumed_jtis_.insert(jti);
    return true;
}

} // namespace apb
