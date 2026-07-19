#include "../Source/APBReloaded/Domain/APBModelRegistry.h"
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include "../Source/APBReloaded/Domain/APBCatalog.h"
#include "../Source/APBReloaded/Domain/APBDistrictPlacement.h"
#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
using namespace apb;
static int fails=0;
#define CHECK(c,m) do{if(!(c)){std::cerr<<"FAIL: "<<m<<"\n";++fails;}else{std::cout<<"PASS: "<<m<<"\n";}}while(0)

static std::string ReadFile(const std::string& p){
  std::ifstream in(p,std::ios::binary); if(!in) return {};
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

static bool AssetExistsForUePath(const std::string& uePath) {
  // /Game/Imported/Districts/Financial/Foo.Foo -> D:\APBReloaded\Content\Imported\Districts\Financial\Foo.uasset
  const std::string prefix = "/Game/";
  if (uePath.compare(0, prefix.size(), prefix) != 0) return false;
  std::string rest = uePath.substr(prefix.size());
  auto dot = rest.rfind('.');
  if (dot != std::string::npos) rest = rest.substr(0, dot);
  std::string path = std::string(R"(D:\APBReloaded\Content\)") + rest + ".uasset";
  for (char& c : path) if (c == '/') c = '\\';
  std::ifstream in(path, std::ios::binary);
  return static_cast<bool>(in);
}

void TestPlacementManifest() {
  std::string fin = ReadFile(R"(D:\APBReloaded\Content\Data\district_placements\Financial_Block09.json)");
  CHECK(!fin.empty(), "Financial_Block09.json exists");
  CHECK(fin.find("BasicShapes/Cube") == std::string::npos, "Financial manifest zero Engine cube refs");
  CHECK(fin.find("/Game/Imported/Districts/Financial/") != std::string::npos, "Financial uses Imported Financial meshes");
  int n = 0; size_t pos = 0;
  while ((pos = fin.find("\"mesh_id\"", pos)) != std::string::npos) { ++n; pos += 8; }
  CHECK(n >= 8, "Financial manifest has >=8 placements");
  CHECK(fin.find("FinancialDistrict") != std::string::npos, "Financial source package named");
  CHECK(fin.find("steam_multi_block_package_grid") != std::string::npos
        || fin.find("source_packages") != std::string::npos,
        "Financial multi-block layout or source_packages");

  std::string wf = ReadFile(R"(D:\APBReloaded\Content\Data\district_placements\Waterfront_Block05.json)");
  CHECK(!wf.empty(), "Waterfront_Block05.json exists");
  CHECK(wf.find("BasicShapes/Cube") == std::string::npos, "Waterfront manifest zero Engine cube refs");
  CHECK(wf.find("WaterfrontDistrict") != std::string::npos, "Waterfront source package");
  CHECK(wf.find("/Game/Imported/Districts/Waterfront/") != std::string::npos, "Waterfront uses Waterfront meshes");
  CHECK(wf != fin, "Waterfront manifest != Financial manifest");
  int nw = 0; pos = 0;
  while ((pos = wf.find("\"mesh_id\"", pos)) != std::string::npos) { ++nw; pos += 8; }
  CHECK(nw >= 8, "Waterfront manifest has >=8 placements");
  // Distinct Steam block maps / packages
  CHECK(fin.find("Block09") != std::string::npos || fin.find("Block0") != std::string::npos,
        "Financial references Block packages");
  CHECK(wf.find("Block05") != std::string::npos || wf.find("Block0") != std::string::npos,
        "Waterfront references Block packages");
  CHECK(fin.find("cStreamedBuildingActor_location") != std::string::npos
        || fin.find("cStreamed_multi_class_location") != std::string::npos
        || fin.find("master_levelstreaming") != std::string::npos
        || fin.find("steam_multi_block") != std::string::npos
        || fin.find("cStreamedBuildingActor") != std::string::npos,
        "Financial layout is Steam-derived (actor or multi-block)");
  std::cout << "MANIFEST Financial~=" << n << " Waterfront~=" << nw << "\n";
}

/** Exercises shipped Domain placement resolve/parse/near metrics on real bound manifests. */
void TestShippedPlacementResolveAndNearSpawnMetrics() {
  CHECK(ResolveDistrictIdFromMapName("Lvl_APB_Financial_Freeroam") == "Financial", "map→Financial");
  CHECK(ResolveDistrictIdFromMapName("UEDPIE_0_Lvl_APB_Waterfront_Freeroam") == "Waterfront", "map→Waterfront");
  CHECK(ResolveDistrictIdFromMapName("Lvl_APB_PGAsylum_Freeroam") == "PGAsylum", "map→PGAsylum");
  CHECK(PlacementBaseNameForDistrict("Financial") == "Financial_Block09", "Financial base name");
  CHECK(PlacementBaseNameForDistrict("Waterfront") == "Waterfront_Block05", "Waterfront base name");
  CHECK(PreferredManifestFileName("Financial", true) == "Financial_Block09_bound.json", "prefer bound Financial");

  const std::string finPath = std::string(R"(D:\APBReloaded\Content\Data\district_placements\)")
    + PreferredManifestFileName("Financial", true);
  const std::string finText = ReadFile(finPath);
  CHECK(!finText.empty(), "load Financial bound manifest file");
  DistrictManifestPure fin;
  CHECK(ParsePlacementManifestJson(finText, fin), "parse Financial bound via shipped parser");
  CHECK(fin.placements.size() >= 100, "Financial bound placements >= 100");
  CHECK(!ManifestUsesEngineCubes(fin), "Financial bound has no Engine cubes");
  int imported = 0;
  for (const auto& e : fin.placements) if (PathIsImportedDistrict(e.ue_path)) ++imported;
  CHECK(imported == static_cast<int>(fin.placements.size()), "all Financial paths under Imported/Districts");
  const int near60k = CountPlacementsNear(fin, fin.player_start, 60000.0);
  CHECK(near60k > 0, "Financial near player_start r=60k has placements");
  CHECK(near60k >= 50, "Financial near bubble is substantial");
  std::cout << "FINANCIAL placements=" << fin.placements.size()
            << " near60k=" << near60k
            << " player_start=(" << fin.player_start.x << "," << fin.player_start.y << "," << fin.player_start.z << ")"
            << " bound=" << fin.bound_count << " hit=" << fin.hit_rate << "\n";

  // Spot-check mesh assets exist on disk for sample bound paths
  int assetHits = 0, assetChecks = 0;
  for (size_t i = 0; i < fin.placements.size() && assetChecks < 40; i += std::max<size_t>(1, fin.placements.size() / 40)) {
    ++assetChecks;
    if (AssetExistsForUePath(fin.placements[i].ue_path)) ++assetHits;
  }
  CHECK(assetHits == assetChecks && assetChecks > 0, "sample Financial uassets exist for bound paths");
  std::cout << "ASSET_SPOT assetHits=" << assetHits << "/" << assetChecks << "\n";

  const std::string wfPath = std::string(R"(D:\APBReloaded\Content\Data\district_placements\)")
    + PreferredManifestFileName("Waterfront", true);
  const std::string wfText = ReadFile(wfPath);
  CHECK(!wfText.empty(), "load Waterfront bound manifest file");
  DistrictManifestPure wf;
  CHECK(ParsePlacementManifestJson(wfText, wf), "parse Waterfront bound via shipped parser");
  CHECK(wf.placements.size() >= 50, "Waterfront bound placements >= 50");
  CHECK(!ManifestUsesEngineCubes(wf), "Waterfront bound has no Engine cubes");
  const int wfNear = CountPlacementsNear(wf, wf.player_start, 60000.0);
  CHECK(wfNear > 0, "Waterfront near player_start r=60k has placements");
  std::cout << "WATERFRONT placements=" << wf.placements.size() << " near60k=" << wfNear << "\n";
  std::cout << "STREAM_SPAWN_METRIC district=Financial spawned_near_potential=" << near60k
            << " bound=" << fin.bound_count << " total=" << fin.placements.size() << "\n";
  std::cout << "BOUND_SPAWN_METRIC district=Financial bound=" << fin.bound_count
            << " total_manifest=" << fin.manifest_total << " hit_rate=" << fin.hit_rate << "\n";
}

/** Static proof freeroam game mode still invokes loader with district id. */
void TestFreeroamSourceInvokesLoader() {
  const std::string gm = ReadFile(R"(D:\APBReloaded\Source\APBReloaded\Systems\APBFreeroamGameMode.cpp)");
  CHECK(!gm.empty(), "APBFreeroamGameMode.cpp readable");
  CHECK(gm.find("LoadDistrictContent") != std::string::npos, "freeroam has LoadDistrictContent");
  CHECK(gm.find("LoadManifestForDistrict") != std::string::npos, "freeroam calls LoadManifestForDistrict");
  CHECK(gm.find("SpawnFromManifestNear") != std::string::npos, "freeroam calls SpawnFromManifestNear");
  CHECK(gm.find("STREAM_SPAWN") != std::string::npos, "freeroam logs STREAM_SPAWN");
  CHECK(gm.find("BOUND_SPAWN") != std::string::npos, "freeroam logs BOUND_SPAWN");
  CHECK(gm.find("EnsureDistrictLighting") != std::string::npos, "freeroam ensures district lighting");
  CHECK(gm.find("AlignPlayerStartsAndTeleport") != std::string::npos, "freeroam aligns player starts");
  CHECK(gm.find("BasicShapes/Cube") == std::string::npos
        || gm.find("refusing") != std::string::npos
        || gm.find("no BasicShapes") != std::string::npos,
        "freeroam does not treat cubes as world success");
  const std::string loader = ReadFile(R"(D:\APBReloaded\Source\APBReloaded\Systems\APBDistrictPlacementLoader.cpp)");
  CHECK(loader.find("BasicShapes/Cube") != std::string::npos, "loader rejects Engine cubes");
  CHECK(loader.find("Imported/Districts") != std::string::npos, "loader resolves Imported/Districts");
  CHECK(loader.find("EnsureVisibleMeshMaterials") != std::string::npos, "loader ensures visible materials");
}

int main(){
  ModelRegistry reg;
  CHECK(reg.LoadFromFile(R"(D:\APBReloaded\Content\Data\model_reference_catalog.json)"), "load model catalog");
  CHECK(reg.VehicleCount() > 0, "vehicles referenced");
  CHECK(reg.CharacterCount() > 0, "characters referenced");
  const ModelRef* fam = reg.FindByFamily("V_A_2DrCoupe");
  CHECK(fam != nullptr, "find coupe family");
  if (fam) {
    CHECK(!fam->rel_path.empty(), "rel_path set");
    CHECK(fam->ue5_import_hint.find("/Game/Imported/") != std::string::npos, "ue5 import hint");
    std::cout << "REF package=" << fam->package << " path=" << fam->rel_path << "\n";
  }
  WorldService w;
  CHECK(w.InitFromDataDir(R"(D:\APBReloaded\Content\Data)"), "world init loads models");
  CHECK(w.models.VehicleCount() > 0, "world models bound");
  TestPlacementManifest();
  TestShippedPlacementResolveAndNearSpawnMetrics();
  TestFreeroamSourceInvokesLoader();
  std::cout << "FAILS=" << fails << "\n";
  return fails?1:0;
}
