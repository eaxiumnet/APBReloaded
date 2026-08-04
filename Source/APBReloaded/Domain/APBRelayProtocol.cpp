#include "APBRelayProtocol.h"
#include "APBCrypto.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace apb {
namespace {

enum class JsonType { String, Integer, Boolean };
struct JsonValue {
	JsonType type = JsonType::String;
	std::string text;
	int64_t integer = 0;
	bool boolean = false;
};

void SkipWs(const std::string& text, size_t& pos) {
	while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' ||
		text[pos] == '\r' || text[pos] == '\n')) ++pos;
}

bool ParseString(const std::string& text, size_t& pos, std::string& out) {
	if (pos >= text.size() || text[pos] != '"') return false;
	++pos;
	out.clear();
	while (pos < text.size()) {
		const unsigned char c = static_cast<unsigned char>(text[pos++]);
		if (c == '"') return true;
		if (c < 0x20u) return false;
		if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
		if (pos >= text.size()) return false;
		const char escaped = text[pos++];
		switch (escaped) {
			case '"': out.push_back('"'); break;
			case '\\': out.push_back('\\'); break;
			case '/': out.push_back('/'); break;
			case 'b': out.push_back('\b'); break;
			case 'f': out.push_back('\f'); break;
			case 'n': out.push_back('\n'); break;
			case 'r': out.push_back('\r'); break;
			case 't': out.push_back('\t'); break;
			default: return false;
		}
	}
	return false;
}

bool ParseInteger(const std::string& text, size_t& pos, int64_t& out) {
	const size_t start = pos;
	if (pos < text.size() && text[pos] == '-') ++pos;
	const size_t digits = pos;
	while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9') ++pos;
	if (digits == pos) return false;
	const std::string token = text.substr(start, pos - start);
	errno = 0;
	char* end = nullptr;
	const long long parsed = std::strtoll(token.c_str(), &end, 10);
	if (errno == ERANGE || end == nullptr || *end != '\0') return false;
	out = static_cast<int64_t>(parsed);
	return true;
}

bool ParseFlatJson(const std::string& text, std::unordered_map<std::string, JsonValue>& values) {
	values.clear();
	size_t pos = 0;
	SkipWs(text, pos);
	if (pos >= text.size() || text[pos++] != '{') return false;
	SkipWs(text, pos);
	if (pos < text.size() && text[pos] == '}') { ++pos; SkipWs(text, pos); return pos == text.size(); }
	while (pos < text.size()) {
		std::string key;
		if (!ParseString(text, pos, key)) return false;
		if (values.find(key) != values.end()) return false;
		SkipWs(text, pos);
		if (pos >= text.size() || text[pos++] != ':') return false;
		SkipWs(text, pos);
		JsonValue value;
		if (pos < text.size() && text[pos] == '"') {
			value.type = JsonType::String;
			if (!ParseString(text, pos, value.text)) return false;
		} else if (pos < text.size() && (text[pos] == '-' ||
			(text[pos] >= '0' && text[pos] <= '9'))) {
			value.type = JsonType::Integer;
			if (!ParseInteger(text, pos, value.integer)) return false;
		} else if (text.compare(pos, 4, "true") == 0) {
			value.type = JsonType::Boolean; value.boolean = true; pos += 4;
		} else if (text.compare(pos, 5, "false") == 0) {
			value.type = JsonType::Boolean; value.boolean = false; pos += 5;
		} else {
			return false;
		}
		values.emplace(std::move(key), std::move(value));
		SkipWs(text, pos);
		if (pos >= text.size()) return false;
		if (text[pos] == '}') { ++pos; SkipWs(text, pos); return pos == text.size(); }
		if (text[pos++] != ',') return false;
		SkipWs(text, pos);
	}
	return false;
}

bool GetString(const std::unordered_map<std::string, JsonValue>& values,
	const char* key, std::string& out, bool required = false) {
	auto it = values.find(key);
	if (it == values.end()) { out.clear(); return !required; }
	if (it->second.type != JsonType::String) return false;
	out = it->second.text;
	return true;
}

bool GetInteger(const std::unordered_map<std::string, JsonValue>& values,
	const char* key, int64_t& out, bool required = false) {
	auto it = values.find(key);
	if (it == values.end()) { out = 0; return !required; }
	if (it->second.type != JsonType::Integer) return false;
	out = it->second.integer;
	return true;
}

bool GetBoolean(const std::unordered_map<std::string, JsonValue>& values,
	const char* key, bool& out) {
	auto it = values.find(key);
	if (it == values.end()) { out = false; return true; }
	if (it->second.type != JsonType::Boolean) return false;
	out = it->second.boolean;
	return true;
}

std::string JsonEscape(const std::string& value) {
	std::string out;
	out.reserve(value.size());
	for (unsigned char c : value) {
		switch (c) {
			case '"': out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default: out.push_back(c < 0x20u ? ' ' : static_cast<char>(c)); break;
		}
	}
	return out;
}

std::string DefaultRequestId(RelayVerb verb, int32_t numeric_id, int64_t seq,
	const std::string& jti) {
	std::ostringstream out;
	out << RelayCodec::VerbToken(verb) << ':';
	if (!jti.empty()) out << jti;
	else out << numeric_id << ':' << seq;
	return out.str();
}

RelayMessage MakeEnvelope(RelayVerb verb, const std::string& request_id, int64_t sent_ms,
	const std::string& auth, int32_t numeric_id = 0, int64_t seq = 0,
	const std::string& jti = "") {
	RelayMessage message;
	message.verb = verb;
	message.request_id = request_id.empty() ? DefaultRequestId(verb, numeric_id, seq, jti) : request_id;
	message.sent_ms = sent_ms;
	message.auth = auth;
	return message;
}

}

const char* RelayCodec::VerbToken(RelayVerb verb) {
	switch (verb) {
		case RelayVerb::Register: return "register";
		case RelayVerb::RegisterAck: return "register_ack";
		case RelayVerb::Heartbeat: return "heartbeat";
		case RelayVerb::ReportLoad: return "report_load";
		case RelayVerb::ExpectTicket: return "expect";
		case RelayVerb::ExpectAck: return "expect_ack";
		case RelayVerb::ChatRelay: return "chat";
		case RelayVerb::PlayerJoined: return "join";
		case RelayVerb::PlayerLeft: return "leave";
		case RelayVerb::Handoff: return "handoff";
		case RelayVerb::Return: return "return";
		case RelayVerb::SocialRequest: return "social_req";
		case RelayVerb::SocialResult: return "social_res";
		case RelayVerb::SocialProjection: return "social_proj";
		case RelayVerb::SocialChat: return "social_chat";
		default: return "unknown";
	}
}

RelayVerb RelayCodec::VerbFromToken(const std::string& token) {
	if (token == "register") return RelayVerb::Register;
	if (token == "register_ack") return RelayVerb::RegisterAck;
	if (token == "heartbeat") return RelayVerb::Heartbeat;
	if (token == "report_load") return RelayVerb::ReportLoad;
	if (token == "expect") return RelayVerb::ExpectTicket;
	if (token == "expect_ack") return RelayVerb::ExpectAck;
	if (token == "chat") return RelayVerb::ChatRelay;
	if (token == "join") return RelayVerb::PlayerJoined;
	if (token == "leave") return RelayVerb::PlayerLeft;
	if (token == "handoff") return RelayVerb::Handoff;
	if (token == "return") return RelayVerb::Return;
	if (token == "social_req") return RelayVerb::SocialRequest;
	if (token == "social_res") return RelayVerb::SocialResult;
	if (token == "social_proj") return RelayVerb::SocialProjection;
	if (token == "social_chat") return RelayVerb::SocialChat;
	return RelayVerb::Unknown;
}

std::string RelayCodec::Encode(const RelayMessage& message) {
	if (message.version != kRelayProtocolVersion || message.request_id.empty() ||
		message.verb == RelayVerb::Unknown) return {};
	std::ostringstream out;
	out << "{\"version\":" << message.version
		<< ",\"request_id\":\"" << JsonEscape(message.request_id)
		<< "\",\"sent_ms\":" << message.sent_ms
		<< ",\"auth\":\"" << "\",\"verb\":\"" << VerbToken(message.verb) << '"';
	if (!message.district.empty()) out << ",\"district\":\"" << JsonEscape(message.district) << '"';
	if (message.numeric_id != 0) out << ",\"numeric_id\":" << message.numeric_id;
	if (message.port != 0) out << ",\"port\":" << message.port;
	if (!message.account.empty()) out << ",\"account\":\"" << JsonEscape(message.account) << '"';
	if (!message.character.empty()) out << ",\"character\":\"" << JsonEscape(message.character) << '"';
	if (!message.faction.empty()) out << ",\"faction\":\"" << JsonEscape(message.faction) << '"';
	if (!message.jti.empty()) out << ",\"jti\":\"" << JsonEscape(message.jti) << '"';
	if (!message.nonce.empty()) out << ",\"nonce\":\"" << JsonEscape(message.nonce) << '"';
	if (message.jti_expires_ms != 0) out << ",\"jti_expires_ms\":" << message.jti_expires_ms;
	if (!message.from.empty()) out << ",\"from\":\"" << JsonEscape(message.from) << '"';
	if (!message.to.empty()) out << ",\"to\":\"" << JsonEscape(message.to) << '"';
	if (!message.body.empty()) out << ",\"body\":\"" << JsonEscape(message.body) << '"';
	if (message.player_count != 0) out << ",\"player_count\":" << message.player_count;
	if (message.seq != 0) out << ",\"seq\":" << message.seq;
	if (!message.operation_id.empty()) out << ",\"operation_id\":\"" << JsonEscape(message.operation_id) << '"';
	if (!message.authority_epoch.empty()) out << ",\"authority_epoch\":\"" << JsonEscape(message.authority_epoch) << '"';
	if (message.revision != 0) out << ",\"revision\":" << message.revision;
	if (!message.social_op.empty()) out << ",\"social_op\":\"" << JsonEscape(message.social_op) << '"';
	if (!message.social_status.empty()) out << ",\"social_status\":\"" << JsonEscape(message.social_status) << '"';
	if (message.verb == RelayVerb::RegisterAck || message.verb == RelayVerb::ExpectAck)
		out << ",\"ok\":" << (message.ok ? "true" : "false");
	out << "}\n";
	std::string encoded = out.str();
	if (!message.auth.empty()) {
		std::string hmac_hex = hmac_sha256_hex(
			reinterpret_cast<const uint8_t*>(message.auth.data()), message.auth.size(),
			reinterpret_cast<const uint8_t*>(encoded.data()), encoded.size());
		size_t auth_pos = encoded.find("\"auth\":\"\"");
		if (auth_pos != std::string::npos) {
			encoded.insert(auth_pos + 8, hmac_hex);
		}
	}
	return IsFrameSizeValid(encoded) ? encoded : std::string{};
}

bool RelayCodec::IsFrameSizeValid(const std::string& line) {
	return line.size() <= kRelayMaxFrameBytes;
}

bool RelayCodec::Decode(const std::string& line, RelayMessage& out) {
	out = RelayMessage{};
	if (!IsFrameSizeValid(line)) return false;
	std::string input = line;
	while (!input.empty() && (input.back() == '\n' || input.back() == '\r')) input.pop_back();
	std::unordered_map<std::string, JsonValue> values;
	if (!ParseFlatJson(input, values)) return false;
	int64_t integer = 0;
	if (!GetInteger(values, "version", integer, true)) return false;
	out.version = static_cast<int32_t>(integer);
	if (out.version != kRelayProtocolVersion) return false;
	if (!GetString(values, "request_id", out.request_id, true) || out.request_id.empty()) return false;
	if (!GetInteger(values, "sent_ms", out.sent_ms, true)) return false;
	if (!GetString(values, "auth", out.auth, true)) return false;
	std::string token;
	if (!GetString(values, "verb", token, true)) return false;
	out.verb = VerbFromToken(token);
	if (out.verb == RelayVerb::Unknown) return false;
	if (!GetString(values, "district", out.district) ||
		!GetInteger(values, "numeric_id", integer)) return false;
	out.numeric_id = static_cast<int32_t>(integer);
	if (!GetInteger(values, "port", integer)) return false;
	out.port = static_cast<int32_t>(integer);
	if (!GetString(values, "account", out.account) || !GetString(values, "character", out.character) ||
		!GetString(values, "faction", out.faction) || !GetString(values, "jti", out.jti) ||
		!GetString(values, "nonce", out.nonce) ||
		!GetInteger(values, "jti_expires_ms", out.jti_expires_ms) ||
		!GetString(values, "from", out.from) || !GetString(values, "to", out.to) ||
		!GetString(values, "body", out.body) || !GetInteger(values, "player_count", integer) ||
		!GetInteger(values, "seq", out.seq) || !GetBoolean(values, "ok", out.ok) ||
		!GetString(values, "operation_id", out.operation_id) ||
		!GetString(values, "authority_epoch", out.authority_epoch) ||
		!GetInteger(values, "revision", out.revision) ||
		!GetString(values, "social_op", out.social_op) ||
		!GetString(values, "social_status", out.social_status)) return false;
	out.player_count = static_cast<int32_t>(integer);
	return true;
}

std::vector<RelayMessage> RelayCodec::DecodeStream(std::string& buffer) {
	std::vector<RelayMessage> messages;
	size_t consumed = 0;
	while (true) {
		const size_t newline = buffer.find('\n', consumed);
		if (newline == std::string::npos) break;
		std::string line = buffer.substr(consumed, newline - consumed);
		consumed = newline + 1;
		if (!line.empty() && line.back() == '\r') line.pop_back();
		RelayMessage message;
		if (!line.empty() && Decode(line, message)) messages.push_back(std::move(message));
	}
	if (consumed > 0) buffer.erase(0, consumed);
	if (buffer.size() > kRelayMaxFrameBytes) buffer.clear();
	return messages;
}

RelayMessage RelayCodec::MakeRegister(const std::string& district, int32_t numeric_id, int32_t port,
	const std::string& request_id, int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::Register, request_id, sent_ms, auth, numeric_id);
	message.district = district; message.numeric_id = numeric_id; message.port = port; return message;
}

RelayMessage RelayCodec::MakeRegisterAck(int32_t numeric_id, bool ok, const std::string& request_id,
	int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::RegisterAck, request_id, sent_ms, auth, numeric_id);
	message.numeric_id = numeric_id; message.ok = ok; return message;
}

RelayMessage RelayCodec::MakeHeartbeat(int32_t numeric_id, int64_t seq, const std::string& request_id,
	int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::Heartbeat, request_id, sent_ms, auth, numeric_id, seq);
	message.numeric_id = numeric_id; message.seq = seq; return message;
}

RelayMessage RelayCodec::MakeReportLoad(int32_t numeric_id, int32_t player_count,
	const std::string& request_id, int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::ReportLoad, request_id, sent_ms, auth, numeric_id);
	message.numeric_id = numeric_id; message.player_count = player_count; return message;
}

RelayMessage RelayCodec::MakeExpectTicket(const std::string& account, const std::string& character,
	const std::string& faction, const std::string& jti, const std::string& district, int32_t numeric_id,
	const std::string& request_id, int64_t sent_ms, int64_t jti_expires_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::ExpectTicket, request_id, sent_ms, auth, numeric_id, 0, jti);
	message.account = account; message.character = character; message.faction = faction; message.jti = jti;
	message.district = district; message.numeric_id = numeric_id; message.jti_expires_ms = jti_expires_ms; return message;
}

RelayMessage RelayCodec::MakeExpectAck(const std::string& jti, bool ok, const std::string& request_id,
	int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::ExpectAck, request_id, sent_ms, auth, 0, 0, jti);
	message.jti = jti; message.ok = ok; return message;
}

RelayMessage RelayCodec::MakeChatRelay(const std::string& from, const std::string& to,
	const std::string& body, int32_t numeric_id, const std::string& request_id, int64_t sent_ms,
	const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::ChatRelay, request_id, sent_ms, auth, numeric_id);
	message.from = from; message.to = to; message.body = body; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakePlayerJoined(const std::string& account, const std::string& character,
	int32_t numeric_id, const std::string& request_id, int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::PlayerJoined, request_id, sent_ms, auth, numeric_id);
	message.account = account; message.character = character; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakePlayerLeft(const std::string& account, const std::string& character,
	int32_t numeric_id, const std::string& request_id, int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::PlayerLeft, request_id, sent_ms, auth, numeric_id);
	message.account = account; message.character = character; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeHandoff(const std::string& account, const std::string& character,
	const std::string& faction, const std::string& jti, const std::string& nonce,
	const std::string& body, const int32_t numeric_id,
	const std::string& request_id, const int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::Handoff, request_id, sent_ms, auth, numeric_id, 0, jti);
	message.account = account; message.character = character; message.faction = faction; message.jti = jti;
	message.nonce = nonce; message.body = body; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeReturn(const std::string& account, const std::string& character,
	const std::string& faction, const std::string& jti, const std::string& nonce,
	const std::string& body, const int32_t numeric_id,
	const std::string& request_id, const int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::Return, request_id, sent_ms, auth, numeric_id, 0, jti);
	message.account = account; message.character = character; message.faction = faction; message.jti = jti;
	message.nonce = nonce; message.body = body; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeSocialRequest(const std::string& account, const std::string& character,
	const std::string& social_op, const std::string& operation_id, const std::string& body,
	const int32_t numeric_id, const std::string& request_id, const int64_t sent_ms,
	const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::SocialRequest, request_id, sent_ms, auth, numeric_id, 0, operation_id);
	message.account = account; message.character = character; message.social_op = social_op;
	message.operation_id = operation_id; message.body = body; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeSocialResult(const std::string& character, const std::string& operation_id,
	const std::string& social_status, const int64_t revision, const std::string& authority_epoch,
	const std::string& body, const int32_t numeric_id, const std::string& request_id,
	const int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::SocialResult, request_id, sent_ms, auth, numeric_id, 0, operation_id);
	message.character = character; message.operation_id = operation_id; message.social_status = social_status;
	message.revision = revision; message.authority_epoch = authority_epoch; message.body = body;
	message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeSocialProjection(const std::string& character, const int64_t revision,
	const std::string& authority_epoch, const std::string& body, const int32_t numeric_id,
	const std::string& request_id, const int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::SocialProjection, request_id, sent_ms, auth, numeric_id, revision);
	message.character = character; message.revision = revision; message.authority_epoch = authority_epoch;
	message.body = body; message.numeric_id = numeric_id; return message;
}

RelayMessage RelayCodec::MakeSocialChat(const std::string& from, const std::string& to,
	const std::string& body, const std::string& social_op, const int32_t numeric_id,
	const std::string& request_id, const int64_t sent_ms, const std::string& auth) {
	RelayMessage message = MakeEnvelope(RelayVerb::SocialChat, request_id, sent_ms, auth, numeric_id);
	message.from = from; message.to = to; message.body = body; message.social_op = social_op;
	message.numeric_id = numeric_id; return message;
}

bool ShouldApplySocialProjection(const std::string& current_epoch, const int64_t current_revision,
	const std::string& incoming_epoch, const int64_t incoming_revision) {
	if (current_epoch.empty()) return true;
	if (current_epoch != incoming_epoch) return true;
	return incoming_revision > current_revision;
}

const char* RelayRejectReasonName(RelayRejectReason reason) {
	switch (reason) {
		case RelayRejectReason::None: return "none";
		case RelayRejectReason::Malformed: return "malformed";
		case RelayRejectReason::Oversize: return "oversize";
		case RelayRejectReason::UnknownVersion: return "unknown_version";
		case RelayRejectReason::MissingRequestId: return "missing_request_id";
		case RelayRejectReason::DuplicateRequestId: return "duplicate_request_id";
		case RelayRejectReason::TimedOut: return "timed_out";
		case RelayRejectReason::AuthFailed: return "auth_failed";
		case RelayRejectReason::StaleJti: return "stale_jti";
		case RelayRejectReason::ReplayJti: return "replay_jti";
		case RelayRejectReason::QueueFull: return "queue_full";
		case RelayRejectReason::VerbDirection: return "verb_direction";
		case RelayRejectReason::IdentityMismatch: return "identity_mismatch";
		default: return "unknown";
	}
}

RelayInbox::RelayInbox(RelayValidationOptions options) : options_(std::move(options)) {}

RelayRejectReason RelayInbox::Submit(const std::string& frame) {
	if (!RelayCodec::IsFrameSizeValid(frame)) return RelayRejectReason::Oversize;
	RelayMessage message;
	if (!RelayCodec::Decode(frame, message)) {
		std::string input = frame;
		while (!input.empty() && (input.back() == '\n' || input.back() == '\r')) input.pop_back();
		std::unordered_map<std::string, JsonValue> values;
		int64_t version = 0;
		if (ParseFlatJson(input, values) && GetInteger(values, "version", version, true)) {
			if (version != kRelayProtocolVersion) return RelayRejectReason::UnknownVersion;
			std::string request_id;
			if (!GetString(values, "request_id", request_id, true) || request_id.empty())
				return RelayRejectReason::MissingRequestId;
		}
		return RelayRejectReason::Malformed;
	}
	if (message.sent_ms < options_.now_ms - kRelayRequestTimeoutMs ||
		message.sent_ms > options_.now_ms + kRelayRequestTimeoutMs) return RelayRejectReason::TimedOut;

	if (options_.require_auth && message.auth.empty()) return RelayRejectReason::AuthFailed;
	if (!options_.expected_auth.empty()) {
		std::string provided_auth = message.auth;
		RelayMessage m2 = message;
		m2.auth = "";
		std::string canonical = RelayCodec::Encode(m2);
		std::string expected_hmac = hmac_sha256_hex(
			reinterpret_cast<const uint8_t*>(options_.expected_auth.data()), options_.expected_auth.size(),
			reinterpret_cast<const uint8_t*>(canonical.data()), canonical.size());
		
		if (provided_auth.size() != expected_hmac.size()) {
			return RelayRejectReason::AuthFailed;
		}
		volatile uint8_t diff = 0;
		for (size_t i = 0; i < expected_hmac.size(); ++i) {
			diff |= (static_cast<uint8_t>(provided_auth[i]) ^ static_cast<uint8_t>(expected_hmac[i]));
		}
		if (diff != 0) {
			return RelayRejectReason::AuthFailed;
		}
	}

	if (options_.is_world_server.has_value()) {
		if (options_.is_world_server.value()) {
			if (message.verb == RelayVerb::RegisterAck || message.verb == RelayVerb::ExpectTicket ||
				message.verb == RelayVerb::Handoff || message.verb == RelayVerb::SocialResult ||
				message.verb == RelayVerb::SocialProjection) {
				return RelayRejectReason::VerbDirection;
			}
		} else {
			if (message.verb == RelayVerb::Register || message.verb == RelayVerb::Heartbeat ||
				message.verb == RelayVerb::ReportLoad || message.verb == RelayVerb::ExpectAck ||
				message.verb == RelayVerb::PlayerJoined || message.verb == RelayVerb::PlayerLeft ||
				message.verb == RelayVerb::Return || message.verb == RelayVerb::SocialRequest) {
				return RelayRejectReason::VerbDirection;
			}
		}
	}

	if (options_.expected_numeric_id != 0 && message.numeric_id != 0 &&
		message.numeric_id != options_.expected_numeric_id) {
		return RelayRejectReason::IdentityMismatch;
	}

	if (request_ids_.find(message.request_id) != request_ids_.end()) return RelayRejectReason::DuplicateRequestId;
	if (message.verb == RelayVerb::ExpectTicket) {
		if (message.jti.empty() || message.jti_expires_ms <= options_.now_ms) return RelayRejectReason::StaleJti;
		if (jtis_.find(message.jti) != jtis_.end()) return RelayRejectReason::ReplayJti;
	}
	if (queue_.size() >= kRelayMaxQueueDepth) return RelayRejectReason::QueueFull;
	queue_.push_back(std::move(message));
	RememberRequest(queue_.back().request_id);
	if (queue_.back().verb == RelayVerb::ExpectTicket) RememberJti(queue_.back().jti);
	return RelayRejectReason::None;
}

bool RelayInbox::Pop(RelayMessage& out) {
	if (queue_.empty()) return false;
	out = std::move(queue_.front()); queue_.pop_front(); return true;
}

void RelayInbox::Clear() { queue_.clear(); request_history_.clear(); request_ids_.clear(); jti_history_.clear(); jtis_.clear(); }

void RelayInbox::RememberRequest(const std::string& request_id) {
	request_history_.push_back(request_id); request_ids_.insert(request_id);
	while (request_history_.size() > kRelayMaxQueueDepth) { request_ids_.erase(request_history_.front()); request_history_.pop_front(); }
}

void RelayInbox::RememberJti(const std::string& jti) {
	jti_history_.push_back(jti); jtis_.insert(jti);
	while (jti_history_.size() > kRelayMaxQueueDepth) { jtis_.erase(jti_history_.front()); jti_history_.pop_front(); }
}

int64_t RelayReconnectDelayMs(int32_t attempt) {
	if (attempt < 0) attempt = 0;
	int64_t delay = kRelayReconnectInitialDelayMs;
	for (int32_t i = 0; i < attempt && delay < kRelayReconnectMaxDelayMs; ++i)
		delay = std::min(kRelayReconnectMaxDelayMs, delay * 2);
	return delay;
}

}
