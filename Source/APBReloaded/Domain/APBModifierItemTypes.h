#pragma once
// APB modification-ITEM catalog (the purchasable / equippable mod items you buy on Armas and slot
// on a character / vehicle / weapon), extracted from the retail ModifierItemTypes.INT (mirror of
// the cooked SDD table "ModifierItemType") by tools/scripts/extract_modifier_item_types.ps1 ->
// Content/Data/modifier_item_types.json.
//
// Header-only (matches MedalCatalog / ModifierEffectCatalog / HUDCombatMessageCatalog): every method
// is defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// Each item carries a short "type label" (e.g. "Health Modification", "Activated Modification") and
// a flavour description, split in the INT by the U+21B5 line-break glyph. The item's stat effect
// (the coloured "+20% stored ammo" tooltip) lives in the separate ModifierEffects catalog; an item
// id maps to a modifier-effect id via EffectId(): strip the "FnMod_"/"FNMod_" prefix and any
// trailing "_Tutorial". Not every item binds (empty-slot placeholders, deployable sub-effects, and
// a few renamed variants have no direct effect row), so callers should Find() the derived id and
// tolerate a miss.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct ModifierItemType {
	std::string id;           // "FnMod_Character_Kevlar2", "FnMod_Vehicle_Nitro3", "Mod_None", ...
	std::string category;     // "Character" / "Vehicle" / "Weapon" / "Special"
	std::string type_label;   // "Health Modification" / "Activated Modification" / ... ("" if none)
	std::string description;  // flavour text
	int32_t order = 0;        // stable display order (file order)
};

class ModifierItemTypeCatalog {
public:
	std::vector<ModifierItemType> items; // sorted by order

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
			ModifierItemType it;
			it.id          = Unescape(RawStr(obj, "id"));
			it.category    = Unescape(RawStr(obj, "category"));
			it.type_label  = Unescape(RawStr(obj, "type_label"));
			it.description = Unescape(RawStr(obj, "description"));
			it.order       = (int32_t)RNum(obj, "order", 0);
			if (it.id.empty()) continue;
			auto existing = std::find_if(items.begin(), items.end(),
				[&](const ModifierItemType& x){ return x.id == it.id; });
			if (existing == items.end()) items.push_back(it);
			else *existing = it;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const ModifierItemType& a, const ModifierItemType& b){ return a.order < b.order; });
		return touched > 0;
	}

	const ModifierItemType* Find(const std::string& id) const {
		for (const auto& it : items) if (it.id == id) return &it;
		return nullptr;
	}

	std::string TypeLabel(const std::string& id, const std::string& def = std::string()) const {
		const ModifierItemType* it = Find(id);
		return it ? it->type_label : def;
	}
	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const ModifierItemType* it = Find(id);
		return it ? it->description : def;
	}
	std::string Category(const std::string& id, const std::string& def = std::string()) const {
		const ModifierItemType* it = Find(id);
		return it ? it->category : def;
	}

	// All items of a category ("Character"/"Vehicle"/"Weapon"/"Special"), in display order.
	std::vector<const ModifierItemType*> ForCategory(const std::string& category) const {
		std::vector<const ModifierItemType*> out;
		for (const auto& it : items) if (it.category == category) out.push_back(&it);
		return out;
	}

	// Distinct categories present, sorted.
	std::vector<std::string> Categories() const {
		std::vector<std::string> out;
		for (const auto& it : items)
			if (std::find(out.begin(), out.end(), it.category) == out.end()) out.push_back(it.category);
		std::sort(out.begin(), out.end());
		return out;
	}

	int32_t Count() const { return (int32_t)items.size(); }

	// The ModifierEffects id an item maps to: strip a leading "FnMod_"/"FNMod_" and a trailing
	// "_Tutorial". e.g. "FnMod_Vehicle_Explosives1" -> "Vehicle_Explosives1",
	// "FnMod_Weapon_Rifling3_Tutorial" -> "Weapon_Rifling3". Placeholders like "Mod_None" pass
	// through unchanged (they have no effect row). Static: usable without a catalog instance.
	static std::string EffectId(const std::string& id) {
		std::string s = id;
		if (StartsWith(s, "FnMod_"))      s = s.substr(6);
		else if (StartsWith(s, "FNMod_")) s = s.substr(6);
		const std::string tut = "_Tutorial";
		if (s.size() > tut.size() && s.compare(s.size() - tut.size(), tut.size(), tut) == 0)
			s = s.substr(0, s.size() - tut.size());
		return s;
	}

	// Convenience: the effect id for a stored item (empty if unknown item).
	std::string EffectIdFor(const std::string& id) const {
		const ModifierItemType* it = Find(id);
		return it ? EffectId(it->id) : std::string();
	}

private:
	static bool StartsWith(const std::string& s, const char* p) {
		size_t n = 0; while (p[n]) ++n;
		return s.size() >= n && s.compare(0, n, p) == 0;
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

	// Returns the RAW (still-escaped) string value for "key" within a single object.
	static std::string RawStr(const std::string& obj, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return std::string();
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return std::string();
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t' || obj[i] == '\n' || obj[i] == '\r')) ++i;
		if (i >= obj.size() || obj[i] != '"') return std::string();
		return ReadStringAt(obj, i);
	}

	// Reads a JSON string starting at the opening quote index; returns the raw (escaped) contents.
	static std::string ReadStringAt(const std::string& obj, size_t quotePos) {
		std::string raw; bool esc = false;
		for (size_t i = quotePos + 1; i < obj.size(); ++i) {
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
