#pragma once

#include "APBDistrictPlacement.h"

#include <algorithm>
#include <string>
#include <vector>

namespace apb {

struct PlacementBinding {
	bool valid = false;
	std::string expected_folder;
	std::string reason_code;
	std::vector<std::string> candidate_paths;
};

struct PlacementIdentity {
	bool valid = false;
	std::string key;
	std::string reason_code;
};

struct ManifestCandidate {
	std::string file_name;
	bool synthetic = false;
	std::string reason_code;
};

namespace placement_binding_detail {

inline std::string LowerAscii(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return value;
}

inline bool EndsWith(const std::string& value, const std::string& suffix)
{
	return value.size() >= suffix.size()
		&& value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

inline std::string RemoveAll(std::string value, const std::string& token)
{
	for (std::size_t pos = value.find(token); pos != std::string::npos; pos = value.find(token, pos)) {
		value.erase(pos, token.size());
	}
	return value;
}

inline void AddUnique(std::vector<std::string>& values, const std::string& value)
{
	if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) {
		values.push_back(value);
	}
}

inline std::string DistrictFolder(const std::string& provenance)
{
	const std::string lower = LowerAscii(provenance);
	if (lower.find("waterfront") != std::string::npos) return "Waterfront";
	if (lower.find("asylum") != std::string::npos || lower.find("abington") != std::string::npos) return "Asylum";
	if (lower.find("beacon") != std::string::npos) return "Beacon";
	if (lower.find("crate") != std::string::npos) return "Crate";
	if (lower.find("social") != std::string::npos || lower.find("breakwater") != std::string::npos) return "Social";
	if (lower.find("financial") != std::string::npos) return "Financial";
	return {};
}

}

inline PlacementBinding BuildPlacementBinding(
	const std::string& meshId,
	const std::string& uePath,
	const std::string& package,
	const std::string& manifestDistrictId)
{
	PlacementBinding binding;
	if (meshId.empty()) {
		binding.reason_code = "missing_mesh_id";
		return binding;
	}

	const std::string packageFolder = placement_binding_detail::DistrictFolder(package);
	if (packageFolder.empty()) {
		binding.reason_code = "unknown_package_district";
		return binding;
	}

	const std::string manifestFolder = placement_binding_detail::DistrictFolder(manifestDistrictId);
	if (manifestFolder.empty()) {
		binding.expected_folder = packageFolder;
		binding.reason_code = "unknown_manifest_district";
		return binding;
	}
	if (packageFolder != manifestFolder) {
		binding.expected_folder = packageFolder;
		binding.reason_code = "district_provenance_mismatch";
		return binding;
	}

	binding.valid = true;
	binding.expected_folder = packageFolder;
	binding.reason_code = "ok";
	const std::string prefix = "/Game/Imported/Districts/" + packageFolder + "/";
	if (uePath.compare(0, prefix.size(), prefix) == 0) {
		placement_binding_detail::AddUnique(binding.candidate_paths, uePath);
	}

	std::vector<std::string> stems;
	placement_binding_detail::AddUnique(stems, meshId);
	if (!placement_binding_detail::EndsWith(meshId, "_LOD_0")) {
		placement_binding_detail::AddUnique(stems, meshId + "_LOD_0");
	}
	if (!placement_binding_detail::EndsWith(meshId, "_LOD0")) {
		placement_binding_detail::AddUnique(stems, meshId + "_LOD0");
	}
	placement_binding_detail::AddUnique(stems, placement_binding_detail::RemoveAll(meshId, "_LOD_0"));
	placement_binding_detail::AddUnique(stems, placement_binding_detail::RemoveAll(meshId, "_LOD0"));
	for (const std::string& stem : stems) {
		placement_binding_detail::AddUnique(binding.candidate_paths, prefix + stem + "." + stem);
	}
	return binding;
}

inline PlacementIdentity PlacementDedupKey(const std::string& sourceId)
{
	PlacementIdentity identity;
	if (sourceId.empty()) {
		identity.reason_code = "MissingSourceId";
		return identity;
	}
	identity.valid = true;
	identity.key = sourceId;
	identity.reason_code = "Ok";
	return identity;
}

inline std::vector<ManifestCandidate> PlacementManifestCandidates(const std::string& districtId)
{
	const std::string base = PlacementBaseNameForDistrict(districtId);
	return {
		{base + "_realv2.json", false, ""},
		{base + "_real.json", false, ""},
		{base + "_bound.json", true, "SyntheticBoundManifest"},
	};
}

}
