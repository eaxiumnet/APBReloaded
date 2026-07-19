#pragma once
#include "APBTypes.h"
#include <fstream>
#include <sstream>

namespace apb {

struct ModelRef {
	std::string package;
	std::string rel_path;
	std::string family;
	std::string ue5_import_hint;
	int64_t size = 0;
};

// Loads Content/Data/model_reference_catalog.json (vehicles list) for runtime mesh binding.
class ModelRegistry {
public:
	std::vector<ModelRef> vehicles;
	std::vector<ModelRef> characters;
	std::string source_install;
	std::string umodel_path;
	std::string game_tag = "apb";
	std::string notes;

	bool LoadFromFile(const std::string& path);
	const ModelRef* FindVehiclePackage(const std::string& packageName) const;
	const ModelRef* FindByFamily(const std::string& family) const;
	size_t VehicleCount() const { return vehicles.size(); }
	size_t CharacterCount() const { return characters.size(); }
};

} // namespace apb
