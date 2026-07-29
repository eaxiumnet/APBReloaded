#pragma once
// APB organisation catalog (contact orgs, weapon vendors, store fronts), extracted
// from the retail Organisations.INT (mirror of the cooked SDD table "Organisation") by
// tools/scripts/extract_organisations.ps1 -> Content/Data/organisations.json.
//
// Header-only (matches FactionInfoCatalog / ThreatRatingCatalog): every method is
// defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// The org display "name" is taken verbatim from the INT. The "faction" affiliation and
// "kind" are canonical APB classification (the SDD Organisation.Faction column is cooked
// away and absent from the INT); see work/m15_organisations_note.md for provenance. This
// is the authoritative list the Armas store filters and the contact UI group by.
#include "APBTypes.h"   // Faction enum
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct Organisation {
	std::string id;       // "GKings", "Praetorian", "JokerDistribution", ...
	std::string name;     // "G-Kings", "Praetorians", "Joker Distribution", ...
	std::string faction;  // "Criminal" / "Enforcer" / "None" (neutral store/vendor/tutorial)
	std::string kind;     // "gang" / "default" / "seasonal" / "vendor" / "store" / "tutorial" / "none"
	int32_t rank = 0;     // stable display order (file order)
};

class OrganisationCatalog {
public:
	std::vector<Organisation> organisations; // sorted by rank

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
			Organisation o;
			o.id      = Unescape(RawStr(obj, "id"));
			o.name    = Unescape(RawStr(obj, "name"));
			o.faction = Unescape(RawStr(obj, "faction"));
			o.kind    = Unescape(RawStr(obj, "kind"));
			o.rank    = (int32_t)RNum(obj, "rank", 0);
			if (o.id.empty()) continue;
			auto it = std::find_if(organisations.begin(), organisations.end(),
				[&](const Organisation& e){ return e.id == o.id; });
			if (it == organisations.end()) organisations.push_back(o);
			else *it = o;
			++touched;
		}
		std::sort(organisations.begin(), organisations.end(),
			[](const Organisation& a, const Organisation& b){ return a.rank < b.rank; });
		return touched > 0;
	}

	const Organisation* Find(const std::string& id) const {
		for (const auto& o : organisations) if (o.id == id) return &o;
		return nullptr;
	}

	std::string Name(const std::string& id, const std::string& def = std::string()) const {
		const Organisation* o = Find(id);
		return o ? o->name : def;
	}

	// The Faction enum maps onto the string affiliation stored in the catalog.
	static const char* FactionKey(Faction f) {
		return f == Faction::Enforcer ? "Enforcer" : "Criminal";
	}

	// All organisations affiliated to a faction string ("Criminal"/"Enforcer"/"None"),
	// preserving rank order. Neutral store/vendor/tutorial orgs carry faction "None".
	std::vector<const Organisation*> ForFaction(const std::string& faction) const {
		std::vector<const Organisation*> out;
		for (const auto& o : organisations) if (o.faction == faction) out.push_back(&o);
		return out;
	}
	std::vector<const Organisation*> ForFaction(Faction f) const {
		return ForFaction(FactionKey(f));
	}

	// All organisations of a given kind ("gang"/"vendor"/"store"/...), rank order.
	std::vector<const Organisation*> OfKind(const std::string& kind) const {
		std::vector<const Organisation*> out;
		for (const auto& o : organisations) if (o.kind == kind) out.push_back(&o);
		return out;
	}

	int32_t Count() const { return (int32_t)organisations.size(); }
	int32_t CountForFaction(const std::string& faction) const {
		int32_t n = 0; for (const auto& o : organisations) if (o.faction == faction) ++n; return n;
	}
	int32_t CountOfKind(const std::string& kind) const {
		int32_t n = 0; for (const auto& o : organisations) if (o.kind == kind) ++n; return n;
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
