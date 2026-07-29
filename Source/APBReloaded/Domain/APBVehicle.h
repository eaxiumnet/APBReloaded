#pragma once
// APBVehicle.h — M13 (D15): pure-C++17 vehicle catalog + spawn gate + damage/handling model.
// No UE/platform headers — unit-testable in isolation like the other Domain services.
//
// Two concerns live here:
//   1. VehicleCatalog — data-backed stats/handling parsed from Content/Data/vehicles.json
//      (id, name, class, faction eligibility, min rating, max_speed, accel, max health).
//      Only fields present in the apbdb-seeded data are modelled; nothing is invented.
//   2. VehicleInstance — the runtime damage-state machine (Pristine/Damaged/Critical/
//      Destroyed) driving handling degradation + the explode threshold, plus the kiosk
//      spawn gate (faction + rating). Actual UE spawning, paint grids, and physics live on
//      the District side (M13 N-work) on top of this brain.
#include "APBTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace apb {

// Vehicle body class, derived from the apbdb "subcategory" field.
enum class VehicleClass { Unknown, Car, Van, Truck, Motorcycle };

// Health-driven damage state. APB vehicles degrade as they take damage, catch fire when
// critical, and explode when destroyed.
enum class VehicleDamageState { Pristine, Damaged, Critical, Destroyed };

VehicleClass VehicleClassFromSubcategory(const std::string& subcategory);
const char* VehicleClassName(VehicleClass c);
const char* VehicleDamageStateName(VehicleDamageState s);

// Catalog stats/handling for one vehicle (data-backed, immutable reference data).
struct VehicleDef {
	std::string id, name, subcategory, infracategory;
	VehicleClass vclass = VehicleClass::Unknown;
	bool criminal = false;   // usable by the Criminal faction
	bool enforcer = false;   // usable by the Enforcer faction
	int32_t min_rating = 0;  // role/threat rating required to spawn (kiosk gate)
	double max_speed = 0;    // apbdb "max_speed"
	double accel = 0;        // apbdb "accel"
	double max_health = 0;   // apbdb "health"

	bool EligibleFor(Faction f) const {
		return f == Faction::Enforcer ? enforcer : criminal;
	}
};

class VehicleCatalog {
public:
	std::unordered_map<std::string, VehicleDef> vehicles;

	// Parse a JSON array of vehicle objects (the shipped vehicles.json shape). Additive:
	// entries merge into the map by id. Returns true if at least one vehicle was parsed.
	bool LoadFromText(const std::string& json);
	bool LoadFromFile(const std::string& path);

	const VehicleDef* Find(const std::string& id) const {
		auto it = vehicles.find(id);
		return it == vehicles.end() ? nullptr : &it->second;
	}
	int32_t Count() const { return (int32_t)vehicles.size(); }

	// Kiosk offer list: every vehicle the faction is allowed and the rating unlocks.
	std::vector<const VehicleDef*> AvailableTo(Faction f, int32_t rating) const;
};

// Kiosk spawn gate result.
struct VehicleSpawnResult { bool ok = false; std::string error; std::string vehicle_id; };

// Validate a spawn request against the catalog: unknown vehicle, wrong faction, or a rating
// below the vehicle's requirement are rejected. Pure rule — no world/actor side effects.
VehicleSpawnResult CanSpawnVehicle(const VehicleCatalog& cat, const std::string& vehicle_id,
	Faction faction, int32_t rating);

// Runtime damage/handling state for a spawned vehicle.
struct VehicleInstance {
	std::string vehicle_id;
	double max_health = 0;
	double health = 0;
	// State thresholds as a fraction of max health (tunable recreation defaults).
	double damaged_frac = 0.60;   // at/below 60% -> Damaged
	double critical_frac = 0.25;  // at/below 25% -> Critical (on fire)

	static VehicleInstance FromDef(const VehicleDef& d);

	VehicleDamageState State() const;
	bool Alive() const { return health > 0.0; }

	// Handling multiplier applied to max_speed/accel for the current state
	// (1.0 pristine, worsens as the vehicle is damaged, 0 when destroyed).
	double SpeedFactor() const;

	// Apply positive damage; clamps health at 0. Returns the remaining health.
	double ApplyDamage(double amount);
	// Repair by a positive amount; clamps at max_health.
	void Repair(double amount);
};

} // namespace apb
