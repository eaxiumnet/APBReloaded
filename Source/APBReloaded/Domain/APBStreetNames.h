#pragma once
// APB street-name catalog (world-map / minimap location labels and mission waypoint
// callouts), extracted from the retail StreetName.INT (mirror of the cooked SDD table
// "StreetName") by tools/scripts/extract_street_names.ps1 -> Content/Data/street_names.json.
//
// Header-only (matches FactionInfoCatalog / OrganisationCatalog / MedalCatalog): every method
// is defined in-class so it is implicitly inline and safe to include in multiple TUs.
//
// Each entry carries the district it belongs to ("Financial" / "Waterfront") and a kind:
// "street" for a single named road, "intersection" for a junction label (key contained "_X_").
// Names are verbatim from the INT (accents like "Malaga" and "&" join labels preserved 1:1).
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <algorithm>

namespace apb {

struct StreetName {
	std::string id;       // key, e.g. "FinancialShianxi", "Waterfront_X_HaeinsaAbrams"
	std::string name;     // displayed label, e.g. "Shianxi Boulevard", "Bank & Breakwater"
	std::string district; // "Financial" / "Waterfront"
	std::string kind;     // "street" / "intersection"
	int32_t order = 0;    // stable display order (file order)

	bool IsIntersection() const { return kind == "intersection"; }
};

class StreetNameCatalog {
public:
	std::vector<StreetName> streets; // sorted by order

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
			StreetName s;
			s.id       = Unescape(RawStr(obj, "id"));
			s.name     = Unescape(RawStr(obj, "name"));
			s.district = Unescape(RawStr(obj, "district"));
			s.kind     = Unescape(RawStr(obj, "kind"));
			s.order    = (int32_t)RNum(obj, "order", 0);
			if (s.id.empty()) continue;
			auto it = std::find_if(streets.begin(), streets.end(),
				[&](const StreetName& e){ return e.id == s.id; });
			if (it == streets.end()) streets.push_back(s);
			else *it = s;
			++touched;
		}
		std::sort(streets.begin(), streets.end(),
			[](const StreetName& a, const StreetName& b){ return a.order < b.order; });
		return touched > 0;
	}

	const StreetName* Find(const std::string& id) const {
		for (const auto& s : streets) if (s.id == id) return &s;
		return nullptr;
	}

	std::string Name(const std::string& id, const std::string& def = std::string()) const {
		const StreetName* s = Find(id);
		return s ? s->name : def;
	}

	// All streets in a district ("Financial"/"Waterfront"), preserving order.
	std::vector<const StreetName*> ForDistrict(const std::string& district) const {
		std::vector<const StreetName*> out;
		for (const auto& s : streets) if (s.district == district) out.push_back(&s);
		return out;
	}
	// All streets of a kind ("street"/"intersection"), preserving order.
	std::vector<const StreetName*> OfKind(const std::string& kind) const {
		std::vector<const StreetName*> out;
		for (const auto& s : streets) if (s.kind == kind) out.push_back(&s);
		return out;
	}
	// Distinct districts, in first-seen (display) order.
	std::vector<std::string> Districts() const {
		std::vector<std::string> out;
		for (const auto& s : streets)
			if (std::find(out.begin(), out.end(), s.district) == out.end()) out.push_back(s.district);
		return out;
	}

	int32_t Count() const { return (int32_t)streets.size(); }
	int32_t CountForDistrict(const std::string& district) const {
		int32_t n = 0; for (const auto& s : streets) if (s.district == district) ++n; return n;
	}
	int32_t CountOfKind(const std::string& kind) const {
		int32_t n = 0; for (const auto& s : streets) if (s.kind == kind) ++n; return n;
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
