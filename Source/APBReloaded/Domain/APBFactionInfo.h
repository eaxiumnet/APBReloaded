#pragma once
// APB faction-selection screen content (display names + faction-info lore), extracted
// from the retail Factions.INT (mirror of the cooked SDD table "Faction") by
// tools/scripts/extract_factions.ps1 -> Content/Data/factions.json.
//
// Header-only (matches ThreatRatingCatalog in APBThreat.h): every method is defined
// in-class so it is implicitly inline and safe to include in multiple TUs.
//
// Unlike the other Domain catalogs (which use a naive value scanner that collapses
// "\uXXXX" / "\n"), this one carries a PROPER JSON string unescaper so the multi-
// paragraph lore (paragraph breaks stored as "\n\n") round-trips 1:1 for the faction
// selection / info screen.
#include "APBTypes.h"   // Faction enum
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cctype>
#include <algorithm>

namespace apb {

struct FactionInfo {
	std::string id;               // "None", "Enforcer", "Criminal"
	std::string display_name;     // "None" / "Enforcer" / "Criminal"
	std::string info_title;       // "General Info" / "Enforcer" / "Criminal"
	std::string info_description; // multi-paragraph lore ("\n\n" between paragraphs)
	int32_t rank = 0;             // 0 = General Info, 1 = Enforcer, 2 = Criminal
};

class FactionInfoCatalog {
public:
	std::vector<FactionInfo> factions; // sorted by rank

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::stringstream ss; ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	// Additive/merge: existing ids are updated in place; new ids appended. Never
	// clears on empty input. Returns true if at least one row was added or updated.
	bool LoadFromJsonText(const std::string& text) {
		int32_t touched = 0;
		for (const std::string& obj : SplitTopObjects(text)) {
			FactionInfo fi;
			fi.id               = Unescape(RawStr(obj, "id"));
			fi.display_name     = Unescape(RawStr(obj, "display_name"));
			fi.info_title       = Unescape(RawStr(obj, "info_title"));
			fi.info_description = Unescape(RawStr(obj, "info_description"));
			fi.rank             = (int32_t)RNum(obj, "rank", 0);
			if (fi.id.empty()) continue;
			auto it = std::find_if(factions.begin(), factions.end(),
				[&](const FactionInfo& f){ return f.id == fi.id; });
			if (it == factions.end()) factions.push_back(fi);
			else *it = fi;
			++touched;
		}
		std::sort(factions.begin(), factions.end(),
			[](const FactionInfo& a, const FactionInfo& b){ return a.rank < b.rank; });
		return touched > 0;
	}

	const FactionInfo* Find(const std::string& id) const {
		for (const auto& f : factions) if (f.id == id) return &f;
		return nullptr;
	}
	// The Faction enum maps onto the retail ids "Enforcer"/"Criminal".
	const FactionInfo* ForFaction(Faction f) const {
		return Find(f == Faction::Enforcer ? "Enforcer" : "Criminal");
	}
	// The "None" row carries the shared San Paro city lore shown as "General Info".
	const FactionInfo* GeneralInfo() const { return Find("None"); }

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const FactionInfo* f = Find(id);
		return f ? f->info_description : def;
	}
	// Number of "\n\n"-separated paragraphs in a faction's lore (0 if unknown).
	int32_t ParagraphCount(const std::string& id) const {
		const FactionInfo* f = Find(id);
		if (!f || f->info_description.empty()) return 0;
		int32_t n = 1;
		const std::string& s = f->info_description;
		for (size_t i = 0; i + 1 < s.size(); ++i)
			if (s[i] == '\n' && s[i + 1] == '\n') { ++n; ++i; }
		return n;
	}
	int32_t Count() const { return (int32_t)factions.size(); }

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
