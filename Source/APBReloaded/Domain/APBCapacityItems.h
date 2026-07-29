#pragma once
// APB capacity-item-type catalog (inventory capacity expansion descriptions), extracted from
// the retail CapacityItemTypes.INT (mirror of the cooked SDD table "CapacityItemType") by
// tools/scripts/extract_capacity_item_types.ps1 -> Content/Data/capacity_item_types.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each capacity item expands a specific inventory category's storage (clothing, outfit, songs,
// symbol, themes, vehicle, weapon, fnmod). The "amount" is numeric ("1","2","5","10") or "Max".
// Some items have an _Alt suffix (duplicate function, different unlock source).
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct CapacityItemTypeDef {
	std::string id;           // "Capacity_Clothing_1", "Capacity_Vehicle_Max", ...
	std::string description;  // "Increases Clothing Capacity by 1."
	std::string category;     // "Clothing", "Vehicle", "Symbol", ...
	std::string amount;       // "1", "2", "5", "10", "Max"
	bool is_max = false;      // true when amount == "Max"
	bool is_alt = false;      // true when id has _Alt suffix
	int32_t order = 0;
};

class CapacityItemCatalog {
public:
	std::vector<CapacityItemTypeDef> items;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			CapacityItemTypeDef c;
			c.id          = Unescape(RawStr(obj, "id"));
			c.description = Unescape(RawStr(obj, "description"));
			c.category    = Unescape(RawStr(obj, "category"));
			c.amount      = Unescape(RawStr(obj, "amount"));
			c.is_max      = RBool(obj, "is_max");
			c.is_alt      = RBool(obj, "is_alt");
			c.order       = (int32_t)RNum(obj, "order", 0);
			if (c.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const CapacityItemTypeDef& x){ return x.id == c.id; });
			if (it == items.end()) items.push_back(c);
			else *it = c;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const CapacityItemTypeDef& a, const CapacityItemTypeDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const CapacityItemTypeDef* Find(const std::string& id) const {
		for (const auto& c : items) if (c.id == id) return &c;
		return nullptr;
	}

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const CapacityItemTypeDef* c = Find(id);
		return c ? c->description : def;
	}

	// All capacity items for a given category (e.g. "Clothing" -> +1/+2/+5/+10/+15/Max).
	std::vector<const CapacityItemTypeDef*> ForCategory(const std::string& cat) const {
		std::vector<const CapacityItemTypeDef*> out;
		for (const auto& c : items) if (c.category == cat) out.push_back(&c);
		return out;
	}

	// Distinct category list, first-appearance order.
	std::vector<std::string> Categories() const {
		std::vector<std::string> out;
		for (const auto& c : items)
			if (std::find(out.begin(), out.end(), c.category) == out.end())
				out.push_back(c.category);
		return out;
	}

	// Find the "Max" capacity item for a category (the unlock-cap upgrade).
	const CapacityItemTypeDef* FindMax(const std::string& cat) const {
		for (const auto& c : items) if (c.category == cat && c.is_max) return &c;
		return nullptr;
	}

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

	static bool RBool(const std::string& obj, const std::string& key) {
		const std::string needle = "\"" + key + "\"";
		size_t k = obj.find(needle);
		if (k == std::string::npos) return false;
		size_t colon = obj.find(':', k + needle.size());
		if (colon == std::string::npos) return false;
		size_t i = colon + 1;
		while (i < obj.size() && (obj[i] == ' ' || obj[i] == '\t')) ++i;
		if (i >= obj.size()) return false;
		return obj[i] == 't';
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

