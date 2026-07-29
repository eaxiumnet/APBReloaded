#include "APBDistrictPlacement.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <set>
#include <string>
#include <vector>

#if __has_include("APBPlacementBinding.h")
#include "APBPlacementBinding.h"
#define APB_HAS_PLACEMENT_BINDING 1
#else
#define APB_HAS_PLACEMENT_BINDING 0
#endif

static int fails = 0;

#define CHECK(condition, message) do { \
	if (!(condition)) { ++fails; std::printf("FAIL: %s\n", message); } \
	else { std::printf("PASS: %s\n", message); } \
} while (0)

static std::string ReadFile(const std::string& path)
{
	std::ifstream input(path, std::ios::binary);
	return input ? std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()) : std::string();
}

struct TestPlacement
{
	std::string source_id;
	std::string mesh_id;
	std::string ue_path;
	std::string package;
	apb::PlacementVec3 location;
	apb::PlacementVec3 rotation;
	apb::PlacementVec3 scale;
};

static TestPlacement MakePlacement()
{
	TestPlacement entry;
	entry.source_id = "pkgsha:17:mesh";
	entry.mesh_id = "WF_Building_LOD_0";
	entry.ue_path = "/Game/Imported/Districts/Waterfront/WF_Building_LOD_0.WF_Building_LOD_0";
	entry.package = "Waterfront_Block05";
	entry.location = {100.0, 200.0, 300.0};
	entry.rotation = {0.0, 0.0, 0.0};
	entry.scale = {1.0, 1.0, 1.0};
	return entry;
}

#if APB_HAS_PLACEMENT_BINDING
static int SurvivingCount(const std::vector<TestPlacement>& placements)
{
	std::set<std::string> keys;
	for (const auto& placement : placements) {
		const apb::PlacementIdentity identity = apb::PlacementDedupKey(placement.source_id);
		if (identity.valid) keys.insert(identity.key);
	}
	return static_cast<int>(keys.size());
}
#else
static int SurvivingCount(const std::vector<TestPlacement>& placements)
{
	std::set<std::string> keys;
	for (const auto& placement : placements) {
		keys.insert(placement.mesh_id + "@"
			+ std::to_string(placement.location.x) + ","
			+ std::to_string(placement.location.y) + ","
			+ std::to_string(placement.location.z));
	}
	return static_cast<int>(keys.size());
}
#endif

static void GateCrossDistrict()
{
#if APB_HAS_PLACEMENT_BINDING
	const char* stems[] = {
		"PGBD_B09_AstragalSign_LOD_0",
		"PGBD_B09_CasaMajorCo_LOD_0",
		"PGBD_B09_SanParoPost_LOD_0",
		"PGBD_B09_Scaffolding_LOD_0",
		"PGBD_B09_Scaffolding1_LOD_0",
	};
	bool allFinancial = true;
	for (const char* stem : stems) {
		const apb::PlacementBinding binding = apb::BuildPlacementBinding(
			stem,
			std::string("/Game/Imported/Districts/Beacon/") + stem + "." + stem,
			"FinancialDistrict_Block09",
			"Financial");
		allFinancial = allFinancial && binding.valid && !binding.candidate_paths.empty();
		for (const std::string& candidate : binding.candidate_paths) {
			allFinancial = allFinancial
				&& candidate.find("/Game/Imported/Districts/Financial/") == 0
				&& candidate.find("/Beacon/") == std::string::npos;
		}
	}
	CHECK(allFinancial,
		"GATE_CROSS_DISTRICT all five Financial PGBD_B09 stems never resolve to Beacon");
#else
	CHECK(false,
		"GATE_CROSS_DISTRICT Waterfront provenance never resolves to a Financial path");
#endif
}

static void GateSourceIdDedup()
{
	auto first = MakePlacement();
	auto second = first;
	second.source_id = "pkgsha:18:mesh";
	const int distinctCount = SurvivingCount({first, second});
	second.source_id = first.source_id;
	const int duplicateCount = SurvivingCount({first, second});
	CHECK(distinctCount == 2 && duplicateCount == 1,
		"GATE_SOURCE_ID_DEDUP distinct source_id rows survive and repeated source_id collapses");
}

static void GateMissingSourceId()
{
#if APB_HAS_PLACEMENT_BINDING
	const apb::PlacementIdentity identity = apb::PlacementDedupKey("");
	CHECK(!identity.valid && identity.reason_code == "MissingSourceId" && identity.key.empty(),
		"GATE_MISSING_SOURCE_ID missing identity fails closed with MissingSourceId");
#else
	CHECK(false, "GATE_MISSING_SOURCE_ID missing identity fails closed with MissingSourceId");
#endif
}

static void GateManifestPreference()
{
#if APB_HAS_PLACEMENT_BINDING
	const std::vector<apb::ManifestCandidate> candidates = apb::PlacementManifestCandidates("Financial");
	CHECK(candidates.size() == 3
		&& candidates[0].file_name == "Financial_Block09_realv2.json" && !candidates[0].synthetic
		&& candidates[1].file_name == "Financial_Block09_real.json" && !candidates[1].synthetic
		&& candidates[2].file_name == "Financial_Block09_bound.json" && candidates[2].synthetic
		&& candidates[2].reason_code == "SyntheticBoundManifest",
		"GATE_MANIFEST_PREFERENCE realv2 then real then reason-coded synthetic bound");
#else
	CHECK(false, "GATE_MANIFEST_PREFERENCE realv2 then real then reason-coded synthetic bound");
#endif
}

static void GateManifestSelectionLogging()
{
	const std::string loader = ReadFile(
		R"(D:\APBReloaded\Source\APBReloaded\Systems\District\APBDistrictPlacementLoader.cpp)");
	CHECK(loader.find("UE_LOG(LogTemp, Warning,\n\t\t\t\tTEXT(\"PLACEMENT_MANIFEST_SYNTHETIC") != std::string::npos
		&& loader.find("TEXT(\"PLACEMENT_MANIFEST_CHOSEN") != std::string::npos,
		"GATE_MANIFEST_SELECTION_LOGGING synthetic warning and chosen-file markers stay wired");
}

static void GateStemVariantsPreserved()
{
#if APB_HAS_PLACEMENT_BINDING
	const apb::PlacementBinding bare = apb::BuildPlacementBinding(
		"WF_Building", "", "Waterfront_Block05", "Waterfront");
	const apb::PlacementBinding lodUnderscore = apb::BuildPlacementBinding(
		"WF_Building_LOD_0", "", "Waterfront_Block05", "Waterfront");
	const apb::PlacementBinding lodCompact = apb::BuildPlacementBinding(
		"WF_Building_LOD0", "", "Waterfront_Block05", "Waterfront");
	const auto hasPath = [](const apb::PlacementBinding& binding, const std::string& path) {
		for (const auto& candidate : binding.candidate_paths) {
			if (candidate == path) return true;
			if (candidate.find("/Game/Imported/Districts/Waterfront/") != 0) return false;
		}
		return false;
	};
	const std::string prefix = "/Game/Imported/Districts/Waterfront/";
	CHECK(bare.valid && lodUnderscore.valid && lodCompact.valid
		&& hasPath(bare, prefix + "WF_Building.WF_Building")
		&& hasPath(bare, prefix + "WF_Building_LOD_0.WF_Building_LOD_0")
		&& hasPath(bare, prefix + "WF_Building_LOD0.WF_Building_LOD0")
		&& hasPath(lodUnderscore, prefix + "WF_Building.WF_Building")
		&& hasPath(lodCompact, prefix + "WF_Building.WF_Building"),
		"GATE_STEM_VARIANTS_PRESERVED bare, _LOD_0, _LOD0, and stripped stems stay in the expected district");
#else
	CHECK(false,
		"GATE_STEM_VARIANTS_PRESERVED bare, _LOD_0, _LOD0, and stripped stems stay in the expected district");
#endif
}

static void GateProvenanceMismatch()
{
#if APB_HAS_PLACEMENT_BINDING
	const apb::PlacementBinding binding = apb::BuildPlacementBinding(
		"WF_Building", "", "Waterfront_Block05", "Financial");
	CHECK(!binding.valid && binding.reason_code == "district_provenance_mismatch",
		"GATE_PROVENANCE_MISMATCH package and manifest district mismatch fails closed");
#else
	CHECK(false,
		"GATE_PROVENANCE_MISMATCH package and manifest district mismatch fails closed");
#endif
}

int main()
{
	GateCrossDistrict();
	GateSourceIdDedup();
	GateMissingSourceId();
	GateManifestPreference();
	GateManifestSelectionLogging();
	GateStemVariantsPreserved();
	GateProvenanceMismatch();
	std::printf("FAILS=%d\n", fails);
	return fails == 0 ? 0 : 1;
}
