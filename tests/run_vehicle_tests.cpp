// run_vehicle_tests.cpp — M13 (D15): VehicleCatalog + spawn gate + damage/handling tests.
// Links APBVehicle.cpp + APBCatalog.cpp (for the shared JSON helpers).
// Pattern mirrors the other run_*_tests.cpp.
#include "APBVehicle.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

static std::string DataDir() { return R"(D:\APBReloaded\Content\Data)"; }

// A small inline catalog mirroring the real vehicles.json shape.
static const char* kInlineJson = R"([
  {
    "id": "Vehicle_Car_A_UtilityEstate",
    "name": "Balkan Ravan",
    "category": "Vehicle",
    "subcategory": "VehicleCar",
    "infracategory": "VehicleCarUtilityEstate",
    "criminal": true,
    "enforcer": true,
    "min_rating": 0,
    "max_speed": 21.0,
    "accel": 14.0,
    "health": 800.0
  },
  {
    "id": "Vehicle_Truck_C_Carrying_Enf_Praetorian_T5",
    "name": "Defender Citadel",
    "category": "Vehicle",
    "subcategory": "VehicleVan",
    "infracategory": "VehicleVanCriminalCarrying",
    "criminal": false,
    "enforcer": true,
    "min_rating": 195,
    "max_speed": 21.0,
    "accel": 14.0,
    "health": 800.0
  },
  {
    "id": "Vehicle_Bike_A_Sport",
    "name": "Pioneer Bike",
    "category": "Vehicle",
    "subcategory": "VehicleMotorcycle",
    "infracategory": "VehicleMotorcycleSport",
    "criminal": true,
    "enforcer": false,
    "min_rating": 30,
    "max_speed": 30.0,
    "accel": 20.0,
    "health": 300.0
  }
])";

static void TestClassMapping() {
	CHECK(VehicleClassFromSubcategory("VehicleCar") == VehicleClass::Car, "VehicleCar -> Car");
	CHECK(VehicleClassFromSubcategory("VehicleTruck") == VehicleClass::Truck, "VehicleTruck -> Truck");
	CHECK(VehicleClassFromSubcategory("VehicleVan") == VehicleClass::Van, "VehicleVan -> Van");
	CHECK(VehicleClassFromSubcategory("VehicleMotorcycle") == VehicleClass::Motorcycle, "VehicleMotorcycle -> Motorcycle");
	CHECK(VehicleClassFromSubcategory("vehiclebike") == VehicleClass::Motorcycle, "bike (lowercase) -> Motorcycle");
	CHECK(VehicleClassFromSubcategory("Something") == VehicleClass::Unknown, "unknown subcategory -> Unknown");
}

static void TestParseAndEligibility() {
	VehicleCatalog cat;
	CHECK(cat.LoadFromText(kInlineJson), "inline json parsed");
	CHECK(cat.Count() == 3, "three vehicles loaded");

	const VehicleDef* estate = cat.Find("Vehicle_Car_A_UtilityEstate");
	CHECK(estate != nullptr, "estate found");
	CHECK(estate && estate->vclass == VehicleClass::Car, "estate class = Car");
	CHECK(estate && estate->max_health == 800.0, "estate health parsed");
	CHECK(estate && estate->EligibleFor(Faction::Enforcer) && estate->EligibleFor(Faction::Criminal), "estate usable by both factions");

	const VehicleDef* enf = cat.Find("Vehicle_Truck_C_Carrying_Enf_Praetorian_T5");
	CHECK(enf && enf->EligibleFor(Faction::Enforcer) && !enf->EligibleFor(Faction::Criminal), "praetorian enforcer-only");
	CHECK(enf && enf->min_rating == 195, "praetorian min_rating parsed");

	const VehicleDef* bike = cat.Find("Vehicle_Bike_A_Sport");
	CHECK(bike && bike->vclass == VehicleClass::Motorcycle, "bike class = Motorcycle");
	CHECK(bike && bike->EligibleFor(Faction::Criminal) && !bike->EligibleFor(Faction::Enforcer), "bike criminal-only");
}

static void TestSpawnGate() {
	VehicleCatalog cat; cat.LoadFromText(kInlineJson);

	CHECK(!CanSpawnVehicle(cat, "nope", Faction::Criminal, 255).ok, "unknown vehicle rejected");
	CHECK(CanSpawnVehicle(cat, "nope", Faction::Criminal, 255).error == "unknown_vehicle", "unknown_vehicle error");

	auto wrong = CanSpawnVehicle(cat, "Vehicle_Truck_C_Carrying_Enf_Praetorian_T5", Faction::Criminal, 255);
	CHECK(!wrong.ok && wrong.error == "wrong_faction", "criminal cannot spawn enforcer vehicle");

	auto lowrat = CanSpawnVehicle(cat, "Vehicle_Truck_C_Carrying_Enf_Praetorian_T5", Faction::Enforcer, 100);
	CHECK(!lowrat.ok && lowrat.error == "rating_too_low", "rating below min rejected");

	auto ok = CanSpawnVehicle(cat, "Vehicle_Truck_C_Carrying_Enf_Praetorian_T5", Faction::Enforcer, 195);
	CHECK(ok.ok && ok.error.empty(), "enforcer at exact rating spawns");

	auto ok2 = CanSpawnVehicle(cat, "Vehicle_Car_A_UtilityEstate", Faction::Criminal, 0);
	CHECK(ok2.ok, "rating-0 vehicle spawns for both factions");
}

static void TestAvailableTo() {
	VehicleCatalog cat; cat.LoadFromText(kInlineJson);
	// Criminal rating 40: estate (both, r0) + bike (crim, r30). Not praetorian (enf-only).
	auto crim = cat.AvailableTo(Faction::Criminal, 40);
	CHECK(crim.size() == 2, "criminal r40 sees estate + bike");
	// Enforcer rating 40: estate (both) only; praetorian needs r195.
	auto enf40 = cat.AvailableTo(Faction::Enforcer, 40);
	CHECK(enf40.size() == 1, "enforcer r40 sees only estate");
	auto enf200 = cat.AvailableTo(Faction::Enforcer, 200);
	CHECK(enf200.size() == 2, "enforcer r200 sees estate + praetorian");
}

static void TestDamageModel() {
	VehicleCatalog cat; cat.LoadFromText(kInlineJson);
	const VehicleDef* estate = cat.Find("Vehicle_Car_A_UtilityEstate");
	VehicleInstance v = VehicleInstance::FromDef(*estate);
	CHECK(v.health == 800.0 && v.max_health == 800.0, "instance starts at full health");
	CHECK(v.State() == VehicleDamageState::Pristine, "full health = Pristine");
	CHECK(v.SpeedFactor() == 1.0, "pristine speed factor 1.0");

	// 800 -> 400 (50%) : below 60% damaged_frac = Damaged.
	v.ApplyDamage(400);
	CHECK(v.State() == VehicleDamageState::Damaged, "50% health = Damaged");
	CHECK(v.SpeedFactor() == 0.9, "damaged speed factor 0.9");

	// 400 -> 150 (18.75%) : below 25% critical_frac = Critical.
	v.ApplyDamage(250);
	CHECK(v.State() == VehicleDamageState::Critical, "18.75% health = Critical");
	CHECK(v.SpeedFactor() == 0.7, "critical speed factor 0.7");

	// Repair back above 60%.
	v.Repair(500);
	CHECK(v.health == 650.0, "repair adds health");
	CHECK(v.State() == VehicleDamageState::Pristine, "repaired above 60% = Pristine");

	// Over-repair clamps at max.
	v.Repair(1000);
	CHECK(v.health == 800.0, "repair clamps at max_health");

	// Destroy: health clamps at 0.
	double rem = v.ApplyDamage(5000);
	CHECK(rem == 0.0 && v.health == 0.0, "over-damage clamps at 0");
	CHECK(!v.Alive() && v.State() == VehicleDamageState::Destroyed, "0 health = Destroyed, not Alive");
	CHECK(v.SpeedFactor() == 0.0, "destroyed speed factor 0.0");
}

static void TestRealVehiclesJson() {
	VehicleCatalog cat;
	const std::string path = DataDir() + "\\vehicles.json";
	if (!cat.LoadFromFile(path)) {
		std::printf("SKIP: real vehicles.json not readable at %s\n", path.c_str());
		return;
	}
	CHECK(cat.Count() > 0, "real vehicles.json parsed at least one vehicle");
	const VehicleDef* known = cat.Find("Vehicle_Car_A_UtilityEstate");
	CHECK(known != nullptr, "known real id present");
	CHECK(known && known->vclass == VehicleClass::Car, "known real id maps to Car");
	CHECK(known && known->max_health > 0.0, "known real id has health");
}

int main() {
	std::printf("=== APB Vehicle Tests (M13) ===\n");
	TestClassMapping();
	TestParseAndEligibility();
	TestSpawnGate();
	TestAvailableTo();
	TestDamageModel();
	TestRealVehiclesJson();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
