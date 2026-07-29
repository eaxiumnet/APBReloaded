#pragma once
// APB REWARD-PACKAGE ITEM-TYPE text catalog — the retail id -> player-facing prose for a "reward package"
// (a bundle granted through progression, Armas, the Joker Distribution or an event: an outfit, a vehicle-
// component kit, a weapon package, ...). Each package carries up to three text fields the UI/mail system
// renders when the package is described or delivered:
//   Description   store/inventory blurb explaining what the package grants
//   MailSubject   subject line of the in-game mail that delivers the package
//   MailBody      body of that mail
// This is the TEXT companion to the reward_packages catalog (APBRewardPackages.h): reward_packages holds the
// package -> item mapping / metadata; this holds the prose. Extracted from the retail RewardPackageItemTypes.INT
// (mirror of the cooked SDD table "RewardPackageItemTypes") by
// tools/scripts/extract_reward_package_item_types.ps1 -> Content/Data/reward_package_item_types.json.
//
// MailSubject strings embed literal double-quotes (e.g. Joker Distribution: "Asylum" Outfit!) and any
// <col: ...> markup is preserved VERBATIM for 1:1 rendering; RTW's U+21B5 in-string line break is normalised
// to '\n'. Fields are often empty (most packages only carry a Description); an id is kept if ANY of its three
// fields has text (139).
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

struct RewardPackageItemTypeEntry {
	std::string id;            // "RewardPackage_Outfit_CSASting_Male", "RewardPackage_Components_Espacio_Kit1", ...
	std::string description;   // store/inventory blurb
	std::string mail_subject;  // in-game mail subject line
	std::string mail_body;     // in-game mail body
	int32_t order = 0;         // stable display order (file order)
};

class RewardPackageItemTypeCatalog {
public:
	std::vector<RewardPackageItemTypeEntry> items; // sorted by order

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
			RewardPackageItemTypeEntry r;
			r.id           = Unescape(RawStr(obj, "id"));
			r.description  = Unescape(RawStr(obj, "description"));
			r.mail_subject = Unescape(RawStr(obj, "mail_subject"));
			r.mail_body    = Unescape(RawStr(obj, "mail_body"));
			r.order        = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const RewardPackageItemTypeEntry& e){ return e.id == r.id; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const RewardPackageItemTypeEntry& a, const RewardPackageItemTypeEntry& b){ return a.order < b.order; });
		return touched > 0;
	}

	const RewardPackageItemTypeEntry* Find(const std::string& id) const {
		for (const auto& r : items) if (r.id == id) return &r;
		return nullptr;
	}

	// The store/inventory blurb for a package. Returns def if the package or field is absent.
	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const RewardPackageItemTypeEntry* r = Find(id);
		return (r && !r->description.empty()) ? r->description : def;
	}

	// The mail subject line for a package. Returns def if the package or field is absent.
	std::string MailSubject(const std::string& id, const std::string& def = std::string()) const {
		const RewardPackageItemTypeEntry* r = Find(id);
		return (r && !r->mail_subject.empty()) ? r->mail_subject : def;
	}

	// The mail body for a package. Returns def if the package or field is absent.
	std::string MailBody(const std::string& id, const std::string& def = std::string()) const {
		const RewardPackageItemTypeEntry* r = Find(id);
		return (r && !r->mail_body.empty()) ? r->mail_body : def;
	}

	// All packages whose category (see Category) matches. Preserves order.
	std::vector<const RewardPackageItemTypeEntry*> ForCategory(const std::string& category) const {
		std::vector<const RewardPackageItemTypeEntry*> out;
		for (const auto& r : items) if (Category(r.id) == category) out.push_back(&r);
		return out;
	}

	int32_t Count() const { return (int32_t)items.size(); }

	// The package category: the SECOND '_'-separated segment. Every id is prefixed "RewardPackage_", so the
	// meaningful grouping is the token after it ("RewardPackage_Outfit_CSASting_Male" -> "Outfit";
	// "RewardPackage_Components_Espacio_Kit1" -> "Components"). Falls back to the whole id if there is no
	// second segment. Static.
	static std::string Category(const std::string& id) {
		size_t first = id.find('_');
		if (first == std::string::npos) return id;
		size_t second = id.find('_', first + 1);
		if (second == std::string::npos) return id.substr(first + 1);
		return id.substr(first + 1, second - first - 1);
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
