// APBTicket.cpp — C4 implementation
#include "APBTicket.h"
#include <chrono>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <limits>
#include <optional>

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
        if (c == '=') continue; // padding
        int v = val(c);
        if (v == -1) throw std::invalid_argument("invalid b64url character");
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(char(buf >> bits));
        }
    }
    return out;
}

static std::string escape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 2);
    for (char c : s) {
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else out += c;
    }
    return out;
}

static std::string unescape_json(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    bool escape = false;
    for (char c : s) {
        if (escape) {
            if (c == '"') out += '"';
            else if (c == '\\') out += '\\';
            else if (c == 'n') out += '\n';
            else if (c == 'r') out += '\r';
            else if (c == 't') out += '\t';
            else out += c;
            escape = false;
        } else {
            if (c == '\\') escape = true;
            else out += c;
        }
    }
    return out;
}

std::string TicketService::build_payload(const TicketClaims& c) {
    std::ostringstream o;
    o << "{\"account\":\"" << escape_json(c.account) << "\""
      << ",\"character\":\"" << escape_json(c.character) << "\""
      << ",\"faction\":\"" << escape_json(c.faction) << "\""
      << ",\"district\":\"" << escape_json(c.district) << "\""
      << ",\"jti\":\"" << escape_json(c.jti) << "\""
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
    auto end = pos;
    while (end < json.size()) {
        if (json[end] == '\\') {
            end += 2;
        } else if (json[end] == '"') {
            break;
        } else {
            end++;
        }
    }
    if (end >= json.size() || json[end] != '"') return {};
    return unescape_json(json.substr(pos, end - pos));
}

static std::optional<int64_t> json_int(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return std::nullopt;
    pos += needle.size();
    
    // Skip any whitespace after colon
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        pos++;
    }
    
    if (pos >= json.size()) return std::nullopt;
    
    try {
        size_t parsed_len = 0;
        int64_t result = std::stoll(json.substr(pos), &parsed_len, 10);
        if (parsed_len == 0) return std::nullopt;
        return result;
    } catch (const std::invalid_argument&) {
        return std::nullopt;
    } catch (const std::out_of_range&) {
        return std::nullopt;
    }
}

bool TicketService::parse_payload(const std::string& json, TicketClaims& out) {
    TicketClaims parsed;
    parsed.account   = json_str(json, "account");
    parsed.character = json_str(json, "character");
    parsed.faction   = json_str(json, "faction");
    parsed.district  = json_str(json, "district");
    parsed.jti       = json_str(json, "jti");
    const auto issued = json_int(json, "issued");
    const auto expiry = json_int(json, "expiry");
    if (!issued || !expiry ||
        *expiry < std::numeric_limits<int32_t>::min() ||
        *expiry > std::numeric_limits<int32_t>::max() ||
        parsed.account.empty() || parsed.jti.empty()) {
        return false;
    }
    parsed.issued_utc = *issued;
    parsed.expiry_secs = static_cast<int32_t>(*expiry);
    out = std::move(parsed);
    return true;
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
