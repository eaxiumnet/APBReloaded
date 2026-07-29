#pragma once
// APB ceremony-message catalog (big on-screen celebration popup titles), extracted from
// the retail HUDCeremonyMsg.INT (mirror of the cooked SDD table "HUDCeremonyMsg") by
// tools/scripts/extract_hud_ceremony_msgs.ps1 -> Content/Data/hud_ceremony_msgs.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each ceremony carries a title (the big popup text) and a category (first id token).
// The <WeaponName> placeholder token is kept verbatim — the HUD substitutes it at runtime.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct CeremonyMsgDef {
	std::string id;        // "AM_CombatYouStreakKillOn", "AM_FameYouContactGainLevel", ...
	std::string title;     // "KILL STREAK!", "STANDING LEVEL UP!", ...
	std::string category;  // "AM", "Minigame", "ProvingGrounds", "Reward", "Trade", ...
	int32_t order = 0;
};

class CeremonyMsgCatalog {
public:
	std::vector<CeremonyMsgDef> msgs;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			CeremonyMsgDef m;
			m.id       = Unescape(RawStr(obj, "id"));
			m.title    = Unescape(RawStr(obj, "title"));
			m.category = Unescape(RawStr(obj, "category"));
			m.order    = (int32_t)RNum(obj, "order", 0);
			if (m.id.empty()) continue;
			auto it = std::find_if(msgs.begin(), msgs.end(),
				[&](const CeremonyMsgDef& e){ return e.id == m.id; });
			if (it == msgs.end()) msgs.push_back(m);
			else *it = m;
			++touched;
		}
		std::sort(msgs.begin(), msgs.end(),
			[](const CeremonyMsgDef& a, const CeremonyMsgDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const CeremonyMsgDef* Find(const std::string& id) const {
		for (const auto& m : msgs) if (m.id == id) return &m;
		return nullptr;
	}

	std::string Title(const std::string& id, const std::string& def = std::string()) const {
		const CeremonyMsgDef* m = Find(id);
		return m ? m->title : def;
	}
	std::string Category(const std::string& id, const std::string& def = std::string()) const {
		const CeremonyMsgDef* m = Find(id);
		return m ? m->category : def;
	}

	// All ceremonies in a given category (e.g. "AM" for achievement-manager events).
	std::vector<const CeremonyMsgDef*> ForCategory(const std::string& cat) const {
		std::vector<const CeremonyMsgDef*> out;
		for (const auto& m : msgs) if (m.category == cat) out.push_back(&m);
		return out;
	}

	// Distinct category list, first-appearance order.
	std::vector<std::string> Categories() const {
		std::vector<std::string> out;
		for (const auto& m : msgs)
			if (std::find(out.begin(), out.end(), m.category) == out.end())
				out.push_back(m.category);
		return out;
	}

	// True when the title contains a <Token> placeholder (e.g. <WeaponName>).
	static bool HasPlaceholder(const CeremonyMsgDef& m) {
		return m.title.find('<') != std::string::npos;
	}

	size_t Count() const { return msgs.size(); }

private:
	static std::vector<std::string> SplitTopObjects(const std::string& text) {
		std::vector<std::string> out;
		int depth = 0; size_t start = 0; bool inStr = false; bool esc = false;
		for (size_t i = 0; i < text.size(); ++i) {
			char c = text[i];
			if (esc) { esc = false; continue; }
			if (c == '\\' && inStr) { esc = true; continue; }
			if (c == '"') { inStr = !inStr; continue; }
			if (inStr) continue;
			if (c == '{') { if (depth == 0) start = i; ++depth; }
			else if (c == '}') { --depth; if (depth == 0) out.push_back(text.substr(start, i - start + 1)); }
		}
		return out;
	}

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

