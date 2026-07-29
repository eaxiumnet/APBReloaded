// APBVehicle.cpp — M13 (D15): implementation of the pure-C++17 vehicle catalog,
// spawn gate, and runtime damage/handling model declared in APBVehicle.h.
//
// JSON parsing reuses the public helpers declared in APBCatalog.h
// (JsonGetString/JsonGetNumber/JsonSplitObjects, defined in APBCatalog.cpp — link it),
// plus a local bool parser for the bare "criminal"/"enforcer" fields and a local file
// reader (APBCatalog's ReadFile has internal linkage and is not visible here).
#include "APBVehicle.h"
#include "APBCatalog.h"
#include <fstream>
#include <sstream>
#include <cctype>

namespace apb {
namespace {

std::string ReadWholeFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary);
	if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf();
	return ss.str();
}

// Parse a bare JSON boolean value ("criminal": true). Returns def when the key is
// missing or the value is not a recognizable true/false token.
bool JsonGetBool(const std::string& obj, const std::string& key, bool def) {
	const std::string pat = std::string(1, char(34)) + key + std::string(1, char(34));
	size_t p = obj.find(pat); if (p == std::string::npos) return def;
	p = obj.find(':', p + pat.size()); if (p == std::string::npos) return def; ++p;
	while (p < obj.size() && isspace((unsigned char)obj[p])) ++p;
	if (obj.compare(p, 4, "true") == 0) return true;
	if (obj.compare(p, 5, "false") == 0) return false;
	return def;
}

std::string ToLower(std::string s) {
	for (char& c : s) c = (char)tolower((unsigned char)c);
	return s;
}

} // namespace

VehicleClass VehicleClassFromSubcategory(const std::string& subcategory) {
	const std::string s = ToLower(subcategory);
	if (s.find("motor") != std::string::npos || s.find("bike") != std::string::npos)
		return VehicleClass::Motorcycle;
	if (s.find("truck") != std::string::npos) return VehicleClass::Truck;
	if (s.find("van") != std::string::npos)   return VehicleClass::Van;
	if (s.find("car") != std::string::npos)   return VehicleClass::Car;
	return VehicleClass::Unknown;
}

const char* VehicleClassName(VehicleClass c) {
	switch (c) {
		case VehicleClass::Car:        return "Car";
		case VehicleClass::Van:        return "Van";
		case VehicleClass::Truck:      return "Truck";
		case VehicleClass::Motorcycle: return "Motorcycle";
		default:                       return "Unknown";
	}
}

const char* VehicleDamageStateName(VehicleDamageState s) {
	switch (s) {
		case VehicleDamageState::Pristine:  return "Pristine";
		case VehicleDamageState::Damaged:   return "Damaged";
		case VehicleDamageState::Critical:  return "Critical";
		case VehicleDamageState::Destroyed: return "Destroyed";
		default:                            return "Unknown";
	}
}

bool VehicleCatalog::LoadFromText(const std::string& json) {
	int added = 0;
	for (const auto& obj : JsonSplitObjects(json)) {
		VehicleDef d;
		d.id = JsonGetString(obj, "id");
		if (d.id.empty()) continue;
		d.name = JsonGetString(obj, "name", d.id);
		d.subcategory = JsonGetString(obj, "subcategory");
		d.infracategory = JsonGetString(obj, "infracategory");
		d.vclass = VehicleClassFromSubcategory(d.subcategory);
		d.criminal = JsonGetBool(obj, "criminal", false);
		d.enforcer = JsonGetBool(obj, "enforcer", false);
		d.min_rating = (int32_t)JsonGetNumber(obj, "min_rating", 0);
		d.max_speed = JsonGetNumber(obj, "max_speed", 0);
		d.accel = JsonGetNumber(obj, "accel", 0);
		d.max_health = JsonGetNumber(obj, "health", 0);
		vehicles[d.id] = d;
		++added;
	}
	return added > 0;
}

bool VehicleCatalog::LoadFromFile(const std::string& path) {
	const std::string text = ReadWholeFile(path);
	if (text.empty()) return false;
	return LoadFromText(text);
}

std::vector<const VehicleDef*> VehicleCatalog::AvailableTo(Faction f, int32_t rating) const {
	std::vector<const VehicleDef*> out;
	for (const auto& kv : vehicles) {
		const VehicleDef& d = kv.second;
		if (d.EligibleFor(f) && rating >= d.min_rating) out.push_back(&d);
	}
	return out;
}

VehicleSpawnResult CanSpawnVehicle(const VehicleCatalog& cat, const std::string& vehicle_id,
	Faction faction, int32_t rating) {
	VehicleSpawnResult r; r.vehicle_id = vehicle_id;
	const VehicleDef* d = cat.Find(vehicle_id);
	if (!d) { r.error = "unknown_vehicle"; return r; }
	if (!d->EligibleFor(faction)) { r.error = "wrong_faction"; return r; }
	if (rating < d->min_rating) { r.error = "rating_too_low"; return r; }
	r.ok = true;
	return r;
}

VehicleInstance VehicleInstance::FromDef(const VehicleDef& d) {
	VehicleInstance v;
	v.vehicle_id = d.id;
	v.max_health = d.max_health;
	v.health = d.max_health;
	return v;
}

VehicleDamageState VehicleInstance::State() const {
	if (health <= 0.0) return VehicleDamageState::Destroyed;
	const double frac = max_health > 0.0 ? health / max_health : 1.0;
	if (frac <= critical_frac) return VehicleDamageState::Critical;
	if (frac <= damaged_frac)  return VehicleDamageState::Damaged;
	return VehicleDamageState::Pristine;
}

double VehicleInstance::SpeedFactor() const {
	switch (State()) {
		case VehicleDamageState::Pristine:  return 1.0;
		case VehicleDamageState::Damaged:   return 0.9;
		case VehicleDamageState::Critical:  return 0.7;
		default:                            return 0.0;
	}
}

double VehicleInstance::ApplyDamage(double amount) {
	if (amount > 0.0) health -= amount;
	if (health < 0.0) health = 0.0;
	return health;
}

void VehicleInstance::Repair(double amount) {
	if (amount > 0.0) health += amount;
	if (health > max_health) health = max_health;
}

} // namespace apb
