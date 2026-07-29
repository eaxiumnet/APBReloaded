#pragma once
// APB mission-target-type catalog (display names for mission objectives), extracted from
// the retail TaskTargetTypes.INT (mirror of the cooked SDD table "TaskTargetType") by
// tools/scripts/extract_task_target_types.ps1 -> Content/Data/task_target_types.json.
//
// Header-only (matches AmmoCategoryCatalog / MedalCatalog): every method is defined in-class so
// it is implicitly inline and safe to include in multiple TUs.
//
// Each target type carries a display name shown when the player approaches a mission objective
// ("Graffiti Point", "ATM", "Checkpoint", "Pedestrian", etc.). Multiple ids may share the same
// display name (e.g. all Checkpoint variants show "Checkpoint"); the mission system resolves
// by id, not by display name.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct TaskTargetTypeDef {
	std::string id;            // "BankMachine", "Graffiti_Default", "Checkpoint_Race", ...
	std::string display_name;  // "ATM", "Graffiti Point", "Checkpoint", ...
	int32_t order = 0;
};

class TaskTargetTypeCatalog {
public:
	std::vector<TaskTargetTypeDef> targets;

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			TaskTargetTypeDef t;
			t.id           = Unescape(RawStr(obj, "id"));
			t.display_name = Unescape(RawStr(obj, "display_name"));
			t.order        = (int32_t)RNum(obj, "order", 0);
			if (t.id.empty()) continue;
			auto it = std::find_if(targets.begin(), targets.end(),
				[&](const TaskTargetTypeDef& e){ return e.id == t.id; });
			if (it == targets.end()) targets.push_back(t);
			else *it = t;
			++touched;
		}
		std::sort(targets.begin(), targets.end(),
			[](const TaskTargetTypeDef& a, const TaskTargetTypeDef& b){ return a.order < b.order; });
		return touched > 0;
	}

	const TaskTargetTypeDef* Find(const std::string& id) const {
		for (const auto& t : targets) if (t.id == id) return &t;
		return nullptr;
	}

	std::string DisplayName(const std::string& id, const std::string& def = std::string()) const {
		const TaskTargetTypeDef* t = Find(id);
		return t ? t->display_name : def;
	}

	// All target types that share a given display name (e.g. all "Checkpoint" variants).
	std::vector<const TaskTargetTypeDef*> ByDisplayName(const std::string& name) const {
		std::vector<const TaskTargetTypeDef*> out;
		for (const auto& t : targets) if (t.display_name == name) out.push_back(&t);
		return out;
	}

	// Distinct display names, first-appearance order.
	std::vector<std::string> DistinctDisplayNames() const {
		std::vector<std::string> out;
		for (const auto& t : targets)
			if (std::find(out.begin(), out.end(), t.display_name) == out.end())
				out.push_back(t.display_name);
		return out;
	}

	// True when the id is an NPC target (starts with "NPC_").
	static bool IsNPCTarget(const TaskTargetTypeDef& t) {
		return t.id.compare(0, 4, "NPC_") == 0;
	}

	// True when the id is a checkpoint variant (contains "Checkpoint").
	static bool IsCheckpoint(const TaskTargetTypeDef& t) {
		return t.id.find("Checkpoint") != std::string::npos;
	}

	size_t Count() const { return targets.size(); }

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

