// run_progression_tests.cpp — M15 (brief #14): economy & progression tests.
// Links APBProgression.cpp + APBCatalog.cpp (for the shared JSON helpers).
// Pattern mirrors the other run_*_tests.cpp.
#include "APBProgression.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

static std::string DataDir() { return R"(D:\APBReloaded\Content\Data)"; }

static const char* kContactsJson = R"([
  { "id": "Financial_C1", "title": "Double-B", "faction": null, "source": "x" },
  { "id": "Financial_C2", "title": "Veronika Lee", "faction": null, "source": "x" },
  { "id": "Waterfront_C3", "title": "Britney Bloodrose", "faction": null, "source": "x" }
])";

static const char* kRolesJson = R"([
  { "id": "Role_Assault_Rifle", "name": "Rifleman", "description": "d", "source": "x" },
  { "id": "Role_Driver", "name": "Getaway Driver", "description": "d", "source": "x" }
])";

static void TestCatalogParse() {
	ProgressionCatalog cat;
	CHECK(cat.LoadContactsFromText(kContactsJson), "contacts parsed");
	CHECK(cat.LoadRolesFromText(kRolesJson), "roles parsed");
	CHECK(cat.ContactCount() == 3, "three contacts");
	CHECK(cat.RoleCount() == 2, "two roles");

	const ContactDef* dbb = cat.FindContact("Financial_C1");
	CHECK(dbb && dbb->title == "Double-B", "contact title parsed");
	CHECK(dbb && dbb->district == "Financial", "district derived from id prefix");

	const RoleDef* rifle = cat.FindRole("Role_Assault_Rifle");
	CHECK(rifle && rifle->name == "Rifleman", "role name parsed");

	auto fin = cat.ContactsInDistrict("Financial");
	CHECK(fin.size() == 2, "two Financial contacts");
	auto wf = cat.ContactsInDistrict("Waterfront");
	CHECK(wf.size() == 1, "one Waterfront contact");
}

static void TestDistrictFromContactId() {
	CHECK(DistrictFromContactId("Financial_C1") == "Financial", "Financial_C1 -> Financial");
	CHECK(DistrictFromContactId("Waterfront_C3") == "Waterfront", "Waterfront_C3 -> Waterfront");
	CHECK(DistrictFromContactId("NoUnderscore").empty(), "no underscore -> empty");
}

static void TestLevelLadder() {
	LevelLadder l = LevelLadder::DefaultContactLadder();
	CHECK(l.MaxLevel() == 15, "ladder has levels 0..15");
	CHECK(l.thresholds[0] == 0, "level 0 at standing 0");
	CHECK(l.StandingForLevel(1) == 1000, "level 1 threshold 1000");
	CHECK(l.StandingForLevel(2) == 3000, "level 2 threshold 3000 (1000+2000)");
	CHECK(l.LevelFor(0) == 0, "standing 0 = level 0");
	CHECK(l.LevelFor(999) == 0, "standing 999 = level 0");
	CHECK(l.LevelFor(1000) == 1, "standing 1000 = level 1");
	CHECK(l.LevelFor(2999) == 1, "standing 2999 = level 1");
	CHECK(l.LevelFor(3000) == 2, "standing 3000 = level 2");
	// Clamp above max.
	CHECK(l.LevelFor(1000000000LL) == 15, "huge standing clamps at max level");
	CHECK(l.StandingForLevel(99) == l.StandingForLevel(15), "over-max level clamps");
}

static void TestCharacterProgress() {
	CharacterProgress p;
	LevelLadder l = LevelLadder::DefaultContactLadder();
	CHECK(p.ContactStanding("Financial_C1") == 0, "unknown contact standing 0");
	CHECK(p.ContactLevel("Financial_C1", l) == 0, "unknown contact level 0");

	CHECK(p.AddContactStanding("Financial_C1", 1200) == 1200, "add standing returns total");
	CHECK(p.ContactLevel("Financial_C1", l) == 1, "1200 standing = level 1");
	p.AddContactStanding("Financial_C1", 1800); // total 3000 -> level 2
	CHECK(p.ContactLevel("Financial_C1", l) == 2, "3000 standing = level 2");

	// Negative amounts ignored.
	CHECK(p.AddContactStanding("Financial_C1", -500) == 3000, "negative standing ignored");

	CHECK(p.AddRoleXp("Role_Driver", 250) == 250, "role xp accrues");
	CHECK(p.AddRoleXp("Role_Driver", 250) == 500, "role xp accumulates");
	CHECK(p.RoleXp("Role_Driver") == 500, "role xp readback");
}

static void TestMissionReward() {
	// Threat tier 2 reward_multiplier 1.0 -> unchanged.
	MissionReward r1 = ComputeMissionReward(1000, 500, 100, 1.0);
	CHECK(r1.cash == 1000 && r1.standing == 500, "mult 1.0 leaves cash/standing");
	CHECK(r1.role_xp == 100, "role xp flat at mult 1.0");

	// High threat 1.55 scales cash + standing, NOT role xp.
	MissionReward r2 = ComputeMissionReward(1000, 500, 100, 1.55);
	CHECK(r2.cash == 1550, "cash scaled by 1.55");
	CHECK(r2.standing == 775, "standing scaled by 1.55");
	CHECK(r2.role_xp == 100, "role xp NOT threat-scaled");

	// Low threat 0.5 halves; rounding to nearest.
	MissionReward r3 = ComputeMissionReward(1000, 501, 0, 0.5);
	CHECK(r3.cash == 500, "cash halved");
	CHECK(r3.standing == 251, "standing 501*0.5=250.5 rounds to 251");

	// Negative multiplier clamps to 0.
	MissionReward r4 = ComputeMissionReward(1000, 500, 100, -1.0);
	CHECK(r4.cash == 0 && r4.standing == 0, "negative multiplier -> 0 cash/standing");
	CHECK(r4.role_xp == 100, "role xp still awarded");
}

static void TestUnlocks() {
	CharacterProgress p;
	LevelLadder l = LevelLadder::DefaultContactLadder();
	std::vector<ContactUnlock> unlocks = {
		{ "Financial_C1", 1, "OSCAR_Rifle" },
		{ "Financial_C1", 2, "NTEC_Rifle" },
		{ "Financial_C1", 5, "Legendary_Rifle" },
	};
	// Level 0: nothing.
	CHECK(UnlockedItems(p, l, unlocks).empty(), "no unlocks at level 0");

	// Standing 1000 -> level 1: OSCAR only.
	p.AddContactStanding("Financial_C1", 1000);
	auto u1 = UnlockedItems(p, l, unlocks);
	CHECK(u1.size() == 1 && u1[0] == "OSCAR_Rifle", "level 1 unlocks OSCAR only");
	CHECK(IsUnlocked(p, l, unlocks[0]), "OSCAR unlocked");
	CHECK(!IsUnlocked(p, l, unlocks[1]), "NTEC not yet unlocked");

	// Standing 3000 -> level 2: OSCAR + NTEC.
	p.AddContactStanding("Financial_C1", 2000);
	auto u2 = UnlockedItems(p, l, unlocks);
	CHECK(u2.size() == 2, "level 2 unlocks OSCAR + NTEC");
}

static void TestCashSink() {
	int64_t balance = 1000;
	CHECK(TrySpend(balance, 400) && balance == 600, "spend debits balance");
	CHECK(!TrySpend(balance, 700) && balance == 600, "insufficient funds rejected, balance intact");
	CHECK(!TrySpend(balance, -50) && balance == 600, "negative cost rejected");
	CHECK(TrySpend(balance, 600) && balance == 0, "spend to exactly zero allowed");
}

static void TestRealDataFiles() {
	ProgressionCatalog cat;
	const std::string contacts = DataDir() + "\\contacts_lore.json";
	const std::string roles = DataDir() + "\\roles.json";
	if (!cat.LoadContactsFromFile(contacts) || !cat.LoadRolesFromFile(roles)) {
		std::printf("SKIP: real contacts_lore.json / roles.json not readable\n");
		return;
	}
	CHECK(cat.ContactCount() > 0, "real contacts_lore.json parsed");
	CHECK(cat.RoleCount() > 0, "real roles.json parsed");
	const ContactDef* known = cat.FindContact("Financial_C1");
	CHECK(known != nullptr, "known real contact present");
	CHECK(known && known->district == "Financial", "known real contact district derived");
}

int main() {
	std::printf("=== APB Progression Tests (M15 economy/progression) ===\n");
	TestCatalogParse();
	TestDistrictFromContactId();
	TestLevelLadder();
	TestCharacterProgress();
	TestMissionReward();
	TestUnlocks();
	TestCashSink();
	TestRealDataFiles();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
