#pragma once
// APB ammunition-category catalog (weapon ammo pools + HUD ammo-counter text), extracted from
// the retail AmmoCategories.INT (mirror of the cooked SDD table "AmmoCategories") by
// tools/scripts/extract_ammo_categories.ps1 -> Content/Data/ammo_categories.json.
//
// Header-only (matches MedalCatalog / StreetNameCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each category carries four localized strings, all verbatim from the INT:
//   name             full name (inventory / mod screen)
//   name_abbreviated short label next to the HUD ammo counter
//   quantity_text    counter template containing the "<Num>" token, e.g. "<Num> bullets"
//   description      caliber / flavour text
// FormatQuantity() performs the live "<Num>" substitution the HUD does at runtime.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct AmmoCategory {
	std::string id;               // "Rifle", "44Magnum", "GrenadeFrag", "None", ...
	std::string name;             // "Rifle Ammo", "Magnum Ammo", ...
	std::string name_abbreviated; // "Rifle", "Magnum", "Frag Grenade", ...
	std::string quantity_text;    // "<Num> bullets", "<Num> grenades", ...
	std::string description;      // caliber / flavour text
	int32_t order = 0;            // stable display order (file order)
};

class AmmoCategoryCatalog {
public:
	std::vector<AmmoCategory> ammo; // sorted by order

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
			AmmoCategory a;
			a.id               = Unescape(RawStr(obj, "id"));
			a.name             = Unescape(RawStr(obj, "name"));
			a.name_abbreviated = Unescape(RawStr(obj, "name_abbreviated"));
			a.quantity_text    = Unescape(RawStr(obj, "quantity_text"));
			a.description      = Unescape(RawStr(obj, "description"));
			a.order            = (int32_t)RNum(obj, "order", 0);
			if (a.id.empty()) continue;
			auto it = std::find_if(ammo.begin(), ammo.end(),
				[&](const AmmoCategory& e){ return e.id == a.id; });
			if (it == ammo.end()) ammo.push_back(a);
			else *it = a;
			++touched;
		}
		std::sort(ammo.begin(), ammo.end(),
			[](const AmmoCategory& a, const AmmoCategory& b){ return a.order < b.order; });
		return touched > 0;
	}

	const AmmoCategory* Find(const std::string& id) const {
		for (const auto& a : ammo) if (a.id == id) return &a;
		return nullptr;
	}

	std::string Name(const std::string& id, const std::string& def = std::string()) const {
		const AmmoCategory* a = Find(id);
		return a ? a->name : def;
	}
	std::string Abbreviated(const std::string& id, const std::string& def = std::string()) const {
		const AmmoCategory* a = Find(id);
		return a ? a->name_abbreviated : def;
	}
	std::string QuantityText(const std::string& id, const std::string& def = std::string()) const {
		const AmmoCategory* a = Find(id);
		return a ? a->quantity_text : def;
	}
	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const AmmoCategory* a = Find(id);
		return a ? a->description : def;
	}

	// HUD ammo-counter text with the live count substituted for the "<Num>" token, e.g.
	// FormatQuantity("Rifle", 30) -> "30 rounds". If the category has no quantity template the
	// count is returned as a bare string; unknown ids return the caller default.
	std::string FormatQuantity(const std::string& id, int32_t count,
		const std::string& def = std::string()) const {
		const AmmoCategory* a = Find(id);
		if (!a) return def;
		const std::string num = std::to_string(count);
		const std::string& tmpl = a->quantity_text;
		const std::string token = "<Num>";
		size_t pos = tmpl.find(token);
		if (pos == std::string::npos) return tmpl.empty() ? num : tmpl;
		std::string out = tmpl;
		out.replace(pos, token.size(), num);
		return out;
	}

	int32_t Count() const { return (int32_t)ammo.size(); }

private:
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
