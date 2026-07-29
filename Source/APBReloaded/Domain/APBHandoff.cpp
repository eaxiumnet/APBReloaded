#include "APBHandoff.h"
#include "APBCrypto.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apb {
namespace {

enum class JsonKind { String, Number, Boolean, Array, Object };

struct JsonValue {
	JsonKind kind = JsonKind::String;
	std::string string;
	double number = 0.0;
	bool boolean = false;
	std::vector<JsonValue> array;
	std::unordered_map<std::string, JsonValue> object;
};

void SkipWhitespace(const std::string& text, size_t& pos) {
	while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t' || text[pos] == '\r' || text[pos] == '\n')) ++pos;
}

bool ParseString(const std::string& text, size_t& pos, std::string& out) {
	if (pos >= text.size() || text[pos++] != '"') return false;
	out.clear();
	while (pos < text.size()) {
		const unsigned char c = static_cast<unsigned char>(text[pos++]);
		if (c == '"') return true;
		if (c < 0x20u) return false;
		if (c != '\\') { out.push_back(static_cast<char>(c)); continue; }
		if (pos >= text.size()) return false;
		switch (text[pos++]) {
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

bool ParseValue(const std::string& text, size_t& pos, JsonValue& out);

bool ParseArray(const std::string& text, size_t& pos, JsonValue& out) {
	if (pos >= text.size() || text[pos++] != '[') return false;
	out.kind = JsonKind::Array;
	out.array.clear();
	SkipWhitespace(text, pos);
	if (pos < text.size() && text[pos] == ']') { ++pos; return true; }
	while (pos < text.size()) {
		JsonValue item;
		if (!ParseValue(text, pos, item)) return false;
		out.array.push_back(std::move(item));
		SkipWhitespace(text, pos);
		if (pos >= text.size()) return false;
		if (text[pos] == ']') { ++pos; return true; }
		if (text[pos++] != ',') return false;
		SkipWhitespace(text, pos);
	}
	return false;
}

bool ParseObject(const std::string& text, size_t& pos, JsonValue& out) {
	if (pos >= text.size() || text[pos++] != '{') return false;
	out.kind = JsonKind::Object;
	out.object.clear();
	SkipWhitespace(text, pos);
	if (pos < text.size() && text[pos] == '}') { ++pos; return true; }
	while (pos < text.size()) {
		std::string key;
		if (!ParseString(text, pos, key)) return false;
		if (out.object.find(key) != out.object.end()) return false;
		SkipWhitespace(text, pos);
		if (pos >= text.size() || text[pos++] != ':') return false;
		SkipWhitespace(text, pos);
		JsonValue value;
		if (!ParseValue(text, pos, value)) return false;
		out.object.emplace(std::move(key), std::move(value));
		SkipWhitespace(text, pos);
		if (pos >= text.size()) return false;
		if (text[pos] == '}') { ++pos; return true; }
		if (text[pos++] != ',') return false;
		SkipWhitespace(text, pos);
	}
	return false;
}

bool ParseValue(const std::string& text, size_t& pos, JsonValue& out) {
	SkipWhitespace(text, pos);
	if (pos >= text.size()) return false;
	if (text[pos] == '{') return ParseObject(text, pos, out);
	if (text[pos] == '[') return ParseArray(text, pos, out);
	if (text[pos] == '"') { out.kind = JsonKind::String; return ParseString(text, pos, out.string); }
	if (text.compare(pos, 4, "true") == 0) { out.kind = JsonKind::Boolean; out.boolean = true; pos += 4; return true; }
	if (text.compare(pos, 5, "false") == 0) { out.kind = JsonKind::Boolean; out.boolean = false; pos += 5; return true; }
	char* end = nullptr;
	const double number = std::strtod(text.c_str() + pos, &end);
	if (end == text.c_str() + pos || !std::isfinite(number)) return false;
	pos = static_cast<size_t>(end - text.c_str());
	out.kind = JsonKind::Number;
	out.number = number;
	return true;
}

bool ParseJson(const std::string& text, JsonValue& out) {
	size_t pos = 0;
	if (!ParseValue(text, pos, out)) return false;
	SkipWhitespace(text, pos);
	return pos == text.size();
}

const JsonValue* Field(const JsonValue& object, const char* name) {
	if (object.kind != JsonKind::Object) return nullptr;
	const auto it = object.object.find(name);
	return it == object.object.end() ? nullptr : &it->second;
}

bool ReadString(const JsonValue& object, const char* name, std::string& out) {
	const JsonValue* value = Field(object, name);
	if (!value || value->kind != JsonKind::String) return false;
	out = value->string;
	return true;
}

bool ReadBool(const JsonValue& object, const char* name, bool& out) {
	const JsonValue* value = Field(object, name);
	if (!value || value->kind != JsonKind::Boolean) return false;
	out = value->boolean;
	return true;
}

bool ReadNumber(const JsonValue& object, const char* name, double& out) {
	const JsonValue* value = Field(object, name);
	if (!value || value->kind != JsonKind::Number || !std::isfinite(value->number)) return false;
	out = value->number;
	return true;
}

bool ReadInt64(const JsonValue& object, const char* name, int64_t& out) {
	double value = 0;
	if (!ReadNumber(object, name, value) || std::floor(value) != value ||
		value < static_cast<double>(std::numeric_limits<int64_t>::min()) ||
		value > static_cast<double>(std::numeric_limits<int64_t>::max())) return false;
	out = static_cast<int64_t>(value);
	return true;
}

bool ReadInt32(const JsonValue& object, const char* name, int32_t& out) {
	int64_t value = 0;
	if (!ReadInt64(object, name, value) || value < std::numeric_limits<int32_t>::min() ||
		value > std::numeric_limits<int32_t>::max()) return false;
	out = static_cast<int32_t>(value);
	return true;
}

void WriteString(std::ostringstream& out, const std::string& value) {
	out << '"';
	for (const unsigned char c : value) {
		switch (c) {
			case '"': out << "\\\""; break;
			case '\\': out << "\\\\"; break;
			case '\b': out << "\\b"; break;
			case '\f': out << "\\f"; break;
			case '\n': out << "\\n"; break;
			case '\r': out << "\\r"; break;
			case '\t': out << "\\t"; break;
			default: if (c >= 0x20u) out << static_cast<char>(c); break;
		}
	}
	out << '"';
}

bool ParseFaction(const std::string& text, Faction& out) {
	if (text == "Enforcer") { out = Faction::Enforcer; return true; }
	if (text == "Criminal") { out = Faction::Criminal; return true; }
	return false;
}

bool ReadProgressEntries(const JsonValue& object, const char* name, std::vector<SnapshotProgressEntry>& out) {
	const JsonValue* values = Field(object, name);
	if (!values || values->kind != JsonKind::Array) return false;
	out.clear();
	for (const JsonValue& value : values->array) {
		SnapshotProgressEntry entry;
		if (!ReadString(value, "id", entry.id) || !ReadInt64(value, "value", entry.value) || entry.id.empty()) return false;
		out.push_back(std::move(entry));
	}
	std::sort(out.begin(), out.end(), [](const SnapshotProgressEntry& a, const SnapshotProgressEntry& b) { return a.id < b.id; });
	return std::adjacent_find(out.begin(), out.end(), [](const SnapshotProgressEntry& a, const SnapshotProgressEntry& b) { return a.id == b.id; }) == out.end();
}

void WriteProgressEntries(std::ostringstream& out, const std::vector<SnapshotProgressEntry>& entries) {
	out << '[';
	for (size_t i = 0; i < entries.size(); ++i) {
		if (i) out << ',';
		out << "{\"id\":";
		WriteString(out, entries[i].id);
		out << ",\"value\":" << entries[i].value << '}';
	}
	out << ']';
}

bool SnapshotFromValue(const JsonValue& root, DomainSnapshot& out) {
	if (root.kind != JsonKind::Object) return false;
	DomainSnapshot candidate;
	std::string faction;
	if (!ReadBool(root, "has_character", candidate.has_character) || !ReadString(root, "character_name", candidate.character_name) ||
		!ReadString(root, "faction", faction) || !ParseFaction(faction, candidate.faction) ||
		!ReadInt64(root, "cash", candidate.cash) || !ReadInt64(root, "g1c", candidate.g1c) ||
		!ReadInt32(root, "inventory_slot_count", candidate.inventory_slot_count) || !ReadInt32(root, "inventory_total_qty", candidate.inventory_total_qty) ||
		!ReadNumber(root, "threat_points", candidate.threat_points) || !ReadInt32(root, "threat_bots", candidate.threat_bots) ||
		!ReadInt32(root, "threat_level", candidate.threat_level) || !ReadString(root, "threat_tier_name", candidate.threat_tier_name) ||
		!ReadString(root, "threat_tier_description", candidate.threat_tier_description) || !ReadString(root, "mission_id", candidate.mission_id) ||
		!ReadString(root, "mission_title", candidate.mission_title) || !ReadInt32(root, "mission_stage_index", candidate.mission_stage_index) ||
		!ReadInt32(root, "mission_stage_count", candidate.mission_stage_count) || !ReadString(root, "mission_status", candidate.mission_status) ||
		!ReadBool(root, "mission_opposition_contesting", candidate.mission_opposition_contesting) ||
		!ReadBool(root, "mission_opposition_won", candidate.mission_opposition_won) || !ReadNumber(root, "mission_stage_progress", candidate.mission_stage_progress) ||
		!ReadNumber(root, "mission_opp_stage_progress", candidate.mission_opp_stage_progress) || !ReadBool(root, "mission_timed_out", candidate.mission_timed_out) ||
		!ReadNumber(root, "mission_stage_time_limit_sec", candidate.mission_stage_time_limit_sec) || !ReadString(root, "session_id", candidate.session_id) ||
		!ReadString(root, "district_id", candidate.district_id) || !ReadInt32(root, "district_players", candidate.district_players) ||
		!ReadProgressEntries(root, "contact_standings", candidate.contact_standings) || !ReadProgressEntries(root, "role_xp", candidate.role_xp) ||
		!ReadString(root, "active_contact_id", candidate.active_contact_id) || !ReadInt64(root, "active_contact_standing", candidate.active_contact_standing) ||
		!ReadInt32(root, "active_contact_level", candidate.active_contact_level)) return false;
	if (!candidate.has_character || candidate.character_name.empty() || candidate.cash < 0 || candidate.g1c < 0 ||
		candidate.inventory_slot_count < 0 || candidate.inventory_total_qty < 0 || candidate.inventory_slot_count > candidate.inventory_total_qty ||
		candidate.district_players < 0 || !std::isfinite(candidate.threat_points) || !std::isfinite(candidate.mission_stage_progress) ||
		!std::isfinite(candidate.mission_opp_stage_progress) || !std::isfinite(candidate.mission_stage_time_limit_sec)) return false;
	out = std::move(candidate);
	return true;
}

bool ConstantTimeEqual(const std::string& a, const std::string& b) {
	if (a.size() != b.size()) return false;
	unsigned char difference = 0;
	for (size_t i = 0; i < a.size(); ++i) difference |= static_cast<unsigned char>(a[i] ^ b[i]);
	return difference == 0;
}

std::string Payload(const CharacterHandoff& handoff) {
	std::ostringstream out;
	out << "{\"account\":"; WriteString(out, handoff.account);
	out << ",\"character\":"; WriteString(out, handoff.character);
	out << ",\"faction\":"; WriteString(out, handoff.faction);
	out << ",\"jti\":"; WriteString(out, handoff.jti);
	out << ",\"nonce\":"; WriteString(out, handoff.nonce);
	out << ",\"sent_ms\":" << handoff.sent_ms << ",\"snapshot\":" << SerializeSnapshot(handoff.snapshot) << '}';
	return out.str();
}
}

std::string SerializeSnapshot(const DomainSnapshot& snapshot) {
	std::ostringstream out;
	out << std::setprecision(17) << "{\"has_character\":" << (snapshot.has_character ? "true" : "false") << ",\"character_name\":";
	WriteString(out, snapshot.character_name);
	out << ",\"faction\":"; WriteString(out, FactionName(snapshot.faction));
	out << ",\"cash\":" << snapshot.cash << ",\"g1c\":" << snapshot.g1c;
	out << ",\"inventory_slot_count\":" << snapshot.inventory_slot_count << ",\"inventory_total_qty\":" << snapshot.inventory_total_qty;
	out << ",\"threat_points\":" << snapshot.threat_points << ",\"threat_bots\":" << snapshot.threat_bots << ",\"threat_level\":" << snapshot.threat_level;
	out << ",\"threat_tier_name\":"; WriteString(out, snapshot.threat_tier_name);
	out << ",\"threat_tier_description\":"; WriteString(out, snapshot.threat_tier_description);
	out << ",\"mission_id\":"; WriteString(out, snapshot.mission_id);
	out << ",\"mission_title\":"; WriteString(out, snapshot.mission_title);
	out << ",\"mission_stage_index\":" << snapshot.mission_stage_index << ",\"mission_stage_count\":" << snapshot.mission_stage_count;
	out << ",\"mission_status\":"; WriteString(out, snapshot.mission_status);
	out << ",\"mission_opposition_contesting\":" << (snapshot.mission_opposition_contesting ? "true" : "false");
	out << ",\"mission_opposition_won\":" << (snapshot.mission_opposition_won ? "true" : "false");
	out << ",\"mission_stage_progress\":" << snapshot.mission_stage_progress << ",\"mission_opp_stage_progress\":" << snapshot.mission_opp_stage_progress;
	out << ",\"mission_timed_out\":" << (snapshot.mission_timed_out ? "true" : "false");
	out << ",\"mission_stage_time_limit_sec\":" << snapshot.mission_stage_time_limit_sec;
	out << ",\"session_id\":"; WriteString(out, snapshot.session_id);
	out << ",\"district_id\":"; WriteString(out, snapshot.district_id);
	out << ",\"district_players\":" << snapshot.district_players << ",\"contact_standings\":";
	WriteProgressEntries(out, snapshot.contact_standings);
	out << ",\"role_xp\":"; WriteProgressEntries(out, snapshot.role_xp);
	out << ",\"active_contact_id\":"; WriteString(out, snapshot.active_contact_id);
	out << ",\"active_contact_standing\":" << snapshot.active_contact_standing << ",\"active_contact_level\":" << snapshot.active_contact_level << '}';
	return out.str();
}

bool DeserializeSnapshot(const std::string& json, DomainSnapshot& out) {
	JsonValue root;
	return ParseJson(json, root) && SnapshotFromValue(root, out);
}

std::string SignHandoff(const CharacterHandoff& handoff, const std::string& secret_hex) {
	if (handoff.account.empty() || handoff.character.empty() || handoff.faction.empty() || handoff.jti.empty() || handoff.nonce.empty() || secret_hex.empty()) return {};
	const std::vector<uint8_t> key = hex_decode(secret_hex);
	if (key.empty()) return {};
	const std::string payload = Payload(handoff);
	const std::string signature = hmac_sha256_hex(key.data(), key.size(), reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
	return payload + '.' + signature;
}

bool VerifyHandoff(const std::string& signed_handoff, const std::string& secret_hex, CharacterHandoff& out) {
	out = CharacterHandoff{};
	const size_t dot = signed_handoff.rfind('.');
	if (dot == std::string::npos || dot == 0 || dot == signed_handoff.size() - 1 || secret_hex.empty()) return false;
	const std::vector<uint8_t> key = hex_decode(secret_hex);
	if (key.empty()) return false;
	const std::string payload = signed_handoff.substr(0, dot);
	const std::string signature = signed_handoff.substr(dot + 1);
	const std::string expected = hmac_sha256_hex(key.data(), key.size(), reinterpret_cast<const uint8_t*>(payload.data()), payload.size());
	if (!ConstantTimeEqual(expected, signature)) return false;
	JsonValue root;
	if (!ParseJson(payload, root) || root.kind != JsonKind::Object) return false;
	CharacterHandoff candidate;
	const JsonValue* snapshot = Field(root, "snapshot");
	if (!ReadString(root, "account", candidate.account) || !ReadString(root, "character", candidate.character) ||
		!ReadString(root, "faction", candidate.faction) || !ReadString(root, "jti", candidate.jti) || !ReadString(root, "nonce", candidate.nonce) ||
		!ReadInt64(root, "sent_ms", candidate.sent_ms) || !snapshot || snapshot->kind != JsonKind::Object ||
		!SnapshotFromValue(*snapshot, candidate.snapshot)) {
		return false;
	}
	if (candidate.account.empty() || candidate.character.empty() || candidate.faction.empty() || candidate.jti.empty() || candidate.nonce.empty() ||
		candidate.character != candidate.snapshot.character_name || candidate.faction != FactionName(candidate.snapshot.faction)) return false;
	out = std::move(candidate);
	return true;
}
}
