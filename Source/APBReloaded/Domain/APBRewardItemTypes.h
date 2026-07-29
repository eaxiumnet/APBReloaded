#pragma once
// APB reward-package ITEM-TYPE catalog (the per-component entries of a reward package: vehicle
// customization kits, clothing/outfit/title/weapon-skin components, seasonal + affiliate items),
// extracted from the retail RewardPackageItemTypes.INT (mirror of the cooked SDD table
// "RewardPackageItemTypes") by tools/scripts/extract_reward_item_types.ps1 ->
// Content/Data/reward_item_types.json.
//
// Each item type carries BOTH a rewards-UI DISPLAY description AND (for many) a confirmation mail
// subject/body granted with the component. This is the per-ITEM layer beneath the reward-package
// display blurbs in APBRewardPackages.h (RewardPackages.INT) and the reward-mail catalogs in
// APBWeightedRewards.h / APBRedeemableRewards.h. All ids share the "RewardPackage_" prefix, so the
// meaningful family is the SECOND token (Components/Outfit/Clothing/Title/WeaponSkin/...).
//
// Header-only (matches RedeemableRewardCatalog / WeightedRewardCatalog / RewardPackageCatalog): every
// method is defined in-class so it is implicitly inline and safe to include in multiple TUs.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct RewardItemType {
	std::string id;           // "RewardPackage_Components_Espacio_Kit1", "RewardPackage_Outfit_CSASting_Male", ...
	std::string description;  // rewards-UI blurb (verbatim; U+21B5 paragraph breaks -> '\n')
	std::string mail_subject; // confirmation mail subject (verbatim); often empty (desc-only components)
	std::string mail_body;    // confirmation mail body (verbatim); often empty
	int32_t order = 0;        // stable display order (file order)
};

class RewardItemTypeCatalog {
public:
	std::vector<RewardItemType> items; // sorted by order

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
			RewardItemType r;
			r.id           = Unescape(RawStr(obj, "id"));
			r.description  = Unescape(RawStr(obj, "description"));
			r.mail_subject = Unescape(RawStr(obj, "mail_subject"));
			r.mail_body    = Unescape(RawStr(obj, "mail_body"));
			r.order        = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(items.begin(), items.end(),
				[&](const RewardItemType& e){ return e.id == r.id; });
			if (it == items.end()) items.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(items.begin(), items.end(),
			[](const RewardItemType& a, const RewardItemType& b){ return a.order < b.order; });
		return touched > 0;
	}

	const RewardItemType* Find(const std::string& id) const {
		for (const auto& r : items) if (r.id == id) return &r;
		return nullptr;
	}

	std::string Description(const std::string& id, const std::string& def = std::string()) const {
		const RewardItemType* r = Find(id);
		return r ? r->description : def;
	}
	std::string MailSubject(const std::string& id, const std::string& def = std::string()) const {
		const RewardItemType* r = Find(id);
		return r ? r->mail_subject : def;
	}
	std::string MailBody(const std::string& id, const std::string& def = std::string()) const {
		const RewardItemType* r = Find(id);
		return r ? r->mail_body : def;
	}

	// True if the item exists and has a non-empty rewards-UI description.
	bool HasDescription(const std::string& id) const {
		const RewardItemType* r = Find(id);
		return r && !r->description.empty();
	}
	// True if the item exists and carries confirmation-mail text (subject or body).
	bool HasMail(const std::string& id) const {
		const RewardItemType* r = Find(id);
		return r && (!r->mail_subject.empty() || !r->mail_body.empty());
	}

	// All items whose id belongs to a family, in display order.
	std::vector<const RewardItemType*> ForCategory(const std::string& category) const {
		std::vector<const RewardItemType*> out;
		for (const auto& r : items) if (Category(r.id) == category) out.push_back(&r);
		return out;
	}

	int32_t Count() const { return (int32_t)items.size(); }

	// The family token. Every id shares the leading "RewardPackage_" prefix, so the meaningful family
	// is the SECOND '_'-separated segment ("RewardPackage_Components_Espacio_Kit1" -> "Components",
	// "RewardPackage_Outfit_CSASting_Male" -> "Outfit"). Ids with fewer than two segments return the
	// first segment. Static.
	static std::string Category(const std::string& id) {
		size_t first = id.find('_');
		if (first == std::string::npos) return id;
		size_t second = id.find('_', first + 1);
		if (second == std::string::npos) return id.substr(first + 1);
		return id.substr(first + 1, second - first - 1);
	}

	std::string CategoryFor(const std::string& id) const { return Category(id); }

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
