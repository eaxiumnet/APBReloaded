#pragma once
// APB gameplay-object label catalog (context-sensitive HUD interaction labels), extracted from
// the retail GameplayObjects.INT (mirror of the cooked SDD table "GameplayObject") by
// tools/scripts/extract_gameplay_objects.ps1 -> Content/Data/gameplay_objects.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each gameplay object carries a description (the HUD label shown when interacting with or
// targeting a world object) and a category (first id token: Prop, Vehicle, TaskItem,
// DisplayPoint, PlayerCharacter, Checkpoint, Graffiti, Pedestrian).
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct GameplayObjectDef {
	std::string id;          // "Prop_Bench", "Vehicle_Ambient_Taxi", "PlayerCharacter_Criminal", ...
	std::string description;  // "Bench", "Taxi", "Criminal", ...
	std::string category;     // "Prop", "Vehicle", "TaskItem", "DisplayPoint", ...
	int32_t order = 0;
};

class GameplayObjectCatalog {
public:
	std::vector<GameplayObjectDef> objects;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			GameplayObjectDef g;
			g.id          = Unescape(RawStr(obj, "id"));
			g.description = Unescape(RawStr(obj, "description"));
			g.category   = Unescape(RawStr(obj, "category"));
			g.order      = (int32_t)RNum(obj, "order", 0);
			if (g.id.empty()) continue;
			auto it = std::find_if(objects.begin(), objects.end(),
				[&](const GameplayObjectDef& e){ return e.id == g.id; });
			if (it == objects.end()) objects.push_back(g);
			else *it = g;
			++touched;
		}
		std::sort(objects.begin(), objects.end(),
			[](const GameplayObjectDef& a, const GameplayObjectDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const GameplayObjectDef* Find(const std::string& id) const {
		for (const auto& g : objects) if (g.id == id) return &g;
		return nullptr;
	}

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const GameplayObjectDef* g = Find(id);
		return g ? g->description : def;
	}
	std::string Category(const std::string& id, const std::string& def = std::string()) const {
		const GameplayObjectDef* g = Find(id);
		return g ? g->category : def;
	}

	// All objects in a given category (e.g. "Prop", "Vehicle").
	std::vector<const GameplayObjectDef*> ForCategory(const std::string& cat) const {
		std::vector<const GameplayObjectDef*> out;
		for (const auto& g : objects) if (g.category == cat) out.push_back(&g);
		return out;
	}

	// Distinct category list, first-appearance order.
	std::vector<std::string> Categories() const {
		std::vector<std::string> out;
		for (const auto& g : objects)
			if (std::find(out.begin(), out.end(), g.category) == out.end())
				out.push_back(g.category);
		return out;
	}

	// True when the id is a prop (category == "Prop").
	static bool IsProp(const GameplayObjectDef& g) { return g.category == "Prop"; }
	// True when the id is a vehicle (category == "Vehicle").
	static bool IsVehicle(const GameplayObjectDef& g) { return g.category == "Vehicle"; }
	// True when the id is an ambient vehicle (id starts with "Vehicle_Ambient_").
	static bool IsAmbientVehicle(const GameplayObjectDef& g) {
		return g.id.compare(0, 16, "Vehicle_Ambient_") == 0;
	}
	// True when the id is a player character (category == "PlayerCharacter").
	static bool IsPlayerCharacter(const GameplayObjectDef& g) { return g.category == "PlayerCharacter"; }

	size_t Count() const { return objects.size(); }

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

