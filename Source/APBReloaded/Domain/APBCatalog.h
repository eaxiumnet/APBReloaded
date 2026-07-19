#pragma once
#include "APBTypes.h"
#include <fstream>
#include <sstream>
#include <cstdlib>

namespace apb {

class Catalog {
public:
	std::unordered_map<std::string, ItemDef> items;
	std::vector<DistrictInfo> districts;
	std::unordered_map<std::string, std::string> mission_titles;
	std::string source_note;
	bool LoadWeaponsJson(const std::string& path);
	bool LoadVehiclesJson(const std::string& path);
	bool LoadDistrictsJson(const std::string& path);
	bool LoadMissionsJson(const std::string& path);
	bool LoadClothingJson(const std::string& path);
	bool LoadAllFromDir(const std::string& dir);
	const ItemDef* FindItem(const std::string& id) const {
		auto it = items.find(id); return it==items.end()?nullptr:&it->second;
	}
	std::vector<ItemDef> ArmasCatalog() const {
		std::vector<ItemDef> o; for (auto& kv: items) if (kv.second.armas_listed) o.push_back(kv.second); return o;
	}
	std::vector<DistrictInfo> JoinableDistricts() const {
		std::vector<DistrictInfo> o; for (auto& d: districts) if (d.joinable) o.push_back(d); return o;
	}
};

std::string JsonGetString(const std::string& obj, const std::string& key, const std::string& def="");
double JsonGetNumber(const std::string& obj, const std::string& key, double def=0.0);
std::vector<std::string> JsonSplitObjects(const std::string& text);

} // namespace apb
