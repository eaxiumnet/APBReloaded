#pragma once
#include "APBTypes.h"
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace apb {

struct ThreatTier {
	int32_t level = 0;
	std::string name;
	double opposition_multiplier = 1.0;
	int32_t bot_count = 3;
	double threshold = 0.0;
	double reward_multiplier = 1.0;
	bool citywide_pvp = false;
	std::string description; // player-facing HUD blurb (apbdb /heat, mirrors HeatLevels.INT)
};

// Notoriety (Criminal) / Prestige (Enforcer). Prefer apbdb /heat thresholds from
// Content/Data/threat_table.json; fall back to linear ladder for unit tests without data.
class ThreatSystem {
public:
	Faction faction = Faction::Criminal;
	double points = 0;
	std::vector<ThreatTier> criminal_tiers;
	std::vector<ThreatTier> enforcer_tiers;
	bool loaded_from_table = false;

	void ApplyKillOpponent() { points += 8; }
	void ApplyCivilianHarm() { points += 12; }
	void ApplyMissionObjective() { points += 5; }
	void ApplyMissionComplete() { points += 15; }
	void ApplyMissionFail() { points = std::max(0.0, points - 6); }

	bool LoadFromThreatTableJson(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::ostringstream ss;
		ss << in.rdbuf();
		const std::string text = ss.str();
		if (text.empty()) return false;

		criminal_tiers = ParseTierArray(text, "criminal_notoriety");
		enforcer_tiers = ParseTierArray(text, "enforcer_prestige");
		if (criminal_tiers.empty() && enforcer_tiers.empty()) return false;
		EnsureFallbackTiers();
		loaded_from_table = true;
		return true;
	}

	int32_t LevelIndex() const {
		const auto& tiers = ActiveTiers();
		if (tiers.empty()) {
			int32_t i = (int32_t)(points / 20.0);
			if (i < 0) i = 0;
			if (i > 5) i = 5;
			return i;
		}
		int32_t best = 0;
		for (const auto& t : tiers) {
			if (points + 1e-9 >= t.threshold) best = t.level;
		}
		return best;
	}

	ThreatTier CurrentTier() const {
		const auto& tiers = ActiveTiers();
		const int32_t i = LevelIndex();
		for (const auto& t : tiers) {
			if (t.level == i) return t;
		}
		// Linear fallback (pre-apbdb)
		static const char* crim[] = {"Clean", "Suspicious", "Wanted", "High Threat", "Most Wanted", "Legend"};
		static const char* enf[] = {"Rookie", "Officer", "Sergeant", "Lieutenant", "Captain", "Commander"};
		static const double mult[] = {0.6, 0.85, 1.0, 1.25, 1.55, 1.9};
		static const int32_t bots[] = {2, 3, 4, 5, 7, 9};
		ThreatTier t;
		t.level = i;
		t.name = (faction == Faction::Enforcer) ? enf[i] : crim[i];
		t.opposition_multiplier = mult[i];
		t.bot_count = bots[i];
		t.threshold = i * 20.0;
		t.reward_multiplier = mult[i];
		return t;
	}

private:
	const std::vector<ThreatTier>& ActiveTiers() const {
		return (faction == Faction::Enforcer) ? enforcer_tiers : criminal_tiers;
	}

	void EnsureFallbackTiers() {
		if (criminal_tiers.empty()) {
			static const char* crim[] = {"Clean", "Suspicious", "Wanted", "High Threat", "Most Wanted", "Legend"};
			static const double mult[] = {0.6, 0.85, 1.0, 1.25, 1.55, 1.9};
			static const int32_t bots[] = {2, 3, 4, 5, 7, 9};
			for (int i = 0; i <= 5; ++i) {
				ThreatTier t;
				t.level = i;
				t.name = crim[i];
				t.opposition_multiplier = mult[i];
				t.bot_count = bots[i];
				t.threshold = i * 20.0;
				t.reward_multiplier = mult[i];
				criminal_tiers.push_back(t);
			}
		}
		if (enforcer_tiers.empty()) {
			static const char* enf[] = {"Rookie", "Officer", "Sergeant", "Lieutenant", "Captain", "Commander"};
			static const double mult[] = {0.6, 0.85, 1.0, 1.25, 1.55, 1.9};
			static const int32_t bots[] = {2, 3, 4, 5, 7, 9};
			for (int i = 0; i <= 5; ++i) {
				ThreatTier t;
				t.level = i;
				t.name = enf[i];
				t.opposition_multiplier = mult[i];
				t.bot_count = bots[i];
				t.threshold = i * 20.0;
				t.reward_multiplier = mult[i];
				enforcer_tiers.push_back(t);
			}
		}
	}

	static std::vector<std::string> SplitObjectsInSection(const std::string& text, const std::string& key) {
		std::vector<std::string> out;
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = text.find(pat);
		if (p == std::string::npos) return out;
		size_t lb = text.find('[', p);
		if (lb == std::string::npos) return out;
		int depth = 0;
		size_t start = std::string::npos;
		for (size_t i = lb; i < text.size(); ++i) {
			if (text[i] == '[') {
				++depth;
			} else if (text[i] == ']') {
				--depth;
				if (depth == 0) {
					// parse objects inside this array slice
					const std::string slice = text.substr(lb, i - lb + 1);
					int od = 0;
					size_t os = std::string::npos;
					for (size_t j = 0; j < slice.size(); ++j) {
						if (slice[j] == '{') {
							if (od == 0) os = j;
							++od;
						} else if (slice[j] == '}') {
							--od;
							if (od == 0 && os != std::string::npos) {
								out.push_back(slice.substr(os, j - os + 1));
								os = std::string::npos;
							}
						}
					}
					break;
				}
			}
		}
		(void)start;
		return out;
	}

	static double Num(const std::string& obj, const std::string& key, double def) {
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = obj.find(pat);
		if (p == std::string::npos) return def;
		p = obj.find(':', p + pat.size());
		if (p == std::string::npos) return def;
		++p;
		while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
		char* end = nullptr;
		double v = strtod(obj.c_str() + p, &end);
		if (end == obj.c_str() + p) return def;
		return v;
	}

	static std::string Str(const std::string& obj, const std::string& key, const std::string& def) {
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = obj.find(pat);
		if (p == std::string::npos) return def;
		p = obj.find(':', p + pat.size());
		if (p == std::string::npos) return def;
		p = obj.find('"', p + 1);
		if (p == std::string::npos) return def;
		size_t e = p + 1;
		std::string out;
		while (e < obj.size()) {
			if (obj[e] == '\\' && e + 1 < obj.size()) {
				out.push_back(obj[e + 1]);
				e += 2;
				continue;
			}
			if (obj[e] == '"') break;
			out.push_back(obj[e]);
			++e;
		}
		return out.empty() ? def : out;
	}

	static bool Boolish(const std::string& obj, const std::string& key) {
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = obj.find(pat);
		if (p == std::string::npos) return false;
		p = obj.find(':', p + pat.size());
		if (p == std::string::npos) return false;
		const std::string tail = obj.substr(p, 24);
		return tail.find("true") != std::string::npos || tail.find("1") != std::string::npos;
	}

	static std::vector<ThreatTier> ParseTierArray(const std::string& text, const std::string& key) {
		std::vector<ThreatTier> tiers;
		for (const auto& obj : SplitObjectsInSection(text, key)) {
			ThreatTier t;
			t.level = (int32_t)Num(obj, "level", 0);
			t.name = Str(obj, "name", Str(obj, "sAPBDB", "L" + std::to_string(t.level)));
			t.threshold = Num(obj, "threshold", Num(obj, "fThreshold", t.level * 20.0));
			t.reward_multiplier = Num(obj, "reward_multiplier", Num(obj, "fRewardMultiplier", 1.0));
			t.opposition_multiplier = Num(obj, "opposition_multiplier", t.reward_multiplier > 0 ? t.reward_multiplier : 0.6);
			t.bot_count = (int32_t)Num(obj, "bot_count", 3);
			t.citywide_pvp = Boolish(obj, "citywide_pvp") || Boolish(obj, "bPVPUnlockedToAllOpposingFaction");
			t.description = Str(obj, "description", Str(obj, "sDescription", ""));
			tiers.push_back(t);
		}
		std::sort(tiers.begin(), tiers.end(), [](const ThreatTier& a, const ThreatTier& b) {
			return a.level < b.level;
		});
		return tiers;
	}
};

// -----------------------------------------------------------------------------
// Matchmaking THREAT RATING (distinct from notoriety/prestige "heat" above).
// The skill bracket shown next to a player's name (In Training / Green / Bronze /
// Silver / Gold) plus the AllowedDistrictThreats rule gating which district-instance
// threat brackets that rating may join. Seeded from Content/Data/threat_ratings.json
// (extract_threat_ratings.ps1, mirror of the retail SDD table ThreatLevel).
struct ThreatRating {
	std::string id;                        // e.g. "ThreatLevel_01", "ThreatLevel_04"
	std::string displayed_name;            // e.g. "Green", "Gold"
	std::string allowed_district_threats;  // e.g. "Bronze, Silver or Gold" (may be empty)
	int32_t rank = 0;                      // 0 = In Training, 1 = Green, ... 4 = Gold
};

class ThreatRatingCatalog {
public:
	std::vector<ThreatRating> ratings; // sorted by rank

	bool LoadFromJsonFile(const std::string& path) {
		std::ifstream in(path, std::ios::binary);
		if (!in) return false;
		std::ostringstream ss;
		ss << in.rdbuf();
		return LoadFromJsonText(ss.str());
	}

	bool LoadFromJsonText(const std::string& text) {
		if (text.empty()) return false; // merge: never clear existing ratings
		int added = 0;
		for (const auto& obj : SplitTopObjects(text)) {
			ThreatRating r;
			r.id = RStr(obj, "id");
			if (r.id.empty()) continue;
			r.displayed_name = RStr(obj, "displayed_name");
			r.allowed_district_threats = RStr(obj, "allowed_district_threats");
			r.rank = (int32_t)RNum(obj, "rank", (double)ratings.size());
			ratings.push_back(r);
			++added;
		}
		std::sort(ratings.begin(), ratings.end(), [](const ThreatRating& a, const ThreatRating& b) {
			return a.rank < b.rank;
		});
		return added > 0;
	}

	const ThreatRating* Find(const std::string& id) const {
		for (const auto& r : ratings) if (r.id == id) return &r;
		return nullptr;
	}
	const ThreatRating* FindByDisplayedName(const std::string& name) const {
		for (const auto& r : ratings) if (IEquals(r.displayed_name, name)) return &r;
		return nullptr;
	}
	std::string DisplayedName(const std::string& id, const std::string& def = std::string()) const {
		const ThreatRating* r = Find(id);
		return r ? r->displayed_name : def;
	}
	std::string AllowedDistrictThreats(const std::string& id, const std::string& def = std::string()) const {
		const ThreatRating* r = Find(id);
		return r ? r->allowed_district_threats : def;
	}

	// Matchmaking gate: may a player carrying threat rating `ratingId` join a district
	// instance whose displayed threat bracket is `districtThreatName` (e.g. "Green","Gold")?
	// Follows the retail AllowedDistrictThreats list; when that list is empty the rating is
	// confined to its own bracket (In Training / Green carry no cross-threat access).
	bool CanJoinDistrictThreat(const std::string& ratingId, const std::string& districtThreatName) const {
		const ThreatRating* r = Find(ratingId);
		if (!r) return false;
		if (r->allowed_district_threats.empty()) return IEquals(r->displayed_name, districtThreatName);
		for (const auto& tok : SplitAllowed(r->allowed_district_threats))
			if (IEquals(tok, districtThreatName)) return true;
		return false;
	}

	int32_t Count() const { return (int32_t)ratings.size(); }

private:
	static bool IEquals(const std::string& a, const std::string& b) {
		if (a.size() != b.size()) return false;
		for (size_t i = 0; i < a.size(); ++i)
			if (tolower((unsigned char)a[i]) != tolower((unsigned char)b[i])) return false;
		return true;
	}
	static std::string Trim(const std::string& s) {
		size_t a = 0, b = s.size();
		while (a < b && isspace((unsigned char)s[a])) ++a;
		while (b > a && isspace((unsigned char)s[b - 1])) --b;
		return s.substr(a, b - a);
	}
	// "Green, Bronze, Silver or Gold" -> {Green, Bronze, Silver, Gold}
	static std::vector<std::string> SplitAllowed(const std::string& s) {
		std::string t = s;
		for (;;) {
			size_t p = t.find(" or ");
			if (p == std::string::npos) break;
			t.replace(p, 4, ",");
		}
		std::vector<std::string> out;
		size_t start = 0;
		while (start <= t.size()) {
			size_t c = t.find(',', start);
			std::string tok = Trim(t.substr(start, c == std::string::npos ? std::string::npos : c - start));
			if (!tok.empty()) out.push_back(tok);
			if (c == std::string::npos) break;
			start = c + 1;
		}
		return out;
	}
	static std::vector<std::string> SplitTopObjects(const std::string& text) {
		std::vector<std::string> out;
		int depth = 0;
		size_t os = std::string::npos;
		for (size_t i = 0; i < text.size(); ++i) {
			const char ch = text[i];
			if (ch == '{') {
				if (depth == 0) os = i;
				++depth;
			} else if (ch == '}') {
				--depth;
				if (depth == 0 && os != std::string::npos) {
					out.push_back(text.substr(os, i - os + 1));
					os = std::string::npos;
				}
			}
		}
		return out;
	}
	static std::string RStr(const std::string& obj, const std::string& key) {
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = obj.find(pat);
		if (p == std::string::npos) return "";
		p = obj.find(':', p + pat.size());
		if (p == std::string::npos) return "";
		p = obj.find('"', p + 1);
		if (p == std::string::npos) return "";
		size_t e = p + 1;
		std::string out;
		while (e < obj.size()) {
			if (obj[e] == '\\' && e + 1 < obj.size()) {
				out.push_back(obj[e + 1]);
				e += 2;
				continue;
			}
			if (obj[e] == '"') break;
			out.push_back(obj[e]);
			++e;
		}
		return out;
	}
	static double RNum(const std::string& obj, const std::string& key, double def) {
		const std::string pat = std::string("\"") + key + "\"";
		size_t p = obj.find(pat);
		if (p == std::string::npos) return def;
		p = obj.find(':', p + pat.size());
		if (p == std::string::npos) return def;
		++p;
		while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
		char* end = nullptr;
		double v = strtod(obj.c_str() + p, &end);
		if (end == obj.c_str() + p) return def;
		return v;
	}
};

} // namespace apb

