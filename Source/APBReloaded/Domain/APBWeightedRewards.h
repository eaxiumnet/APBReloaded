#pragma once
// APB weighted-reward MAIL catalog (the in-game mail sent when a standalone weighted reward is granted:
// contact/organisation biography lore, weapon/consumable/deployable reward mails, minigame + legendary
// drops, seasonal grants), extracted from the retail WeightedRewards.INT (mirror of the cooked SDD table
// "WeightedRewards") by tools/scripts/extract_weighted_rewards.ps1 -> Content/Data/weighted_rewards.json.
//
// This is the MAIL-BODY half of the reward system, the counterpart to the reward-package DISPLAY
// descriptions in APBRewardPackages.h (RewardPackages.INT). Use it to fill the mail Subject/Body the
// player receives when a reward lands (e.g. tie role-milestone / mission completion to a reward mail).
//
// Header-only (matches RewardPackageCatalog / HUDMessageCatalog / RoleMilestoneCatalog): every method is
// defined in-class so it is implicitly inline and safe to include in multiple TUs.
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdint>
#include <cstdlib>
#include <algorithm>

namespace apb {

struct WeightedReward {
	std::string id;                      // "Bio_Agrotech", "Legendary_Corsair_JT", "Consumable_...", ...
	std::string reward_mail_subject;     // mail subject line (verbatim)
	std::string reward_mail_body;        // mail body (verbatim prose; U+21B5 paragraph breaks -> '\n')
	std::string out_of_season_subject;   // alt subject outside a seasonal window (empty in retail)
	std::string out_of_season_body;      // alt body outside a seasonal window (empty in retail)
	int32_t order = 0;                   // stable display order (file order)
};

class WeightedRewardCatalog {
public:
	std::vector<WeightedReward> rewards; // sorted by order

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
			WeightedReward r;
			r.id                    = Unescape(RawStr(obj, "id"));
			r.reward_mail_subject   = Unescape(RawStr(obj, "reward_mail_subject"));
			r.reward_mail_body      = Unescape(RawStr(obj, "reward_mail_body"));
			r.out_of_season_subject = Unescape(RawStr(obj, "out_of_season_subject"));
			r.out_of_season_body    = Unescape(RawStr(obj, "out_of_season_body"));
			r.order                 = (int32_t)RNum(obj, "order", 0);
			if (r.id.empty()) continue;
			auto it = std::find_if(rewards.begin(), rewards.end(),
				[&](const WeightedReward& e){ return e.id == r.id; });
			if (it == rewards.end()) rewards.push_back(r);
			else *it = r;
			++touched;
		}
		std::sort(rewards.begin(), rewards.end(),
			[](const WeightedReward& a, const WeightedReward& b){ return a.order < b.order; });
		return touched > 0;
	}

	const WeightedReward* Find(const std::string& id) const {
		for (const auto& r : rewards) if (r.id == id) return &r;
		return nullptr;
	}

	std::string RewardSubject(const std::string& id, const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		return r ? r->reward_mail_subject : def;
	}
	std::string RewardBody(const std::string& id, const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		return r ? r->reward_mail_body : def;
	}
	std::string OutOfSeasonSubject(const std::string& id, const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		return r ? r->out_of_season_subject : def;
	}
	std::string OutOfSeasonBody(const std::string& id, const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		return r ? r->out_of_season_body : def;
	}

	// True if the reward exists and has a non-empty subject or body.
	bool HasReward(const std::string& id) const {
		const WeightedReward* r = Find(id);
		return r && (!r->reward_mail_subject.empty() || !r->reward_mail_body.empty());
	}

	// The mail subject/body the player should receive given whether the seasonal window is open: the
	// out-of-season variant when non-empty, else the regular text. Unknown ids -> def.
	std::string MailSubjectFor(const std::string& id, bool outOfSeason,
		const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		if (!r) return def;
		if (outOfSeason && !r->out_of_season_subject.empty()) return r->out_of_season_subject;
		return r->reward_mail_subject;
	}
	std::string MailBodyFor(const std::string& id, bool outOfSeason,
		const std::string& def = std::string()) const {
		const WeightedReward* r = Find(id);
		if (!r) return def;
		if (outOfSeason && !r->out_of_season_body.empty()) return r->out_of_season_body;
		return r->reward_mail_body;
	}

	// All rewards whose id belongs to a family (the token before the first '_'), in display order.
	std::vector<const WeightedReward*> ForCategory(const std::string& category) const {
		std::vector<const WeightedReward*> out;
		for (const auto& r : rewards) if (Category(r.id) == category) out.push_back(&r);
		return out;
	}

	int32_t Count() const { return (int32_t)rewards.size(); }

	// The family token: the substring before the first '_' ("Bio_Agrotech" -> "Bio",
	// "Legendary_Corsair_JT" -> "Legendary"). Ids with no '_' return the whole id. Static.
	static std::string Category(const std::string& id) {
		size_t u = id.find('_');
		return (u == std::string::npos) ? id : id.substr(0, u);
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
