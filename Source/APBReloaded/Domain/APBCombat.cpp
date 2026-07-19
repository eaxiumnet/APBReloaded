#include "APBCombat.h"
namespace apb {
static double Dist(double ax,double ay,double bx,double by){ double dx=bx-ax,dy=by-ay; return std::sqrt(dx*dx+dy*dy); }
ShotResult ResolveShot(const ItemDef& weapon, CombatantState& shooter, CombatantState& target,
	double aim_x, double aim_y, double damage_mult){
	ShotResult r;
	if(!shooter.alive||!target.alive){ r.reason="dead"; return r; }
	if(shooter.faction==target.faction){ r.reason="friendly"; return r; }
	double range=weapon.max_range>0?weapon.max_range:80.0;
	double d=Dist(shooter.x,shooter.y,target.x,target.y);
	if(d>range){ r.reason="out_of_range"; return r; }
	if(Dist(aim_x,aim_y,target.x,target.y)>8.0){ r.reason="miss"; return r; }
	double dmg=weapon.damage*damage_mult; if(dmg<=0) dmg=35;
	double half=range*0.5;
	if(d>half) dmg*=std::max(0.35, 1.0-(d-half)/std::max(1.0, range-half));
	target.health-=dmg; r.hit=true; r.damage=dmg;
	if(target.health<=0){ target.health=0; target.alive=false; r.killed=true; }
	r.reason="hit"; return r;
}
}
