// Drives shipped Domain: mission scripts + customization + session loop
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include <iostream>
#include <string>
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

int main() {
	std::cout << "APB fidelity tests (mission scripts + customization + session)\n";
	TestMissionScripts();
	TestCustomization();
	TestSessionLoop();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
