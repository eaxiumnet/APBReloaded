#pragma once
// APB TASK-OPERATION UI-PROFILE catalog — the retail HUD "tracked value" labels for each mission-operation
// type. Every mission stage runs an "operation" (AntiGraffiti, ArmedGuard, BombDisposal, Escape120, ...) whose
// id shares the TaskOperation id space 1:1; that operation's UI profile carries a fixed 4-slot array of short
// labels the mission HUD shows beside each tracked counter for the stage:
//   TrackedValueDescription[0..3]   slot 0 the primary ("Cover Graffiti:", "Guard Targets:"), slots 1..3 for
//                                   multi-counter operations ("Bombs Armed:" / "Bombs Disarmed:")
// Extracted from the retail TaskOperationUIProfile.INT (mirror of the cooked SDD table "TaskOperationUIProfile")
// by tools/scripts/extract_task_operation_ui_profiles.ps1 -> Content/Data/task_operation_ui_profiles.json,
// FLATTENED to one row per profile with a fixed desc0..desc3 quartet. 193 raw profiles -> 178 rows (15 all-empty
// placeholders such as "None"/"Simple" dropped). This is the per-tracked-value companion to the task_operations
// catalog (MissionOperationCatalog), which holds the single UIDescription per operation.
//
// Values embed <col: ...> markup (resolved to a text colour by the HUD) and are preserved VERBATIM for 1:1
// rendering; RTW's U+21B5 in-string line break is normalised to '\n'.
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

struct TaskOperationUIProfileEntry {
	std::string id;        // operation id, shares the TaskOperation id space ("AntiGraffiti", "BombDisposal", ...)
	std::string desc[4];   // TrackedValueDescription[0..3]; slot 0 primary, later slots for multi-counter ops
	int32_t order = 0;     // stable display order (file order)
};

class TaskOperationUIProfileCatalog {
public:
	std::vector<TaskOperationUIProfileEntry> items; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge keyed by id: an existing id is updated in place; a new one is appended. Never clears
	// on empty input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			TaskOperationUIProfileEntry r;
			r.id      = Unescape(RawStr(obj, "id"));
			r.desc[0] = Unescape(RawStr(obj, "desc0"));
			r.desc[1] = Unescape(RawStr(obj, "desc1"));
			r.desc[2] = Unescape(RawStr(obj, "desc2"));
			r.desc[3] = Unescape(RawStr(obj, "desc3"));
			r.order   = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const TaskOperationUIProfileEntry& e){ return e.id == r.id; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const TaskOperationUIProfileEntry& a, const TaskOperationUIProfileEntry& b){ return a.order < b.order; });
		return touched > 0;
	}

	// A profile by operation id. Returns nullptr if absent.
	const TaskOperationUIProfileEntry* Find(const std::string& id) const {
		for (const auto& r : items) if (r.id == id) return &r;
		return nullptr;
	}

	// The tracked-value label at slot index (0..3) for an operation id, or def when the id/slot is absent
	// or empty. Out-of-range indices return def.
	std::string TrackedValueDescription(const std::string& id, int32_t index, const std::string& def = std::string()) const {
		if (index < 0 || index > 3) return def;
		const TaskOperationUIProfileEntry* r = Find(id);
		return (r && !r->desc[index].empty()) ? r->desc[index] : def;
	}

	// The primary tracked-value label (slot 0), or def.
	std::string PrimaryDescription(const std::string& id, const std::string& def = std::string()) const {
		return TrackedValueDescription(id, 0, def);
	}

	// Number of non-empty tracked-value slots this operation displays (0 if the id is unknown).
	int32_t TrackedValueCount(const std::string& id) const {
		const TaskOperationUIProfileEntry* r = Find(id);
		if (!r) return 0;
		int32_t n = 0;
		for (int i = 0; i < 4; ++i) if (!r->desc[i].empty()) ++n;
		return n;
	}

	// All non-empty tracked-value labels for an operation id, in slot order. Empty if the id is unknown.
	std::vector<std::string> Descriptions(const std::string& id) const {
		std::vector<std::string> out;
		const TaskOperationUIProfileEntry* r = Find(id);
		if (!r) return out;
		for (int i = 0; i < 4; ++i) if (!r->desc[i].empty()) out.push_back(r->desc[i]);
		return out;
	}

	// Total profiles (rows that render at least one label).
	int32_t Count() const { return (int32_t)items.size(); }

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
