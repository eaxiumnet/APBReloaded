#pragma once
#include "APBCatalog.h"
namespace apb {
struct CombatantState {
	std::string name; Faction faction=Faction::Criminal; double health=1000, x=0, y=0; bool alive=true;
};
struct ShotResult { bool hit=false; double damage=0; bool killed=false; std::string reason; };
ShotResult ResolveShot(const ItemDef& weapon, CombatantState& shooter, CombatantState& target,
	double aim_x, double aim_y, double damage_mult=1.0);
}
