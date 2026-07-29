#pragma once
// APB DAILY-ACTIVITY CONTACT catalog — the retail text for "daily activities", the small "do X today"
// objectives a player picks up from a contact each day (e.g. "Blow up 3 enemy vehicles"). Each activity id
// carries three text fields and, for many activities, several randomised flavour VARIANTS of that text (the
// game rotates them so the same objective reads differently day to day / faction to faction):
//   Title             short punny name shown in the daily list ("Casamajor Car-nage", "Vroom Vroom Boom")
//   HUDDescription    terse HUD line ("Blow up <col: Yellow>3</col> enemy vehicles.")
//   LongDescription   the contact's flavour brief
// Extracted from the retail DailyActivityContacts.INT (mirror of the cooked SDD table "DailyActivityContacts")
// by tools/scripts/extract_daily_activity_contacts.ps1 -> Content/Data/daily_activity_contacts.json, FLATTENED
// to one row per (id, variant): 85 activities -> 133 rows (44 have a 2nd variant, 3 a 3rd, 1 a 4th). Variant 1
// is the unnumbered retail key.
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

struct DailyActivityEntry {
	std::string id;                // "DestroyEnemyVehicles", "Mission_CompleteObjectives", ...
	int32_t variant = 1;           // 1-based flavour variant (1 = unnumbered retail key)
	std::string title;             // short list name
	std::string hud_description;   // terse HUD line (may carry <col:> markup)
	std::string long_description;  // contact flavour brief
	int32_t order = 0;             // stable display order (file order, grouped by id then variant)
};

class DailyActivityContactCatalog {
public:
	std::vector<DailyActivityEntry> items; // sorted by order

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge keyed by (id, variant): an existing (id,variant) is updated in place; a new one is
	// appended. Never clears on empty input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			DailyActivityEntry r;
			r.id               = Unescape(RawStr(obj, "id"));
			r.variant          = (int32_t)RNum(obj, "variant", 1);
			r.title            = Unescape(RawStr(obj, "title"));
			r.hud_description  = Unescape(RawStr(obj, "hud_description"));
			r.long_description = Unescape(RawStr(obj, "long_description"));
			r.order            = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const DailyActivityEntry& e){ return e.id == r.id && e.variant == r.variant; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const DailyActivityEntry& a, const DailyActivityEntry& b){ return a.order < b.order; });
		return touched > 0;
	}

	// A specific (id, variant). Returns nullptr if absent. variant is 1-based.
	const DailyActivityEntry* Find(const std::string& id, int32_t variant = 1) const {
		for (const auto& r : items) if (r.id == id && r.variant == variant) return &r;
		return nullptr;
	}

	// All variants of an activity, ordered by variant number. Empty if the id is unknown.
	std::vector<const DailyActivityEntry*> Variants(const std::string& id) const {
		std::vector<const DailyActivityEntry*> out;
		for (const auto& r : items) if (r.id == id) out.push_back(&r);
		std::sort(out.begin(), out.end(),
			[](const DailyActivityEntry* a, const DailyActivityEntry* b){ return a->variant < b->variant; });
		return out;
	}

	// Number of flavour variants an activity ships (0 if unknown).
	int32_t VariantCount(const std::string& id) const {
		int32_t n = 0;
		for (const auto& r : items) if (r.id == id) ++n;
		return n;
	}

	std::string Title(const std::string& id, int32_t variant = 1, const std::string& def = std::string()) const {
		const DailyActivityEntry* r = Find(id, variant);
		return (r && !r->title.empty()) ? r->title : def;
	}
	std::string HUDDescription(const std::string& id, int32_t variant = 1, const std::string& def = std::string()) const {
		const DailyActivityEntry* r = Find(id, variant);
		return (r && !r->hud_description.empty()) ? r->hud_description : def;
	}
	std::string LongDescription(const std::string& id, int32_t variant = 1, const std::string& def = std::string()) const {
		const DailyActivityEntry* r = Find(id, variant);
		return (r && !r->long_description.empty()) ? r->long_description : def;
	}

	// Total rows across all activities and variants.
	int32_t Count() const { return (int32_t)items.size(); }

	// Distinct activity ids (regardless of variant count).
	int32_t ActivityCount() const {
		std::vector<std::string> seen;
		for (const auto& r : items)
			if (std::find(seen.begin(), seen.end(), r.id) == seen.end()) seen.push_back(r.id);
		return (int32_t)seen.size();
	}

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
