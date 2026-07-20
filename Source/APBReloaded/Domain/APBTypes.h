#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <algorithm>
#include <cmath>
#include <cctype>

namespace apb {

enum class Faction : int32_t { Enforcer = 0, Criminal = 1 };

inline const char* FactionName(Faction f) {
	return f == Faction::Enforcer ? "Enforcer" : "Criminal";
}
inline Faction Opposing(Faction f) {
	return f == Faction::Enforcer ? Faction::Criminal : Faction::Enforcer;
}
inline Faction FactionFromString(const std::string& s) {
	std::string k; k.resize(s.size());
	for (size_t i=0;i<s.size();++i) k[i] = (char)tolower((unsigned char)s[i]);
	if (k=="enforcer"||k=="enf"||k=="0") return Faction::Enforcer;
	return Faction::Criminal;
}

struct ItemDef {
	std::string id, name, category;
	double damage=0, rpm=0, max_range=0;
	int32_t clip=0;
	int64_t armas_price=0, market_value=0;
	bool armas_listed=false;
	std::string slot;
	int32_t wardrobe_tab=0; // clothing only; 1..15, 0=unset
};
struct DistrictInfo {
	std::string id, name, map_name;
	int32_t population=0, max_players=64;
	bool joinable=true;
};
struct CharacterProfile {
	std::string name;
	Faction faction = Faction::Criminal;
	int64_t cash=10000, g1c=500;
	double threat_points=0;
};

} // namespace apb
