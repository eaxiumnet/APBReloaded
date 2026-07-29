#pragma once
// APB equipment-type catalog (mission toolkit item descriptions), extracted from the retail
// EquipmentTypes.INT (mirror of the cooked SDD table "EquipmentType") by
// tools/scripts/extract_equipment_types.ps1 -> Content/Data/equipment_types.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each equipment type carries a description of the mission toolkit item (battering ram,
// handcuffs, spray can, etc.) plus a parsed base id and mk tier (1-4). The base + mk encode the
// upgrade progression: Mk2/3/4 are faster/more effective variants of the base item.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct EquipmentTypeDef {
	std::string id;           // "Equipment_BatteringRam", "Equipment_BatteringRam_Mk2", ...
	std::string description;  // "A Battering Ram used to breach doors."
	std::string base;         // "Equipment_BatteringRam" (mk stripped)
	int32_t mk = 0;           // 0 (base), 2, 3, 4
	int32_t order = 0;
};

class EquipmentTypeCatalog {
public:
	std::vector<EquipmentTypeDef> items;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			EquipmentTypeDef e;
			e.id          = Unescape(RawStr(obj, "id"));
			e.description = Unescape(RawStr(obj, "description"));
			e.base        = Unescape(RawStr(obj, "base"));
			e.mk          = (int32_t)RNum(obj, "mk", 0);
			e.order       = (int32_t)RNum(obj, "order", 0);
			if (e.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const EquipmentTypeDef& x){ return x.id == e.id; });
			if (it == items.end()) items.push_back(e);
			else *it = e;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const EquipmentTypeDef& a, const EquipmentTypeDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const EquipmentTypeDef* Find(const std::string& id) const {
		for (const auto& e : items) if (e.id == id) return &e;
		return nullptr;
	}

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const EquipmentTypeDef* e = Find(id);
		return e ? e->description : def;
	}

	// All mk tiers for a base id (e.g. "Equipment_BatteringRam" -> base + Mk2/3/4).
	std::vector<const EquipmentTypeDef*> ForBase(const std::string& base) const {
		std::vector<const EquipmentTypeDef*> out;
		for (const auto& e : items) if (e.base == base) out.push_back(&e);
		return out;
	}

	// The base (mk=0) entry for a given id (strips _MkN suffix).
	const EquipmentTypeDef* FindBase(const std::string& id) const {
		const EquipmentTypeDef* e = Find(id);
		if (!e) return nullptr;
		// If already base, return it; else find the base sibling.
		if (e->mk == 0) return e;
		for (const auto& b : items) if (b.base == e->base && b.mk == 0) return &b;
		return nullptr;
	}

	// Distinct base ids, first-appearance order.
	std::vector<std::string> Bases() const {
		std::vector<std::string> out;
		for (const auto& e : items)
			if (std::find(out.begin(), out.end(), e.base) == out.end())
				out.push_back(e.base);
		return out;
	}

	// Count distinct equipment families (bases).
	size_t BaseCount() const {
		return Bases().size();
	}

	// True when the id is a mk-2+ upgraded variant.
	static bool IsUpgrade(const EquipmentTypeDef& e) { return e.mk >= 2; }

	size_t Count() const { return items.size(); }

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

