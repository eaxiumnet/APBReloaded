#pragma once
// APB WEAPON item-type catalog — the id -> player-facing DESCRIPTION dictionary the Armas marketplace /
// weapon-select / inventory UI shows as a weapon's flavour + role blurb (e.g. the HVR-243 sniper, the
// STAR assault rifle). Extracted from the retail WeaponItemTypes.INT (mirror of the cooked SDD table
// "WeaponItemTypes") by tools/scripts/extract_weapon_item_types.ps1 -> Content/Data/weapon_item_types.json.
//
// This is the missing DESCRIPTION leg of the weapon-info triple:
//   - weapons_catalog.json      (apbdb)              -> stats / ballistics
//   - weapon_display_names.json (InventoryItemTypes) -> id -> display name
//   - weapon_item_types.json    (THIS)               -> id -> rich description
// Of the ~936 weapon ids only ~839 carry a description (empty-description rows are dropped).
//
// Header-only (matches the other Domain catalogs): every method is defined in-class so it is implicitly
// inline and safe to include in multiple TUs.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct WeaponItemType {
	std::string id;          // "Weapon_SniperRifle_DMR", "Weapon_AssaultRifle_STAR_PR1", ...
	std::string description; // player-facing text (verbatim; embedded quotes survive; U+21B5 -> '\n')
	int32_t order = 0;       // stable display order (file order)
};

class WeaponItemTypeCatalog {
public:
	std::vector<WeaponItemType> items; // sorted by order

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
			WeaponItemType r;
			r.id          = Unescape(RawStr(obj, "id"));
			r.description = Unescape(RawStr(obj, "description"));
			r.order       = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const WeaponItemType& e){ return e.id == r.id; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const WeaponItemType& a, const WeaponItemType& b){ return a.order < b.order; });
		return touched > 0;
	}

	const WeaponItemType* Find(const std::string& id) const {
		for (const auto& r : items) if (r.id == id) return &r;
		return nullptr;
	}

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const WeaponItemType* r = Find(id);
		return r ? r->description : def;
	}

	// True if the weapon exists (every stored row has a non-empty description).
	bool HasDescription(const std::string& id) const {
		const WeaponItemType* r = Find(id);
		return r && !r->description.empty();
	}

	// All weapons whose id belongs to a family (first '_'-separated token), in display order.
	std::vector<const WeaponItemType*> ForCategory(const std::string& category) const {
		std::vector<const WeaponItemType*> out;
		for (const auto& r : items) if (Category(r.id) == category) out.push_back(&r);
		return out;
	}

	// All weapons of a weapon CLASS (second '_'-separated token: "SniperRifle", "AssaultRifle", ...),
	// in display order. For weapon ids the first token is always "Weapon", so the class token is the
	// discriminating one.
	std::vector<const WeaponItemType*> ForClass(const std::string& weaponClass) const {
		std::vector<const WeaponItemType*> out;
		for (const auto& r : items) if (Class(r.id) == weaponClass) out.push_back(&r);
		return out;
	}

	int32_t Count() const { return (int32_t)items.size(); }

	// The family token: the first '_'-separated segment ("Weapon_SniperRifle_DMR" -> "Weapon"). Ids with
	// no '_' return the id. Static.
	static std::string Category(const std::string& id) {
		size_t first = id.find('_');
		if (first == std::string::npos) return id;
		return id.substr(0, first);
	}

	// The weapon CLASS token: the second '_'-separated segment ("Weapon_SniperRifle_DMR" ->
	// "SniperRifle"). Ids with fewer than two segments return an empty string. Static.
	static std::string Class(const std::string& id) {
		size_t first = id.find('_');
		if (first == std::string::npos) return std::string();
		size_t second = id.find('_', first + 1);
		if (second == std::string::npos) return id.substr(first + 1);
		return id.substr(first + 1, second - first - 1);
	}

	std::string CategoryFor(const std::string& id) const { return Category(id); }

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
