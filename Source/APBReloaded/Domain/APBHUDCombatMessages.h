#pragma once
// APB on-screen combat score-feed catalog (the two-line floating messages shown when a scoring
// event happens: "Enemy Killed", "Kill Assist", "Objective Complete", "Demerit!", ...), extracted
// from the retail HUDCombatMessages.INT (mirror of the cooked SDD table "HUDCombatMessage") by
// tools/scripts/extract_hud_combat_messages.ps1 -> Content/Data/hud_combat_messages.json.
//
// Header-only (matches MedalCatalog / StreetNameCatalog / AmmoCategoryCatalog /
// ScoreboardDescriptionCatalog): every method is defined in-class so it is implicitly inline and
// safe to include in multiple TUs.
//
// Each feed entry is a two-line message, both verbatim from the INT:
//   line0  top line: a token ("<CharacterNameA>", "<MedalName>", "<GameplayObject>") or a label
//          ("Arson", "Teamkill"); may be empty.
//   line2  bottom line: the score message ("Enemy Killed", "Objective Complete", "Demerit!"); may
//          be empty.
// Tokens are kept verbatim; FormatLine0()/FormatLine2() substitute a single token at runtime.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct HUDCombatMessage {
	std::string id;    // "Score_Combat_KillEnemy", "Score_Mission_CSA_Arson", ...
	std::string line0; // top line (token or label), may be empty
	std::string line2; // bottom line (score message), may be empty
	int32_t order = 0; // stable display order (file order)
};

class HUDCombatMessageCatalog {
public:
	std::vector<HUDCombatMessage> messages; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge: existing ids are updated in place; new ids appended. Never clears
	// on empty input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			HUDCombatMessage m;
			m.id    = Unescape(RawStr(obj, "id"));
			m.line0 = Unescape(RawStr(obj, "line0"));
			m.line2 = Unescape(RawStr(obj, "line2"));
			m.order = (int32_t)RNum(obj, "order", 0);
			if (m.id.empty()) continue;
			auto it = std::find_if(messages.begin(), messages.end(),
				[&](const HUDCombatMessage& e){ return e.id == m.id; });
			if (it == messages.end()) messages.push_back(m);
			else *it = m;
			++touched;
		}
		std::sort(messages.begin(), messages.end(),
			[](const HUDCombatMessage& a, const HUDCombatMessage& b){ return a.order < b.order; });
		return touched > 0;
	}

	const HUDCombatMessage* Find(const std::string& id) const {
		for (const auto& m : messages) if (m.id == id) return &m;
		return nullptr;
	}

	std::string Line0(const std::string& id, const std::string& def = std::string()) const {
		const HUDCombatMessage* m = Find(id);
		return m ? m->line0 : def;
	}
	std::string Line2(const std::string& id, const std::string& def = std::string()) const {
		const HUDCombatMessage* m = Find(id);
		return m ? m->line2 : def;
	}

	// line0 with its single "<...>" token replaced by the caller-supplied value, e.g.
	// FormatLine0("Score_Combat_KillEnemy", "xoified") -> "xoified". If line0 has no token the
	// literal line is returned; unknown ids return the caller default.
	std::string FormatLine0(const std::string& id, const std::string& value,
		const std::string& def = std::string()) const {
		const HUDCombatMessage* m = Find(id);
		if (!m) return def;
		return SubstituteToken(m->line0, value);
	}
	std::string FormatLine2(const std::string& id, const std::string& value,
		const std::string& def = std::string()) const {
		const HUDCombatMessage* m = Find(id);
		if (!m) return def;
		return SubstituteToken(m->line2, value);
	}

	int32_t Count() const { return (int32_t)messages.size(); }

private:
	// Replaces the first "<...>" angle-bracket token in "tmpl" with "value". If there is no token
	// the template is returned unchanged.
	static std::string SubstituteToken(const std::string& tmpl, const std::string& value) {
		size_t lt = tmpl.find('<');
		if (lt == std::string::npos) return tmpl;
		size_t gt = tmpl.find('>', lt + 1);
		if (gt == std::string::npos) return tmpl;
		std::string out = tmpl;
		out.replace(lt, gt - lt + 1, value);
		return out;
	}

	// Depth-aware {...} splitter that respects JSON strings and escapes.
	static std::vector<std::string> SplitTopObjects(const std::string& text) {
		std::vector<std::string> out;
		int depth = 0; bool inStr = false, esc = false; size_t start = 0;
		for (size_t i = 0; i < text.size(); ++i) {
			char c = text[i];
			if (inStr) {
				if (esc) esc = false;
				else if (c == '\\') esc = true;
				else if (c == '"') inStr = false;
				continue;
			}
			if (c == '"') { inStr = true; continue; }
			if (c == '{') { if (depth == 0) start = i; ++depth; }
			else if (c == '}') { --depth; if (depth == 0) out.push_back(text.substr(start, i - start + 1)); }
		}
		return out;
	}

	// Returns the RAW (still-escaped) string value for "key" within a single object,
	// honouring backslash escapes so an embedded \" does not terminate early.
	static std::string RawStr(const std::string& obj, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return std::string();
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return std::string();
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r')) ++i;
		if (i >= obj.size() || obj[i] != '"') return std::string();
		++i;
		std::string raw; bool esc = false;
		for (; i < obj.size(); ++i) {
			char c = obj[i];
			if (esc) { raw.push_back('\\'); raw.push_back(c); esc = false; }
			else if (c == '\\') esc = true;
			else if (c == '"') break;
			else raw.push_back(c);
		}
		return raw;
	}

	// Numeric value for "key" (strtod). Returns def if absent.
	static double RNum(const std::string& obj, const std::string& key, double def) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return def;
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return def;
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) ++i;
		if (i >= obj.size()) return def;
		return std::strtod(obj.c_str() + i, nullptr);
	}

	// Proper JSON string unescape: \" \\ \/ \b \f \n \r \t and \uXXXX (-> UTF-8).
	static std::string Unescape(const std::string& raw) {
		std::string out; out.reserve(raw.size());
		for (size_t i = 0; i < raw.size(); ++i) {
			char c = raw[i];
			if (c != '\\') { out.push_back(c); continue; }
			if (i + 1 >= raw.size()) { out.push_back('\\'); break; }
			char n = raw[++i];
			switch (n) {
				case 'n': out.push_back('\n'); break;
				case 'r': out.push_back('\r'); break;
				case 't': out.push_back('\t'); break;
				case 'b': out.push_back('\b'); break;
				case 'f': out.push_back('\f'); break;
				case '/': out.push_back('/'); break;
				case '"': out.push_back('"'); break;
				case '\\': out.push_back('\\'); break;
				case 'u': {
					if (i + 4 < raw.size()) {
						unsigned code = 0; bool ok = true;
						for (int d = 1; d <= 4; ++d) {
							char h = raw[i + d]; unsigned v;
							if (h >= '0' && h <= '9') v = (unsigned)(h - '0');
							else if (h >= 'a' && h <= 'f') v = (unsigned)(h - 'a' + 10);
							else if (h >= 'A' && h <= 'F') v = (unsigned)(h - 'A' + 10);
							else { ok = false; break; }
							code = (code << 4) | v;
						}
						if (ok) { i += 4; AppendUtf8(out, code); break; }
					}
					out.push_back('u');
					break;
				}
				default: out.push_back(n); break;
			}
		}
		return out;
	}

	static void AppendUtf8(std::string& out, unsigned cp) {
		if (cp <= 0x7F) out.push_back((char)cp);
		else if (cp <= 0x7FF) {
			out.push_back((char)(0xC0 | (cp >> 6)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		} else {
			out.push_back((char)(0xE0 | (cp >> 12)));
			out.push_back((char)(0x80 | ((cp >> 6) & 0x3F)));
			out.push_back((char)(0x80 | (cp & 0x3F)));
		}
	}
};

} // namespace apb
