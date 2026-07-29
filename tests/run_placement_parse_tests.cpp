#include "../Source/APBReloaded/Domain/APBDistrictPlacement.h"

#include <cmath>
#include <iostream>
#include <string>
#include <type_traits>
#include <utility>

using namespace apb;

static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::cerr << "FAIL: " << m << "\n"; ++fails; } else { std::cout << "PASS: " << m << "\n"; } } while (0)

template <typename T, typename = void>
struct HasRotation : std::false_type {};

template <typename T>
struct HasRotation<T, std::void_t<decltype(std::declval<T>().rotation)>> : std::true_type {};

template <typename T, typename = void>
struct HasRotationPresent : std::false_type {};

template <typename T>
struct HasRotationPresent<T, std::void_t<decltype(std::declval<T>().rotation_present)>> : std::true_type {};

template <typename T, typename = void>
struct HasScalePresent : std::false_type {};

template <typename T>
struct HasScalePresent<T, std::void_t<decltype(std::declval<T>().scale_present)>> : std::true_type {};

template <typename T, typename = void>
struct HasRejectedRows : std::false_type {};

template <typename T>
struct HasRejectedRows<T, std::void_t<decltype(std::declval<T>().rejected_rows)>> : std::true_type {};

template <typename T, typename = void>
struct HasJsonParsed : std::false_type {};

template <typename T>
struct HasJsonParsed<T, std::void_t<decltype(std::declval<T>().json_parsed)>> : std::true_type {};

template <typename T, typename = void>
struct HasValidManifest : std::false_type {};

template <typename T>
struct HasValidManifest<T, std::void_t<decltype(std::declval<T>().valid_manifest)>> : std::true_type {};

template <typename T, typename = void>
struct HasPlacementMetadata : std::false_type {};

template <typename T>
struct HasPlacementMetadata<T, std::void_t<
	decltype(std::declval<T>().ue_path_present),
	decltype(std::declval<T>().package_present),
	decltype(std::declval<T>().actor),
	decltype(std::declval<T>().actor_present),
	decltype(std::declval<T>().actor_class),
	decltype(std::declval<T>().actor_class_present),
	decltype(std::declval<T>().mesh_source),
	decltype(std::declval<T>().mesh_source_present),
	decltype(std::declval<T>().bound),
	decltype(std::declval<T>().bound_present)>> : std::true_type {};

template <typename T, typename = void>
struct HasManifestSchema : std::false_type {};

template <typename T>
struct HasManifestSchema<T, std::void_t<
	decltype(std::declval<T>().source_packages),
	decltype(std::declval<T>().source_packages_present),
	decltype(std::declval<T>().layout),
	decltype(std::declval<T>().layout_present),
	decltype(std::declval<T>().layout_note),
	decltype(std::declval<T>().layout_note_present),
	decltype(std::declval<T>().actor_count),
	decltype(std::declval<T>().actor_count_present),
	decltype(std::declval<T>().unresolved_actor_count),
	decltype(std::declval<T>().unresolved_actor_count_present),
	decltype(std::declval<T>().player_start_present),
	decltype(std::declval<T>().vehicle_start_present),
	decltype(std::declval<T>().bound_count_present),
	decltype(std::declval<T>().manifest_total_present),
	decltype(std::declval<T>().hit_rate_present)>> : std::true_type {};

template <typename T>
static bool RotationEquals(const T& placement, double x, double y, double z) {
	if constexpr (HasRotation<T>::value) {
		return placement.rotation.x == x && placement.rotation.y == y && placement.rotation.z == z;
	}
	return false;
}

template <typename T>
static bool RotationPresent(const T& placement) {
	if constexpr (HasRotationPresent<T>::value) return placement.rotation_present;
	return false;
}

template <typename T>
static bool ScalePresent(const T& placement) {
	if constexpr (HasScalePresent<T>::value) return placement.scale_present;
	return false;
}

template <typename T>
static size_t RejectedCount(const T& manifest) {
	if constexpr (HasRejectedRows<T>::value) return manifest.rejected_rows.size();
	return 0;
}

template <typename T>
static bool AllRejectedRowsHaveReasons(const T& manifest) {
	if constexpr (HasRejectedRows<T>::value) {
		for (const auto& row : manifest.rejected_rows) {
			if (row.reason.empty()) return false;
		}
		return true;
	}
	return false;
}

template <typename T>
static bool JsonParsed(const T& manifest) {
	if constexpr (HasJsonParsed<T>::value) return manifest.json_parsed;
	return false;
}

template <typename T>
static bool ValidManifest(const T& manifest) {
	if constexpr (HasValidManifest<T>::value) return manifest.valid_manifest;
	return false;
}

template <typename T>
static bool CompleteMetadataMatches(const T& placement) {
	if constexpr (HasPlacementMetadata<T>::value) {
		return placement.ue_path == "/Game/Imported/Mesh.Mesh" && placement.ue_path_present
			&& placement.package == "FixturePackage" && placement.package_present
			&& placement.actor == "Actor_1" && placement.actor_present
			&& placement.actor_class == "PrefabInstance" && placement.actor_class_present
			&& placement.mesh_source == "component" && placement.mesh_source_present
			&& placement.bound && placement.bound_present;
	}
	return false;
}

template <typename T>
static bool OptionalMetadataAbsent(const T& placement) {
	if constexpr (HasPlacementMetadata<T>::value) {
		return !placement.ue_path_present && !placement.package_present
			&& !placement.actor_present && !placement.actor_class_present
			&& !placement.mesh_source_present && !placement.bound_present;
	}
	return false;
}

template <typename T>
static bool ManifestSchemaMatches(const T& manifest) {
	if constexpr (HasManifestSchema<T>::value) {
		return manifest.source_packages_present && manifest.source_packages.size() == 2
			&& manifest.source_packages[0] == "A" && manifest.source_packages[1] == "B"
			&& manifest.layout_present && manifest.layout == "fixture_layout"
			&& manifest.layout_note_present && manifest.layout_note == "fixture note"
			&& manifest.actor_count_present && manifest.actor_count == 3
			&& manifest.unresolved_actor_count_present && manifest.unresolved_actor_count == 2
			&& manifest.player_start_present && manifest.player_start.x == 10.0
			&& manifest.vehicle_start_present && manifest.vehicle_start.x == 40.0
			&& manifest.bound_count_present && manifest.bound_count == 1
			&& manifest.manifest_total_present && manifest.manifest_total == 3
			&& manifest.hit_rate_present && manifest.hit_rate == 0.5;
	}
	return false;
}

static std::string ManifestWithPlacement(const std::string& placement) {
	return std::string("{\"district_id\":\"Financial\",\"source_package\":\"Fixture\",\"provenance\":\"real\",\"renderable_count\":1,\"total_row_count\":1,\"reason_histogram\":{},\"placements\":[")
		+ placement + "]}";
}

static void GateMalformedRejected() {
	DistrictManifestPure manifest;
	const std::string broken = "{\"district_id\":\"Financial\",\"source_package\":\"Fixture\",\"placements\":[{\"mesh_id\":\"fabricated\",\"location\":[1,2,3]}";
	CHECK(!ParsePlacementManifestJson(broken, manifest),
		"GATE_MALFORMED_REJECTED structurally broken JSON containing mesh_id is rejected");
}

static void GateRotationParsed() {
	DistrictManifestPure manifest;
	const bool parsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"mesh\",\"location\":[1,2,3],\"rotation\":[0,90,0]}"), manifest);
	const bool exact = parsed && manifest.placements.size() == 1
		&& RotationEquals(manifest.placements[0], 0.0, 90.0, 0.0);
	CHECK(exact, "GATE_ROTATION_PARSED rotation [0,90,0] round-trips exactly");
}

static void GateScaleParsed() {
	DistrictManifestPure manifest;
	const bool parsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"mesh\",\"location\":[1,2,3],\"scale\":[2.0,1.0,0.5]}"), manifest);
	const bool exact = parsed && manifest.placements.size() == 1
		&& manifest.placements[0].scale.x == 2.0
		&& manifest.placements[0].scale.y == 1.0
		&& manifest.placements[0].scale.z == 0.5
		&& ScalePresent(manifest.placements[0]);
	CHECK(exact, "GATE_SCALE_PARSED scale [2,1,0.5] round-trips exactly");
}

static void GateMissingnessParity() {
	DistrictManifestPure absent;
	DistrictManifestPure explicitDefaults;
	const bool absentParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"absent\",\"location\":[1,2,3]}"), absent);
	const bool explicitParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:2:mesh\",\"mesh_id\":\"explicit\",\"location\":[1,2,3],\"rotation\":[0,0,0],\"scale\":[1,1,1]}"), explicitDefaults);
	const bool distinguishable = absentParsed && explicitParsed
		&& absent.placements.size() == 1 && explicitDefaults.placements.size() == 1
		&& !RotationPresent(absent.placements[0]) && RotationPresent(explicitDefaults.placements[0])
		&& !ScalePresent(absent.placements[0]) && ScalePresent(explicitDefaults.placements[0])
		&& RotationEquals(absent.placements[0], 0.0, 0.0, 0.0)
		&& RotationEquals(explicitDefaults.placements[0], 0.0, 0.0, 0.0)
		&& absent.placements[0].scale.x == 1.0 && absent.placements[0].scale.y == 1.0 && absent.placements[0].scale.z == 1.0
		&& explicitDefaults.placements[0].scale.x == 1.0 && explicitDefaults.placements[0].scale.y == 1.0 && explicitDefaults.placements[0].scale.z == 1.0;
	CHECK(distinguishable,
		"GATE_OPTIONAL_TRANSFORM_ABSENCE absent rotation/scale differ from explicit default-valued fields");
}

static void GateMissingSourceId() {
	DistrictManifestPure manifest;
	ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"mesh_id\":\"mesh\",\"location\":[1,2,3]}"), manifest);
	CHECK(manifest.placements.empty() && RejectedCount(manifest) == 1
		&& manifest.rejected_rows[0].reason == "MissingSourceId",
		"GATE_MISSING_SOURCE_ID missing source_id is rejected and counted");
}

static void GateNonRenderableReason() {
	DistrictManifestPure manifest;
	ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:3:collision\",\"mesh_id\":\"collision\",\"location\":[1,2,3],\"edge\":\"collision\",\"reason\":\"collision_only\"}"), manifest);
	CHECK(manifest.placements.empty() && RejectedCount(manifest) == 1
		&& manifest.rejected_rows[0].reason == "collision_only",
		"GATE_NON_RENDERABLE_REASON collision_only is counted and never renderable");
}

static void GateRequiredFields() {
	DistrictManifestPure missingMesh;
	DistrictManifestPure missingLocation;
	const bool meshAccepted = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"location\":[1,2,3]}"), missingMesh);
	const bool locationAccepted = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:2:mesh\",\"mesh_id\":\"mesh\"}"), missingLocation);
	const bool reasonCoded = meshAccepted && locationAccepted
		&& RejectedCount(missingMesh) == 1 && RejectedCount(missingLocation) == 1
		&& AllRejectedRowsHaveReasons(missingMesh) && AllRejectedRowsHaveReasons(missingLocation);
	CHECK(reasonCoded,
		"GATE_REQUIRED_FIELDS missing mesh_id or location is rejected with a machine-readable reason");
}

static void GateEmptyPlacements() {
	DistrictManifestPure manifest;
	const bool validWithContent = ParsePlacementManifestJson(
		"{\"district_id\":\"Financial\",\"source_package\":\"Fixture\",\"placements\":[]}", manifest);
	CHECK(!validWithContent && JsonParsed(manifest) && !ValidManifest(manifest)
		&& manifest.placements.empty() && RejectedCount(manifest) == 0,
		"GATE_EMPTY_PLACEMENTS well-formed zero-row manifest is parsed but not valid-with-content");
}

static void GateCountConservation() {
	DistrictManifestPure manifest;
	const std::string text =
		"{\"district_id\":\"Financial\",\"source_package\":\"Fixture\",\"placements\":["
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"accepted\",\"location\":[1,2,3]},"
		"{\"source_id\":\"sha:2:mesh\",\"location\":[4,5,6]},"
		"{\"source_id\":\"sha:3:mesh\",\"mesh_id\":\"missing-location\"}]}";
	ParsePlacementManifestJson(text, manifest);
	const size_t rejected = RejectedCount(manifest);
	CHECK(manifest.placements.size() + rejected == 3 && rejected == 2
		&& AllRejectedRowsHaveReasons(manifest),
		"GATE_COUNT_CONSERVATION accepted + rejected equals N and every rejection has a reason code");
}

static void GateExtractorPresenceFlags() {
	DistrictManifestPure fallback;
	DistrictManifestPure contradictory;
	const bool fallbackParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"fallback\",\"location\":[1,2,3],\"rotation\":[0,0,0],\"rotation_present\":false,\"scale\":[1,1,1],\"scale_present\":false}"), fallback);
	const bool contradictoryParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:2:mesh\",\"mesh_id\":\"contradictory\",\"location\":[1,2,3],\"rotation\":\"invalid\"}"), contradictory);
	const bool contractHeld = fallbackParsed && fallback.placements.size() == 1
		&& RotationPresent(fallback.placements[0]) && ScalePresent(fallback.placements[0])
		&& contradictoryParsed && RejectedCount(contradictory) == 1
		&& AllRejectedRowsHaveReasons(contradictory);
	CHECK(contractHeld,
		"GATE_EXTRACTOR_PRESENCE_FLAGS JSON field presence is authoritative and invalid values reject the row");
}

static void GatePlacementMetadataPresence() {
	DistrictManifestPure complete;
	DistrictManifestPure absent;
	const bool completeParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"complete\",\"ue_path\":\"/Game/Imported/Mesh.Mesh\",\"location\":[1,2,3],\"package\":\"FixturePackage\",\"actor\":\"Actor_1\",\"actor_class\":\"PrefabInstance\",\"mesh_source\":\"component\",\"bound\":true}"), complete);
	const bool absentParsed = ParsePlacementManifestJson(ManifestWithPlacement(
		"{\"source_id\":\"sha:2:mesh\",\"mesh_id\":\"absent\",\"location\":[1,2,3]}"), absent);
	const bool contractHeld = completeParsed && absentParsed
		&& complete.placements.size() == 1 && absent.placements.size() == 1
		&& CompleteMetadataMatches(complete.placements[0])
		&& OptionalMetadataAbsent(absent.placements[0]);
	CHECK(contractHeld,
		"GATE_PLACEMENT_METADATA_PRESENCE optional row metadata keeps values separate from presence");
}

static void GateManifestSchemaPresence() {
	DistrictManifestPure manifest;
	const std::string text =
		"{\"district_id\":\"Financial\",\"source_package\":\"Fixture\","
		"\"source_packages\":[\"A\",\"B\"],\"layout\":\"fixture_layout\",\"layout_note\":\"fixture note\","
		"\"actor_count\":3,\"unresolved_actor_count\":2,\"player_start\":[10,20,30],\"vehicle_start\":[40,50,60],"
		"\"bound_count\":1,\"manifest_total\":3,\"hit_rate\":0.5,"
		"\"placements\":[{\"source_id\":\"sha:1:mesh\",\"mesh_id\":\"mesh\",\"location\":[1,2,3]}]}";
	const bool parsed = ParsePlacementManifestJson(text, manifest);
	CHECK(parsed && ManifestSchemaMatches(manifest),
		"GATE_MANIFEST_SCHEMA_PRESENCE documented root fields retain typed values and presence");
}

static void GateAuthoritativeManifestSchema() {
	DistrictManifestPure manifest;
	const std::string text =
		"{\"district_id\":\"Financial\",\"source_package\":\"FinancialDistrict_Block09\","
		"\"provenance\":\"real\",\"renderable_count\":1,\"total_row_count\":2,"
		"\"reason_histogram\":{\"collision_only\":1},\"placements\":["
		"{\"source_id\":\"sha:17:mesh\",\"mesh_id\":\"mesh\",\"ue_path\":\"/Game/Imported/Districts/Financial/mesh.mesh\",\"package\":\"FinancialDistrict_Block09\",\"actor\":\"Actor_1\",\"edge\":\"mesh\",\"location\":[1,2,3]},"
		"{\"source_id\":\"sha:17:collision\",\"mesh_id\":\"collision\",\"package\":\"FinancialDistrict_Block09\",\"actor\":\"Actor_1\",\"edge\":\"collision\",\"location\":[1,2,3],\"reason\":\"collision_only\"}]}";
	const bool parsed = ParsePlacementManifestJson(text, manifest);
	CHECK(parsed && manifest.provenance_present && manifest.provenance == "real"
		&& manifest.renderable_count_present && manifest.renderable_count == 1
		&& manifest.total_row_count_present && manifest.total_row_count == 2
		&& manifest.reason_histogram_present && manifest.reason_histogram.size() == 1
		&& manifest.placements.size() == 1 && RejectedCount(manifest) == 1,
		"GATE_AUTHORITATIVE_MANIFEST_SCHEMA real manifest metadata and non-renderable counts round-trip");
}

static bool RunGate(const std::string& gate) {
	if (gate == "GATE_MALFORMED_REJECTED") GateMalformedRejected();
	else if (gate == "GATE_ROTATION_PARSED") GateRotationParsed();
	else if (gate == "GATE_SCALE_PARSED") GateScaleParsed();
	else if (gate == "GATE_MISSINGNESS_PARITY") GateMissingnessParity();
	else if (gate == "GATE_MISSING_SOURCE_ID") GateMissingSourceId();
	else if (gate == "GATE_NON_RENDERABLE_REASON") GateNonRenderableReason();
	else if (gate == "GATE_REQUIRED_FIELDS") GateRequiredFields();
	else if (gate == "GATE_EMPTY_PLACEMENTS") GateEmptyPlacements();
	else if (gate == "GATE_COUNT_CONSERVATION") GateCountConservation();
	else if (gate == "GATE_EXTRACTOR_PRESENCE_FLAGS") GateExtractorPresenceFlags();
	else if (gate == "GATE_PLACEMENT_METADATA_PRESENCE") GatePlacementMetadataPresence();
	else if (gate == "GATE_MANIFEST_SCHEMA_PRESENCE") GateManifestSchemaPresence();
	else if (gate == "GATE_AUTHORITATIVE_MANIFEST_SCHEMA") GateAuthoritativeManifestSchema();
	else return false;
	return true;
}

int main(int argc, char** argv) {
	if (argc > 1) {
		if (!RunGate(argv[1])) {
			std::cerr << "Unknown gate: " << argv[1] << "\n";
			return 2;
		}
	} else {
		GateMalformedRejected();
		GateRotationParsed();
		GateScaleParsed();
		GateMissingnessParity();
		GateMissingSourceId();
		GateNonRenderableReason();
		GateRequiredFields();
		GateEmptyPlacements();
		GateCountConservation();
		GateExtractorPresenceFlags();
		GatePlacementMetadataPresence();
		GateManifestSchemaPresence();
		GateAuthoritativeManifestSchema();
	}
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
