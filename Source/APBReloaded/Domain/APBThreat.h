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
			tiers.push_back(t);
		}
		std::sort(tiers.begin(), tiers.end(), [](const ThreatTier& a, const ThreatTier& b) {
			return a.level < b.level;
		});
		return tiers;
	}
};

} // namespace apb
