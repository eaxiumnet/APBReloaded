#pragma once
// APB HUD-message catalog (the broad on-screen HUD notifications / prompts / error banners: mission
// events, item/vehicle deliveries, "You cannot abandon opposed missions.", contact-standing prompts,
// arrest/bounty banners, ...), extracted from the retail HUDMessages.INT (mirror of the cooked SDD
// table "HUDMessage") by tools/scripts/extract_hud_messages.ps1 -> Content/Data/hud_messages.json.
// Distinct from the combat score-feed catalog (APBHUDCombatMessages.h / HUDCombatMessages.INT).
//
// Header-only (matches HUDCombatMessageCatalog / RoleMilestoneCatalog / ModifierItemTypeCatalog):
// every method is defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// Each message has two fields, both kept VERBATIM including markup:
//   display_text  the on-screen banner (may contain <col:NAME>...</col> colour spans, <Token>
//                 substitution placeholders, and forced '\n' line breaks).
//   chat_text     the system-chat-log version (usually empty).
// Helpers: StripColor() drops the <col:...>/</col> wrappers (leaving inner text + tokens);
// Format() substitutes a named <Token>. Colour NAMEs resolve against the HUD colour palette.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct HUDMessage {
	std::string id;            // "AM_Abandon_Match_Fail", "AM_AdHocSideDeliverVehicleDamaged", ...
	std::string display_text;  // on-screen banner (markup + tokens verbatim), may be empty
	std::string chat_text;     // system-chat-log version, usually empty
	int32_t order = 0;         // stable display order (file order)
};

class HUDMessageCatalog {
public:
	std::vector<HUDMessage> messages; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge: existing ids are updated in place; new ids appended. Never clears on empty
	// input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			HUDMessage m;
			m.id           = Unescape(RawStr(obj, "id"));
			m.display_text = Unescape(RawStr(obj, "display_text"));
			m.chat_text    = Unescape(RawStr(obj, "chat_text"));
			m.order        = (int32_t)RNum(obj, "order", 0);
			if (m.id.empty()) continue;
			auto it = std::find_if(messages.begin(), messages.end(),
				[&](const HUDMessage& e){ return e.id == m.id; });
			if (it == messages.end()) messages.push_back(m);
			else *it = m;
			++touched;
		}
		std::sort(messages.begin(), messages.end(),
			[](const HUDMessage& a, const HUDMessage& b){ return a.order < b.order; });
		return touched > 0;
	}

	const HUDMessage* Find(const std::string& id) const {
		for (const auto& m : messages) if (m.id == id) return &m;
		return nullptr;
	}

	std::string DisplayText(const std::string& id, const std::string& def = std::string()) const {
		const HUDMessage* m = Find(id);
		return m ? m->display_text : def;
	}
	std::string ChatText(const std::string& id, const std::string& def = std::string()) const {
		const HUDMessage* m = Find(id);
		return m ? m->chat_text : def;
	}

	// display_text with the <col:...>/</col> colour wrappers removed (inner text + <Token>
	// placeholders retained). Unknown ids return the caller default.
	std::string PlainDisplayText(const std::string& id, const std::string& def = std::string()) const {
		const HUDMessage* m = Find(id);
		return m ? StripColor(m->display_text) : def;
	}

	// display_text with every "<token>" occurrence replaced by value. e.g.
	// FormatDisplay("AM_AdHocEnemySideDeliverItem", "CharacterNameA", "xoified"). Colour spans and
	// other tokens are left intact; call repeatedly to fill multiple tokens. Unknown ids -> def.
	std::string FormatDisplay(const std::string& id, const std::string& token,
		const std::string& value, const std::string& def = std::string()) const {
		const HUDMessage* m = Find(id);
		if (!m) return def;
		return Format(m->display_text, token, value);
	}

	int32_t Count() const { return (int32_t)messages.size(); }

	// Removes <col:NAME> opening and </col> closing tags (case-insensitive) while preserving every
	// other angle-bracket token (e.g. <CharacterNameA>) and all literal text. Static: usable without
	// a catalog instance.
	static std::string StripColor(const std::string& text) {
		std::string out; out.reserve(text.size());
		for (size_t i = 0; i < text.size(); ) {
			if (text[i] == '<') {
				size_t gt = text.find('>', i + 1);
				if (gt != std::string::npos) {
					std::string tag = text.substr(i + 1, gt - i - 1);
					std::string low = ToLower(tag);
					if (low.compare(0, 4, "col:") == 0 || low == "/col") {
						i = gt + 1; // drop the whole tag
						continue;
					}
				}
			}
			out.push_back(text[i]);
			++i;
		}
		return out;
	}

	// Replaces every literal "<token>" in tmpl with value.
	static std::string Format(const std::string& tmpl, const std::string& token, const std::string& value) {
		const std::string needle = "<" + token + ">";
		if (needle.size() <= 2) return tmpl;
		std::string out; out.reserve(tmpl.size());
		size_t i = 0;
		while (i < tmpl.size()) {
			size_t hit = tmpl.find(needle, i);
			if (hit == std::string::npos) { out.append(tmpl, i, std::string::npos); break; }
			out.append(tmpl, i, hit - i);
			out.append(value);
			i = hit + needle.size();
		}
		return out;
	}

private:
	static std::string ToLower(const std::string& s) {
		std::string o = s;
		for (char& c : o) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
		return o;
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

	// Returns the RAW (still-escaped) string value for "key" within a single object, honouring
	// backslash escapes so an embedded \" does not terminate early.
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
