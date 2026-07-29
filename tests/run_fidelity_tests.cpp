// Drives shipped Domain: mission scripts + customization + session loop
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include <iostream>
#include <string>
#include <fstream>
using namespace apb;
static int fails = 0;
#define CHECK(c,m) do{ if(!(c)){ std::cerr<<"FAIL: "<<m<<"\n"; ++fails; } else { std::cout<<"PASS: "<<m<<"\n"; } }while(0)
static std::string DataDir(){ return R"(D:\APBReloaded\Content\Data)"; }

void TestMissionScripts() {
	MissionScriptLibrary lib;
	CHECK(lib.LoadFromJsonFile(DataDir()+"/mission_scripts.json"), "load mission_scripts.json");
	CHECK(lib.scripts.size() >= 1, "scripts non-empty");
	const MissionScriptDef* s = lib.Find("JG_BCS4_Bom1");
	CHECK(s != nullptr, "find JG_BCS4_Bom1");
	CHECK(s->stages.size() >= 3, "stage count >= 3");
	CHECK(!s->contact_id.empty(), "contact_id set");
	CHECK(s->stages[0].type == "contact", "first stage contact");
	// apbdb-sourced per-stage countdown (api.apbdb.com/beacon/missions/JG_BCS4_Bom1,
	// aStages[].nTimeLimit). Locks catalog against un-sourced/guessed timer drift.
	CHECK(s->stages[0].time_limit_sec == 300.0, "JG_BCS4_Bom1 stage0 apbdb timer=300");
	for (size_t i = 0; i < s->stages.size(); ++i) {
		CHECK(s->stages[i].time_limit_sec == 300.0, "JG_BCS4_Bom1 all stages apbdb timer=300");
	}
	MissionRun run = MissionRun::FromScript(*s, Faction::Criminal);
	run.Start();
	CHECK(run.status == MissionStatus::Active, "mission active");
	CHECK(run.opposition_contesting, "opposition contesting");
	CHECK(run.StageCount() >= 3, "runtime stages >= 3");
	int safety = 0;
	while (run.status == MissionStatus::Active && safety++ < 50) {
		// defend needs 5 progress
		run.Progress(1.0);
	}
	CHECK(run.status == MissionStatus::Completed, "script completes multi-stage");
	// fail path
	const MissionScriptDef* failS = lib.Find("APB_Script_FailDemo");
	CHECK(failS != nullptr, "find fail demo script");
	MissionRun failRun = MissionRun::FromScript(*failS, Faction::Enforcer);
	failRun.Start();
	failRun.RegisterOppositionTakeout();
	failRun.RegisterOppositionTakeout();
	CHECK(failRun.status == MissionStatus::Failed, "opposition takeouts force fail");
}

void TestCustomization() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "init for customization");
	CHECK(w.CreateCharacter("Alpha", Faction::Criminal), "create alpha");
	CharacterAppearance base = w.appearance;
	CHECK(w.EquipClothing("torso", "Clothing_Crim_Hoodie_T1", 12, 0, "rose"), "equip torso hoodie");
	CHECK(w.EquipClothing("legs", "Clothing_Crim_Jeans_T1", 3, 1, ""), "equip legs jeans");
	CHECK(w.appearance.FindSlot("torso") && w.appearance.FindSlot("torso")->item_id == "Clothing_Crim_Hoodie_T1", "torso id retained");
	CHECK(w.appearance.FindSlot("legs") && w.appearance.FindSlot("legs")->item_id == "Clothing_Crim_Jeans_T1", "legs id retained");
	w.appearance.body.hair_color = 9;
	w.appearance.body.face_preset = 4;
	std::string blob = w.SaveAppearanceBlob();
	CHECK(!blob.empty(), "serialize appearance");
	// second character differs
	WorldService w2;
	w2.InitFromDataDir(DataDir());
	w2.CreateCharacter("Bravo", Faction::Enforcer);
	CHECK(w2.EquipClothing("torso", "Clothing_Enf_Jacket_T1", 1, 2, "badge"), "equip enf jacket");
	CHECK(w2.EquipClothing("legs", "Clothing_Enf_Pants_T1", 1, 0, ""), "equip enf pants");
	CHECK(w.appearance.DiffersFrom(w2.appearance), "two characters differ");
	// reload into fresh appearance
	CharacterAppearance loaded;
	CHECK(CharacterAppearance::Deserialize(blob, loaded), "deserialize");
	CHECK(loaded.FindSlot("torso") && loaded.FindSlot("torso")->item_id == "Clothing_Crim_Hoodie_T1", "reload torso");
	CHECK(loaded.FindSlot("legs") && loaded.FindSlot("legs")->item_id == "Clothing_Crim_Jeans_T1", "reload legs");
	CHECK(loaded.body.hair_color == 9 && loaded.body.face_preset == 4, "reload body fields");
	// unknown clothing rejected
	CHECK(!w.EquipClothing("head", "NotARealGarment_XYZ", 0, 0, ""), "reject unknown clothing");
}

void TestSessionLoop() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "session init");
	CHECK(w.CreateCharacter("Runner", Faction::Criminal), "session create");
	CHECK(w.EquipClothing("torso", "Clothing_Crim_Hoodie_T1", 5, 0, "tag"), "session equip");
	CHECK(w.EquipClothing("feet", "Clothing_Boots_Urban_T1", 0, 0, ""), "session boots");
	auto districts = w.ListDistricts();
	CHECK(!districts.empty(), "districts");
	CHECK(w.JoinDistrict(districts[0].id, "Runner"), "join district");
	CHECK(w.phase == SessionPhase::District, "phase district");
	CHECK(w.appearance.FindSlot("torso") != nullptr, "customization present on session");
	CHECK(w.StartMissionScript("JG_BCS4_Bom1"), "start scripted mission");
	CHECK(w.mission.has_value() && w.mission->StageCount() >= 3, "scripted stages loaded");
	CHECK(w.mission->contact_id == "Contact_BCS_Bomber", "contact from script");
	// combat + threat
	std::string wid; for (auto& kv : w.catalog.items) if (kv.second.category=="Weapon") { wid = kv.first; break; }
	CHECK(!wid.empty(), "weapon in catalog");
	CombatantState me{"Runner", Faction::Criminal, 1000, 0, 0, true};
	CombatantState foe{"Cop", Faction::Enforcer, 100, 2, 0, true};
	double t0 = w.threat.points;
	auto shot = w.FireWeapon(wid, me, foe, 2, 0);
	CHECK(shot.hit && shot.damage > 0, "catalog weapon hit");
	// kill for threat
	foe.health = 1; foe.alive = true;
	w.FireWeapon(wid, me, foe, 2, 0);
	CHECK(w.threat.points > t0, "threat rose");
	// complete mission
	int safety = 0;
	while (w.mission->status == MissionStatus::Active && safety++ < 50) w.AdvanceMission(1.0);
	CHECK(w.mission->status == MissionStatus::Completed, "scripted mission completed");
}

void TestWardrobeTabsM5() {
	const auto& tabs = CustomizationService::WardrobeTabs();
	CHECK(tabs.size() == 15, "15 wardrobe tabs");
	CHECK(std::string(CustomizationService::SlotForTab(1)) == "torso", "tab 1 -> torso");
	CHECK(std::string(CustomizationService::SlotForTab(15)) == "bodyhair", "tab 15 -> bodyhair");
	CHECK(std::string(CustomizationService::SlotForTab(99)).empty(), "unknown tab -> empty");
	std::ifstream in(DataDir()+"/wardrobe_categories.json");
	CHECK(in.good(), "open wardrobe_categories.json");
	std::string line; int matched = 0;
	while (std::getline(in, line)) {
		for (const auto& t : tabs) {
			std::string idKey = "\"tab_id\": " + std::to_string(t.tab_id) + ",";
			std::string slotKey = "\"domain_slot\": \"" + std::string(t.domain_slot) + "\"";
			if (line.find(idKey) != std::string::npos) {
				CHECK(line.find(slotKey) != std::string::npos, std::string("json tab ")+std::to_string(t.tab_id)+" -> "+t.domain_slot);
				++matched;
			}
		}
	}
	CHECK(matched == 15, "all 15 tabs cross-checked vs json");
}

void TestSymbolLayerM5() {
	CharacterAppearance a;
	a.body.hair_color = 7;
	SymbolLayer s; s.symbol_id = 42; s.target_slot = "torso";
	s.pos_x = 1.5f; s.pos_y = -2.25f; s.rotation = 90.f; s.scale = 0.5f;
	s.color_primary = 12; s.color_secondary = 3;
	a.symbols.push_back(s);
	std::string blob = a.Serialize();
	CharacterAppearance loaded;
	CHECK(CharacterAppearance::Deserialize(blob, loaded), "deserialize symbol blob");
	CHECK(loaded.symbols.size() == 1, "one symbol restored");
	const auto& r = loaded.symbols[0];
	CHECK(r.symbol_id == 42 && r.target_slot == "torso", "symbol id+slot restored");
	CHECK(r.pos_x == 1.5f && r.pos_y == -2.25f && r.rotation == 90.f && r.scale == 0.5f, "symbol transform restored");
	CHECK(r.color_primary == 12 && r.color_secondary == 3, "symbol colors restored");
	// backward compat: legacy 1-pipe blob has no symbols
	CharacterAppearance legacy;
	CHECK(CharacterAppearance::Deserialize("H=1;B=0.5|torso:Item_X:0:0:", legacy), "deserialize legacy blob");
	CHECK(legacy.symbols.empty(), "legacy blob has no symbols");
	CHECK(legacy.clothing.size() == 1, "legacy clothing intact");
}

void TestFifteenSlotsM5() {
	CharacterAppearance a;
	for (const auto& t : CustomizationService::WardrobeTabs())
		a.Equip(t.domain_slot, std::string("Item_")+t.domain_slot, t.tab_id, 0);
	CHECK(a.clothing.size() == 15, "15 distinct slots equipped independently");
	std::string blob = a.Serialize();
	CharacterAppearance loaded;
	CHECK(CharacterAppearance::Deserialize(blob, loaded), "deserialize 15-slot blob");
	CHECK(loaded.clothing.size() == 15, "15 slots survive restart");
	for (const auto& t : CustomizationService::WardrobeTabs())
		CHECK(loaded.FindSlot(t.domain_slot) != nullptr, std::string("slot ")+t.domain_slot+" restored");
}

void TestRandomizeM5() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "randomize init catalog");
	CustomizationService svc; svc.catalog = &w.catalog;
	CharacterAppearance r1 = svc.Randomize(Faction::Criminal, 12345);
	CharacterAppearance r2 = svc.Randomize(Faction::Criminal, 12345);
	CharacterAppearance r3 = svc.Randomize(Faction::Criminal, 99999);
	CHECK(!r1.DiffersFrom(r2), "same seed -> deterministic");
	CHECK(r1.DiffersFrom(r3), "different seed -> different result");
	CHECK(r1.body.height >= 0.8f && r1.body.height <= 1.2f, "randomized height in range");
	CHECK(r1.clothing.size() >= 10, "randomize fills most tab pools");
	for (const auto& c : r1.clothing) {
		const ItemDef* it = w.catalog.FindItem(c.item_id);
		CHECK(it != nullptr && it->wardrobe_tab >= 1, std::string("randomized ")+c.slot+" is a real tabbed item");
	}
}

int main() {
	std::cout << "APB fidelity tests (mission scripts + customization + session)\n";
	TestMissionScripts();
	TestCustomization();
	TestSessionLoop();
	TestWardrobeTabsM5();
	TestSymbolLayerM5();
	TestFifteenSlotsM5();
	TestRandomizeM5();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
