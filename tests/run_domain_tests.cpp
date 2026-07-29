// Drives shipped Domain sources (same code linked into UE module).
#include "../Source/APBReloaded/Domain/APBWorldService.h"
#include <iostream>
#include <cassert>
#include <fstream>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static std::string DataDir() {
	return R"(D:\APBReloaded\Content\Data)";
}

void TestLoginSuccessAndFail() {
	// Exercises shipped LoginService via WorldService register/login (real Domain API).
	WorldService auth;
	CHECK(auth.InitFromDataDir(DataDir()), "auth init catalog");
	CHECK(!auth.LoginAccount("no_such_user", "x"), "login_fail unknown user");
	CHECK(auth.RegisterAccount("carol", "secret1"), "register carol");
	CHECK(!auth.RegisterAccount("carol", "other"), "register_fail duplicate");
	CHECK(!auth.LoginAccount("carol", "wrong_password"), "login_fail bad password");
	CHECK(auth.LoginAccount("carol", "secret1"), "login_ok correct password");
	CHECK(auth.login.IsLoggedIn(), "session active after login_ok");
	auth.login.Logout();
	CHECK(!auth.login.IsLoggedIn(), "session cleared after logout");
	// banned account cannot login
	auth.login.accounts["carol"].banned = true;
	CHECK(!auth.LoginAccount("carol", "secret1"), "login_fail banned");
	std::cout << "LOGIN_PATH login_ok + login_fail exercised on LoginService\n";
}

void TestLoginWorldDistrict() {
	WorldService host, client;
	CHECK(host.InitFromDataDir(DataDir()), "host init catalog");
	CHECK(client.InitFromDataDir(DataDir()), "client init catalog");
	CHECK(host.RegisterAccount("alice", "pw"), "register alice");
	CHECK(host.LoginAccount("alice", "pw"), "login alice");
	CHECK(host.EnterWorld("W1"), "enter world W1");
	CHECK(host.CreateCharacter("Alice", Faction::Criminal), "host create criminal");
	CHECK(client.RegisterAccount("bob", "pw"), "register bob");
	CHECK(client.LoginAccount("bob", "pw"), "login bob");
	CHECK(client.EnterWorld("W1"), "client enter world");
	CHECK(client.CreateCharacter("Bob", Faction::Enforcer), "client create enforcer");
	auto districts = host.ListDistricts();
	CHECK(districts.size() >= 6, "live district set size >= 6");
	// all expected names present as data allows
	bool hasFin=false, hasWf=false, hasSoc=false;
	for (auto& d : districts) {
		if (d.id == "Financial") hasFin = true;
		if (d.id == "Waterfront") hasWf = true;
		if (d.id == "Social") hasSoc = true;
	}
	CHECK(hasFin && hasWf && hasSoc, "Financial/Waterfront/Social present");
	std::string did = "Financial";
	auto res = host.ReserveDistrict(did, "Alice");
	CHECK(res.state == DistrictQueueState::Reserved, "reserve district");
	CHECK(host.JoinDistrict(did, "Alice"), "host join district");
	CHECK(host.phase == SessionPhase::District, "host phase district");
	CHECK(host.district.has_value(), "host session exists");
	std::string sid = host.district->session_id;
	CHECK(host.JoinDistrictAsPeer(sid, "Bob"), "peer joins same session");
	CHECK(host.district->players.size() >= 2, "two peers in district session");
	// stream chunks not monolithic
	CHECK(host.stream_plan.chunks.size() >= 4, "stream plan has multiple chunks");
	auto near = host.StreamChunksNear(0, 0);
	CHECK(!near.empty(), "chunks near origin loaded");
	std::cout << "SESSION " << sid << " map=" << host.district->map_name
		<< " players=" << host.district->players.size()
		<< " chunks_near=" << near.size() << "\n";
}

void TestEconomy() {
	WorldService server;
	CHECK(server.InitFromDataDir(DataDir()), "economy init");
	CHECK(server.CreateCharacter("Buyer", Faction::Enforcer), "buyer char");
	// scalable catalog not one-offs
	auto armas = server.catalog.ArmasCatalog();
	CHECK(armas.size() >= 20, "armas catalog scalable (>=20)");
	std::string item;
	for (auto& kv : server.catalog.items) { if (kv.second.armas_listed) { item = kv.first; break; } }
	CHECK(!item.empty(), "catalog has armas item");
	int64_t g1c_before = server.character->g1c;
	auto buy = server.ArmasBuy(item);
	CHECK(buy.ok, "armas purchase ok");
	CHECK(server.character->g1c == g1c_before - buy.g1c_spent, "g1c deducted");
	CHECK(server.inventory.Has(item, 1), "item in inventory");
	server.character->g1c = 0;
	auto fail = server.ArmasBuy(item);
	CHECK(!fail.ok && fail.error == "insufficient_g1c", "armas reject insufficient");

	CharacterProfile sellerProf; sellerProf.name = "Seller"; sellerProf.faction = Faction::Criminal; sellerProf.cash = 0;
	Inventory sellerInv; sellerInv.Grant(item, 2);
	auto list = server.auction.ListItem("Seller", sellerInv, sellerProf, item, 1, 1000);
	CHECK(list.ok, "auction list ok");
	CHECK(sellerInv.Count(item)==1, "item removed on list");
	int32_t buyerCountBefore = server.inventory.Count(item);
	server.character->cash = 5000;
	server.character->g1c = 100;
	auto bought = server.auction.Buyout("Buyer", *server.character, server.inventory, sellerProf, sellerInv, list.listing_id);
	CHECK(bought.ok, "auction buyout ok");
	CHECK(server.inventory.Count(item) == buyerCountBefore + 1, "buyer received auction item");
	CHECK(sellerProf.cash == 950, "seller received cash minus 5% marketplace fee (1000-50)");

	auto list2 = server.auction.ListItem("Seller", sellerInv, sellerProf, item, 1, 999999);
	CHECK(list2.ok, "second list ok");
	server.character->cash = 10;
	auto fail2 = server.auction.Buyout("Buyer", *server.character, server.inventory, sellerProf, sellerInv, list2.listing_id);
	CHECK(!fail2.ok && fail2.error == "insufficient_funds", "auction reject funds");

	auto badlist = server.auction.ListItem("Seller", sellerInv, sellerProf, "not_a_real_item_xyz", 1, 10);
	CHECK(!badlist.ok, "auction reject unknown item");
}

void TestDistrictLoop() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "loop init");
	CHECK(w.RegisterAccount("runner", "x"), "reg");
	CHECK(w.LoginAccount("runner", "x"), "login");
	CHECK(w.EnterWorld("W1"), "world");
	CHECK(w.CreateCharacter("Runner", Faction::Criminal), "runner");
	auto d = w.ListDistricts();
	CHECK(w.JoinDistrict(d[0].id, "Runner"), "join");
	std::string wid;
	double dmg = 0;
	for (auto& kv : w.catalog.items) if (kv.second.category=="Weapon") { wid=kv.first; dmg=kv.second.damage; break; }
	CHECK(!wid.empty() && dmg > 0, "weapon stats from catalog");
	// vehicle fleet
	std::string vid;
	int vehCount = 0;
	for (auto& kv : w.catalog.items) if (kv.second.category=="Vehicle") { ++vehCount; if (vid.empty()) vid=kv.first; }
	CHECK(vehCount >= 20, "vehicle fleet families >= 20");
	CHECK(w.SpawnVehicle(vid), "spawn vehicle");
	CHECK(w.PossessVehicle("Runner"), "possess vehicle");
	CHECK(w.vehicle.has_value() && w.vehicle->possessed, "vehicle possessed");
	CHECK(w.ExitVehicle(), "exit vehicle");

	CombatantState me{"Runner", Faction::Criminal, 1000, 0, 0, true};
	CombatantState foe{"Cop", Faction::Enforcer, 500, 3, 0, true};
	const ItemDef* wdef = w.catalog.FindItem(wid);
	CHECK(wdef != nullptr, "weapon def");
	double expected_scale = wdef->damage;
	auto shot = w.FireWeapon(wid, me, foe, 3, 0);
	CHECK(shot.hit, "shot hit");
	CHECK(shot.damage > 0 && shot.damage <= expected_scale + 0.01, "damage from catalog stats");
	double threat0 = w.threat.points;
	while (foe.alive) {
		foe.health = 1;
		shot = w.FireWeapon(wid, me, foe, 3, 0);
		if (!shot.hit) break;
	}
	CHECK(w.threat.points > threat0, "threat rose after kill");
	CHECK(w.NotorietyOrPrestige() == w.threat.points, "notoriety tracks threat");
	int bots_low = w.threat.CurrentTier().bot_count;
	// apbdb notoriety: L0 thr=0, L1=150, L2=750, L3=1500, L4=4000, L5=7000
	// Use points past L3 so opposition_multiplier > 1.0 and bots increase.
	w.threat.points = 2000;
	int bots_high = w.threat.CurrentTier().bot_count;
	CHECK(bots_high > bots_low, "higher threat more opposition bots");
	CHECK(w.OppositionPressure() > 1.0, "opposition pressure scales");
	CHECK(w.threat.loaded_from_table || w.threat.LevelIndex() >= 3, "threat tiers available");
	w.StartMission();
	CHECK(w.mission.has_value() && w.mission->status == MissionStatus::Active, "mission active");
	CHECK(w.mission->opposition_contesting, "opposition contesting");
	const int64_t cashBeforeMission = w.character->cash;
	const std::string missionContact = w.mission->contact_id;
	int safety=0;
	while (w.mission->status == MissionStatus::Active && safety++ < 80) {
		w.AdvanceMission(1.0);
	}
	CHECK(w.mission->status == MissionStatus::Completed, "mission completed multi-stage");
	CHECK(w.character->cash > cashBeforeMission, "mission complete awards cash");
	if (!missionContact.empty())
		CHECK(w.progress.ContactStanding(missionContact) > 0, "mission complete awards contact standing");
	// social stubs non-blocking (friends/clan); mail via real MailService (M2)
	CHECK(w.social.AddFriend("Bob"), "friend stub");
	CHECK(w.SendMail("Bob", "hi", "body"), "mail send");
	CHECK(w.social.CreateClan("C1", "Crew", "CRW", "Runner"), "clan stub");
	// config blob
	CHECK(w.SaveCharacterConfig(), "save config blob");
	CHECK(w.LoadCharacterConfig(), "load config blob");
}

void TestDomainSnapshotParity() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "snap init");
	CHECK(w.CreateCharacter("Snap", Faction::Criminal), "snap char");
	DomainSnapshot s0 = w.CaptureSnapshot();
	CHECK(s0.has_character, "snap has character");
	CHECK(s0.cash == w.character->cash, "snap cash == character cash");
	CHECK(s0.g1c == w.character->g1c, "snap g1c == character g1c");
	CHECK(s0.inventory_slot_count == (int32_t)w.inventory.slots.size(), "snap inv slots == inventory.slots.size");
	int32_t qty = 0; for (auto& sl : w.inventory.slots) qty += sl.quantity;
	CHECK(s0.inventory_total_qty == qty, "snap total qty");
	// after armas
	std::string item;
	for (auto& kv : w.catalog.items) if (kv.second.armas_listed) { item = kv.first; break; }
	auto buy = w.ArmasBuy(item);
	CHECK(buy.ok, "snap armas");
	DomainSnapshot s1 = w.CaptureSnapshot();
	CHECK(s1.g1c == w.character->g1c, "snap g1c after buy");
	CHECK(s1.inventory_slot_count == (int32_t)w.inventory.slots.size(), "snap inv after buy");
	CHECK(s1.g1c < s0.g1c, "g1c decreased after buy");
	w.JoinDistrict("Financial", "Snap");
	w.StartMissionScript("JG_BCS4_Bom1");
	DomainSnapshot s2 = w.CaptureSnapshot();
	CHECK(s2.mission_stage_count >= 3, "snap mission stages");
	CHECK(s2.mission_title.size() > 0, "snap mission title");
	CHECK(s2.session_id.find("DS-") == 0, "snap session id");
	// advance and re-snap
	w.AdvanceMission(1.0);
	DomainSnapshot s3 = w.CaptureSnapshot();
	CHECK(s3.mission_stage_index == w.mission->current_index, "snap stage index matches mission");
	CHECK(s3.threat_points == w.threat.points, "snap threat matches");
	// M15: snapshot must expose per-character progression (contact standing + role XP), id-sorted.
	w.progress.AddContactStanding("Financial_C1", 1500);
	w.progress.AddContactStanding("Armas_C2", 300);
	w.progress.AddRoleXp("Role2_CrimArson", 750);
	DomainSnapshot sp = w.CaptureSnapshot();
	CHECK(sp.contact_standings.size() == 2, "snap exposes 2 contact standings");
	CHECK(sp.role_xp.size() == 1, "snap exposes 1 role xp");
	CHECK(sp.contact_standings[0].id == "Armas_C2", "snap contacts id-sorted");
	CHECK(sp.contact_standings[1].id == "Financial_C1", "snap contacts id-sorted 2");
	CHECK(sp.contact_standings[1].value == 1500, "snap contact standing value");
	CHECK(sp.role_xp[0].id == "Role2_CrimArson" && sp.role_xp[0].value == 750, "snap role xp value");
	CHECK(sp.active_contact_id == w.mission->contact_id, "snap active contact id == mission contact");
}

// Opposed-mission score race: both sides contest the same stage objective; whoever reaches the
// target first decides the mission. Mirrors APB's symmetric opposed missions (either team can win).
void TestOppositionRace() {
	// (1) Opposition secures the contested stage ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ mission fails for the owner.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "opp init");
	CHECK(w.CreateCharacter("OppLoser", Faction::Criminal), "opp char");
	w.JoinDistrict("Financial", "OppLoser");
	w.StartMission();
	CHECK(w.mission.has_value() && w.mission->status == MissionStatus::Active, "opp mission active");
	CHECK(w.mission->opposition_contesting, "opp mission contesting");
	DomainSnapshot sc = w.CaptureSnapshot();
	CHECK(sc.mission_opposition_contesting, "snap exposes contesting");
	CHECK(sc.mission_opp_stage_progress == 0, "snap opp progress starts at 0");
	CHECK(!sc.mission_opposition_won, "snap opposition_won starts false");
	int guard = 0; bool decided = false;
	while (w.mission->status == MissionStatus::Active && guard++ < 100) {
		decided = w.AdvanceOpposition(1.0);
		if (decided) break;
	}
	CHECK(decided, "AdvanceOpposition reports a decision");
	CHECK(w.mission->status == MissionStatus::Failed, "opposition win fails mission");
	CHECK(w.mission->opposition_won, "opposition_won set on mission");
	DomainSnapshot sf = w.CaptureSnapshot();
	CHECK(sf.mission_opposition_won, "snap exposes opposition_won");
	CHECK(sf.mission_status == "Failed", "snap mission failed after opposition win");

	// (2) Owner can still win the race by reaching the objective first.
	WorldService w2;
	CHECK(w2.InitFromDataDir(DataDir()), "opp2 init");
	CHECK(w2.CreateCharacter("OppWinner", Faction::Enforcer), "opp2 char");
	w2.JoinDistrict("Financial", "OppWinner");
	w2.StartMission();
	int guard2 = 0;
	while (w2.mission->status == MissionStatus::Active && guard2++ < 100) {
		w2.AdvanceMission(1.0);
	}
	CHECK(w2.mission->status == MissionStatus::Completed, "owner wins race when faster");
	CHECK(!w2.mission->opposition_won, "no opposition win when owner completes");
	// A terminal (completed) mission can no longer be flipped by the opposition.
	CHECK(!w2.AdvanceOpposition(1.0), "opposition cannot flip a completed mission");
}

// APB mission stages carry countdown timers (time_limit_sec, parsed from mission JSON). Running
// past a stage's deadline fails the mission. Timing is deterministic: the caller supplies now_sec.
void TestMissionStageTimeout() {
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "timeout init");
	CHECK(w.CreateCharacter("Ticker", Faction::Criminal), "timeout char");
	w.JoinDistrict("Financial", "Ticker");
	w.StartMission();
	CHECK(w.mission.has_value() && w.mission->status == MissionStatus::Active, "timeout mission active");
	// Give the current stage a 30s countdown (APB stage timer).
	w.mission->stages[w.mission->current_index].def.time_limit_sec = 30.0;
	// First tick arms the timer (no timeout yet); snapshot exposes the limit.
	CHECK(!w.TickMission(100.0), "arm tick does not time out");
	DomainSnapshot sa = w.CaptureSnapshot();
	CHECK(sa.mission_stage_time_limit_sec == 30.0, "snap exposes stage time limit");
	// The absolute armed deadline (now_sec + limit) must be exposed for the client HUD
	// countdown; it is stable per stage (armed once), NOT recomputed each capture.
	CHECK(sa.mission_stage_deadline_server_sec == 130.0, "snap exposes armed stage deadline");
	CHECK(!sa.mission_timed_out, "not timed out yet");
	// Still within the window (deadline = 100 + 30 = 130).
	CHECK(!w.TickMission(120.0), "within window no timeout");
	CHECK(w.mission->status == MissionStatus::Active, "mission still active within window");
	// Past the deadline -> mission times out.
	CHECK(w.TickMission(131.0), "past deadline times out");
	CHECK(w.mission->status == MissionStatus::Failed, "timeout fails mission");
	CHECK(w.mission->timed_out, "timed_out flag set");
	DomainSnapshot sf = w.CaptureSnapshot();
	CHECK(sf.mission_timed_out, "snap exposes timed_out");
	CHECK(sf.mission_status == "Failed", "snap failed after timeout");

	// A stage with no time limit never times out no matter how far the clock advances.
	WorldService w2;
	CHECK(w2.InitFromDataDir(DataDir()), "timeout2 init");
	CHECK(w2.CreateCharacter("NoTimer", Faction::Enforcer), "timeout2 char");
	w2.JoinDistrict("Financial", "NoTimer");
	w2.StartMission();
	w2.mission->stages[w2.mission->current_index].def.time_limit_sec = 0.0;
	CHECK(!w2.TickMission(1.0e9), "no-limit stage never times out");
	CHECK(w2.mission->status == MissionStatus::Active, "no-limit mission stays active");
}

void TestContactLevelsFromRetail() {
	// M15: real retail per-contact level counts parsed from Content/Data/contact_levels.json
	// (extracted from ContactLevels.INT). Proves the Domain honors the real roster instead of
	// one invented 0..15 ladder for every contact.
	ProgressionCatalog cat;
	CHECK(cat.LoadContactLevelsFromFile(DataDir() + std::string("\\contact_levels.json")),
		"contact_levels.json parses");
	CHECK(cat.ContactLevelCount() >= 70, "loaded full retail contact roster");
	// Known real max levels straight from ContactLevels.INT.
	CHECK(cat.ContactMaxLevel("CriminalDefault") == 10, "CriminalDefault max_level 10");
	CHECK(cat.ContactMaxLevel("Binky") == 3, "Binky max_level 3");
	CHECK(cat.ContactMaxLevel("Clyde") == 1, "Clyde max_level 1");
	CHECK(cat.ContactMaxLevel("Financial_C07") == 15, "Financial_C07 max_level 15");
	CHECK(cat.ContactMaxLevel("Financial_C11") == 20, "Financial_C11 max_level 20");
	CHECK(cat.ContactMaxLevel("Nonexistent_Contact") == 0, "unknown contact has no level");

	// Zero-padding normalization: the lore id "Financial_C1" must resolve to the padded
	// "Financial_C01" entry (max_level 5).
	CHECK(NormalizeContactId("Financial_C01") == "Financial_C1", "normalize strips leading zero");
	CHECK(NormalizeContactId("Waterfront_E12") == "Waterfront_E12", "normalize keeps multi-digit");
	CHECK(NormalizeContactId("Binky") == "Binky", "normalize leaves non-numeric id");
	CHECK(cat.ContactMaxLevel("Financial_C1") == 5, "lore id resolves to padded entry");

	// The ladder is sized to the contact's real level count, not a fixed 15.
	CHECK(cat.LadderForContact("Financial_C01").MaxLevel() == 5, "ladder sized to real max 5");
	CHECK(cat.LadderForContact("Financial_C11").MaxLevel() == 20, "ladder sized to real max 20");
	CHECK(cat.LadderForContact("Unknown").MaxLevel() == 15, "unknown falls back to default 15");

	// End-to-end: WorldService loads the file during InitFromDataDir.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "contactlevels world init");
	CHECK(w.progression.ContactLevelCount() >= 70, "world loaded retail contact levels");
}

void TestPlayerRolesFromRetail() {
	// M15: the full retail role roster (canonical display names + descriptions) parsed from
	// Content/Data/player_roles.json (extracted from PlayerRoles.INT). Proves the Domain carries
	// the complete shipped roster + names, not just the ~20 partial apbdb-seeded roles.
	ProgressionCatalog cat;
	CHECK(cat.LoadRolesFromFile(DataDir() + std::string("\\player_roles.json")),
		"player_roles.json parses");
	CHECK(cat.RoleCount() >= 240, "loaded full retail role roster");

	// Canonical display names straight from PlayerRoles.INT (activity/weapon Role2_* tracks).
	const RoleDef* arson = cat.FindRole("Role2_CrimArson");
	CHECK(arson != nullptr, "Role2_CrimArson present");
	CHECK(arson && arson->name == "Arsonist", "Role2_CrimArson -> Arsonist");
	CHECK(arson && !arson->description.empty(), "Role2_CrimArson has description");
	const RoleDef* hack = cat.FindRole("Role2_Crim_Hacking");
	CHECK(hack != nullptr, "Role2_Crim_Hacking present");
	CHECK(hack && hack->name == "Black-Hat", "Role2_Crim_Hacking -> Black-Hat");

	// Descriptions are collapsed to a single physical line (no raw control/return glyphs).
	CHECK(arson && arson->description.find('\n') == std::string::npos, "description single-line");

	// Parser-fidelity invariant: the extractor decodes ConvertTo-Json's \uXXXX escapes so the
	// Domain's naive JsonGetString (which does NOT decode \uXXXX) never sees a mangled "u0027".
	// Scan the whole roster to prove no name/description carries such leftovers, and confirm at
	// least one apostrophe survived as a literal char (round-trip, not stripped).
	bool anyMangled = false, anyApostrophe = false;
	for (const auto& kv : cat.roles) {
		if (kv.second.name.find("u0027") != std::string::npos ||
			kv.second.description.find("u0027") != std::string::npos) anyMangled = true;
		if (kv.second.name.find('\'') != std::string::npos ||
			kv.second.description.find('\'') != std::string::npos) anyApostrophe = true;
	}
	CHECK(!anyMangled, "no role text carries a mangled u0027 escape");
	CHECK(anyApostrophe, "at least one role apostrophe round-trips as a literal char");

	// End-to-end: WorldService loads roles.json + player_roles.json in InitFromDataDir; the
	// retail roster merges on top of the partial apbdb seed (merge-by-id).
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "playerroles world init");
	CHECK(w.progression.RoleCount() >= 240, "world merged full retail roster");
	const RoleDef* wArson = w.progression.FindRole("Role2_CrimArson");
	CHECK(wArson && wArson->name == "Arsonist", "world Role2_CrimArson -> Arsonist");
}

void TestMissionTemplatesFromRetail() {
	// M15/D14: canonical retail mission titles parsed from Content/Data/mission_templates.json
	// (extracted from MissionTemplates.INT). Proves the Domain carries the shipped mission-title
	// roster keyed by template id, including titles with apostrophes (parser-clean, not \uXXXX).
	MissionTitleCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\mission_templates.json")),
		"mission_templates.json parses");
	CHECK(cat.Count() >= 200, "loaded full retail mission-title roster");

	// Known canonical titles straight from MissionTemplates.INT.
	CHECK(cat.TitleFor("AE_BCS0_Ter1_B") == "GANGLAND ANNEXATION", "AE_BCS0_Ter1_B title");
	CHECK(cat.TitleFor("DB_BCS4_Del1") == "PIMP MY CRIB", "DB_BCS4_Del1 title");
	// Apostrophe survives the naive JSON string scan (extractor decodes \u0027 -> ').
	CHECK(cat.TitleFor("DB_BCS0_Ars1_G") == "YOU'RE FIRED!", "apostrophe title decoded");
	CHECK(cat.TitleFor("Nonexistent_Template", "n/a") == "n/a", "unknown id returns default");

	// End-to-end: WorldService loads mission_templates.json in InitFromDataDir.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "missiontitles world init");
	CHECK(w.mission_titles.Count() >= 200, "world loaded retail mission titles");

	// D14 completion: mission-script ids share the MissionTemplate id space 1:1, so the canonical
	// retail titles are stamped onto the loaded MissionScriptDefs during InitFromDataDir. Every
	// retail-scheme script id must resolve to a catalog title; the only exemptions are synthetic
	// demo/test scripts (ids prefixed "APB_Script_") that have no retail MissionTemplate entry.
	int scriptsWithTitle = 0, unresolvedNonSynthetic = 0, resolvedScripts = 0;
	for (const auto& kv : w.mission_scripts.scripts) {
		if (!kv.second.title.empty()) ++scriptsWithTitle;
		if (w.mission_titles.Find(kv.first) != nullptr) { ++resolvedScripts; }
		else if (kv.first.rfind("APB_Script_", 0) != 0) ++unresolvedNonSynthetic;
	}
	CHECK(w.mission_scripts.scripts.size() > 0, "world loaded mission scripts");
	CHECK(resolvedScripts >= 40, "retail-scheme scripts resolve to canonical titles");
	CHECK(unresolvedNonSynthetic == 0, "no retail-scheme script id is left without a title");
	CHECK(scriptsWithTitle == (int)w.mission_scripts.scripts.size(),
		"every loaded script carries a display title after ApplyTo");
	// Spot-check specific script -> canonical title stamping.
	const MissionScriptDef* arsScript = w.mission_scripts.Find("DB_BCS3_Ars1");
	CHECK(arsScript != nullptr, "DB_BCS3_Ars1 script loaded");
	CHECK(arsScript && arsScript->title == "BOX-LOCK AND .52 BARREL",
		"DB_BCS3_Ars1 stamped with canonical title");

	// ApplyTo unit behavior: idempotent (a second pass changes nothing).
	CHECK(w.mission_titles.ApplyTo(w.mission_scripts) == 0, "ApplyTo is idempotent");
}

void TestTaskObjectivesFromRetail() {
	// M15/D14: per-stage owner/dispatch mission briefs parsed from Content/Data/task_objectives.json
	// (extracted from the retail TaskObjectives.INT). Proves the Domain carries the shipped stage
	// briefings, keyed to the MissionTemplate id space, with inline retail markup preserved.
	MissionBriefCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\task_objectives.json")),
		"task_objectives.json parses");
	CHECK(cat.Count() >= 900, "loaded full retail stage-brief roster");

	const MissionBrief* b = cat.Find("AE_BCS0_Ter1_B_Stage01");
	CHECK(b != nullptr, "AE_BCS0_Ter1_B_Stage01 present");
	CHECK(b && b->template_id == "AE_BCS0_Ter1_B", "brief template_id parsed");
	CHECK(b && b->stage == 1, "brief stage parsed");
	CHECK(b && !b->owner_brief.empty(), "owner_brief parsed");
	CHECK(b && !b->dispatch_brief.empty(), "dispatch_brief parsed");
	// Retail inline markup preserved verbatim (angle brackets round-trip, not mangled \u003c).
	CHECK(b && b->owner_brief.find("<Col: StageText>") != std::string::npos, "brief markup preserved");
	CHECK(b && b->owner_brief.find("u003c") == std::string::npos, "no mangled u003c escape");

	// ForTemplate returns every stage for a template, ordered ascending by stage number.
	auto stages = cat.ForTemplate("AE_BCS0_Ter1_B");
	CHECK(stages.size() >= 4, "multi-stage template returns all stages");
	bool ordered = true;
	for (size_t i = 1; i < stages.size(); ++i) if (stages[i-1]->stage > stages[i]->stage) ordered = false;
	CHECK(ordered, "ForTemplate is stage-ordered");
	CHECK(!stages.empty() && stages.front()->stage == 1, "first stage is 1");

	// End-to-end via WorldService, plus join integrity: every brief's template resolves to a
	// canonical mission title (the two id spaces are 1:1).
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "taskobjectives world init");
	CHECK(w.mission_briefs.Count() >= 900, "world loaded stage briefs");
	int unresolved = 0;
	for (const auto& kv : w.mission_briefs.briefs)
		if (w.mission_titles.Find(kv.second.template_id) == nullptr) ++unresolved;
	CHECK(unresolved == 0, "every brief template_id resolves to a canonical mission title");
}

void TestTaskOperationsFromRetail() {
	// M15/D14: per-operation objective-type HUD labels parsed from Content/Data/task_operations.json
	// (extracted from the retail TaskOperations.INT, mirror of the SDD table TaskOperation). Proves
	// the Domain carries the short label shown for each mission operation type.
	MissionOperationCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\task_operations.json")),
		"task_operations.json parses");
	CHECK(cat.Count() >= 300, "loaded full retail operation-label roster");

	// Known retail anchors (verified from TaskOperations.INT).
	CHECK(cat.LabelFor("AntiGraffiti10NoHoldPoints") == "Graffiti Target", "graffiti op label");
	CHECK(cat.LabelFor("CheckpointAllAtOnce05") == "Checkpoint", "checkpoint op label");
	CHECK(cat.LabelFor("Escape120") == "Escape!", "escape op label");
	// Missing id yields the caller default, not a crash.
	CHECK(cat.LabelFor("NoSuchOperationId", "?") == "?", "missing op returns default");
	CHECK(cat.Find("NoSuchOperationId") == nullptr, "missing op Find is null");

	// Anti-mangling invariant: ConvertTo-Json \uXXXX-escaping must have been decoded, so no label
	// carries a literal "u0027"/"u0026" and the exclamation in "Escape!" round-trips.
	int mangled = 0;
	for (const auto& kv : cat.ops)
		if (kv.second.find("u0027") != std::string::npos || kv.second.find("u0026") != std::string::npos) ++mangled;
	CHECK(mangled == 0, "no mangled \\uXXXX escape in any op label");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "taskoperations world init");
	CHECK(w.mission_ops.Count() >= 300, "world loaded operation labels");
	CHECK(w.mission_ops.LabelFor("CheckpointAllAtOnce05") == "Checkpoint", "world op label resolves");
}

void TestMissionResultReasonsFromRetail() {
	// M15/D14: authentic mission end-screen Win/Lose/Draw messages parsed from
	// Content/Data/mission_result_reasons.json (extracted from the retail MissionResultReasons.INT,
	// mirror of the SDD table MissionResultReason). Complements the Domain's mission
	// win/fail/timeout/opposition resolution with the exact retail text shown per result reason.
	MissionResultReasonCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\mission_result_reasons.json")),
		"mission_result_reasons.json parses");
	CHECK(cat.Count() >= 15, "loaded full retail result-reason roster");

	// Known retail anchors (verified from MissionResultReasons.INT).
	CHECK(cat.WinMessage("TimedOut") == "The other side ran out of time", "timedout win message");
	CHECK(cat.LoseMessage("TimedOut") == "You ran out of time", "timedout lose message");
	CHECK(cat.WinMessage("WonFinalObjective") == "You completed the mission", "final objective win message");
	CHECK(cat.WinMessage("CompletedUnopposed") == "You have completed your objectives without opposition",
		"unopposed win message");
	// Apostrophe must round-trip literally (ConvertTo-Json \u0027 decoded by the extractor).
	CHECK(cat.LoseMessage("SideTooSmall") == "Your side doesn't have enough players",
		"apostrophe round-trips in lose message");
	// Missing id yields the caller default, not a crash.
	CHECK(cat.WinMessage("NoSuchReasonId", "?") == "?", "missing reason returns default");
	CHECK(cat.Find("NoSuchReasonId") == nullptr, "missing reason Find is null");

	// Anti-mangling invariant: no message carries a literal "u0027"/"u0026".
	int mangled = 0;
	for (const auto& kv : cat.reasons)
		if (kv.second.win_message.find("u0027") != std::string::npos ||
			kv.second.lose_message.find("u0027") != std::string::npos ||
			kv.second.draw_message.find("u0027") != std::string::npos ||
			kv.second.win_message.find("u0026") != std::string::npos) ++mangled;
	CHECK(mangled == 0, "no mangled \\uXXXX escape in any result-reason message");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "resultreasons world init");
	CHECK(w.mission_result_reasons.Count() >= 15, "world loaded result reasons");
	CHECK(w.mission_result_reasons.LoseMessage("TimedOut") == "You ran out of time",
		"world result-reason message resolves");
}

void TestThreatRatingsFromRetail() {
	// Matchmaking threat-rating tiers parsed from Content/Data/threat_ratings.json (extracted from
	// the retail ThreatLevels.INT, mirror of SDD table ThreatLevel). This is the skill bracket shown
	// by a player's name plus the AllowedDistrictThreats district-join gating ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â‚¬Å¡Ã‚Â¬ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â distinct from the
	// notoriety/prestige "heat" points system (threat_table.json / ThreatSystem).
	ThreatRatingCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\threat_ratings.json")),
		"threat_ratings.json parses");
	CHECK(cat.Count() == 5, "loaded all five retail threat-rating tiers");

	// Display names (verified from ThreatLevels.INT).
	CHECK(cat.DisplayedName("ThreatLevel_Training") == "In Training", "training display name");
	CHECK(cat.DisplayedName("ThreatLevel_01") == "Green", "green display name");
	CHECK(cat.DisplayedName("ThreatLevel_04") == "Gold", "gold display name");
	CHECK(cat.FindByDisplayedName("Bronze") != nullptr &&
		cat.FindByDisplayedName("Bronze")->id == "ThreatLevel_02", "reverse lookup by display name");
	// Rank ordering: entry 0 is In Training.
	CHECK(!cat.ratings.empty() && cat.ratings[0].rank == 0 &&
		cat.ratings[0].displayed_name == "In Training", "ratings sorted by rank");

	// Matchmaking district-join gate (AllowedDistrictThreats semantics).
	CHECK(cat.CanJoinDistrictThreat("ThreatLevel_02", "Green"), "bronze may join green district");
	CHECK(cat.CanJoinDistrictThreat("ThreatLevel_02", "Gold"), "bronze may join gold district");
	CHECK(!cat.CanJoinDistrictThreat("ThreatLevel_03", "Green"), "silver may not join green district");
	CHECK(cat.CanJoinDistrictThreat("ThreatLevel_04", "Silver"), "gold may join silver district");
	CHECK(!cat.CanJoinDistrictThreat("ThreatLevel_04", "Bronze"), "gold may not join bronze district");
	// Empty AllowedDistrictThreats confines the rating to its own bracket.
	CHECK(cat.CanJoinDistrictThreat("ThreatLevel_01", "Green"), "green confined to green district");
	CHECK(!cat.CanJoinDistrictThreat("ThreatLevel_01", "Bronze"), "green may not join bronze district");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.DisplayedName("NoSuchRating", "?") == "?", "missing rating returns default");
	CHECK(cat.Find("NoSuchRating") == nullptr, "missing rating Find is null");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "threatratings world init");
	CHECK(w.threat_ratings.Count() == 5, "world loaded threat ratings");
	CHECK(w.threat_ratings.CanJoinDistrictThreat("ThreatLevel_04", "Silver"),
		"world matchmaking gate resolves");
}

void TestFactionInfoFromRetail() {
	// Faction-selection screen content parsed from Content/Data/factions.json (extracted from the
	// retail Factions.INT, mirror of SDD table Faction): display names + the General Info / Enforcer
	// / Criminal lore, with paragraph breaks (U+21B5 pairs) preserved as "\n\n".
	FactionInfoCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\factions.json")), "factions.json parses");
	CHECK(cat.Count() == 3, "loaded None/Enforcer/Criminal (DNT 'Both' skipped)");

	// Display names + info titles (verified from Factions.INT).
	CHECK(cat.Find("None") != nullptr && cat.Find("None")->info_title == "General Info",
		"None entry titled 'General Info'");
	CHECK(cat.Find("Enforcer") != nullptr && cat.Find("Enforcer")->display_name == "Enforcer",
		"enforcer display name");
	CHECK(cat.Find("Criminal") != nullptr && cat.Find("Criminal")->info_title == "Criminal",
		"criminal info title");

	// Faction enum mapping + General Info convenience.
	CHECK(cat.ForFaction(Faction::Enforcer) != nullptr &&
		cat.ForFaction(Faction::Enforcer)->id == "Enforcer", "ForFaction(Enforcer) resolves");
	CHECK(cat.ForFaction(Faction::Criminal) != nullptr &&
		cat.ForFaction(Faction::Criminal)->id == "Criminal", "ForFaction(Criminal) resolves");
	CHECK(cat.GeneralInfo() != nullptr && cat.GeneralInfo()->id == "None", "GeneralInfo() is the None row");

	// Rank ordering: entry 0 is General Info (None).
	CHECK(!cat.factions.empty() && cat.factions[0].rank == 0 && cat.factions[0].id == "None",
		"factions sorted by rank");

	// Lore content anchors (locks 1:1 fidelity of the faction picker text).
	const std::string enf = cat.Description("Enforcer");
	const std::string crim = cat.Description("Criminal");
	CHECK(enf.find("The concept of Enforcers was created by Mayor Jane Derren") == 0,
		"enforcer lore opening verbatim");
	CHECK(enf.find("Prentiss Tigers") != std::string::npos, "enforcer lore names Prentiss Tigers");
	CHECK(crim.find("G-Kings") != std::string::npos && crim.find("Blood Roses") != std::string::npos,
		"criminal lore names G-Kings and Blood Roses");

	// Apostrophe round-trips (no \uXXXX mangling).
	CHECK(crim.find("It's always been here") != std::string::npos, "apostrophe round-trips");
	CHECK(crim.find("\\u") == std::string::npos, "no leftover \\u escapes");

	// Paragraph breaks preserved as real "\n\n" (proper JSON unescape, not literal 'n').
	CHECK(enf.find("\n\n") != std::string::npos, "enforcer lore keeps blank-line paragraph breaks");
	CHECK(enf.find("\\n") == std::string::npos, "no literal backslash-n leaked through");
	CHECK(cat.ParagraphCount("Enforcer") >= 3, "enforcer lore has multiple paragraphs");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Both") == nullptr, "DNT 'Both' faction absent");
	CHECK(cat.Description("Nope", "?") == "?", "missing faction returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "factioninfo world init");
	CHECK(w.faction_info.Count() == 3, "world loaded faction info");
	CHECK(w.faction_info.ForFaction(Faction::Criminal) != nullptr &&
		w.faction_info.ForFaction(Faction::Criminal)->info_title == "Criminal",
		"world faction lookup resolves");
}

void TestHeatLevelDescriptionsFromRetail() {
	// The apbdb /heat descriptions (a verbatim mirror of the retail HeatLevels.INT SDD table)
	// are now threaded through ThreatTier and the DomainSnapshot HUD bridge. Criminal =
	// notoriety N0-N5, Enforcer = prestige P0-P5. Previously the ThreatTier parser dropped
	// the description field so this player-facing text never reached the game.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "heatlevels world init");
	CHECK(w.threat.loaded_from_table, "threat table loaded from apbdb data");

	// Criminal notoriety tier descriptions (verified against HeatLevels.INT).
	w.threat.faction = Faction::Criminal;
	w.threat.points = 0;
	CHECK(w.threat.CurrentTier().description.find("keeping a low profile") != std::string::npos,
		"notoriety L0 description present");
	w.threat.points = 100000; // saturate to the top tier
	CHECK(w.threat.CurrentTier().level == 5, "reaches notoriety L5");
	CHECK(w.threat.CurrentTier().description.find("major price on your head") != std::string::npos,
		"notoriety L5 description present");
	// Apostrophe round-trips with no \uXXXX mangling.
	CHECK(w.threat.CurrentTier().description.find("\\u") == std::string::npos, "no leftover \\u escape");

	// Enforcer prestige tier descriptions.
	w.threat.faction = Faction::Enforcer;
	w.threat.points = 0;
	CHECK(w.threat.CurrentTier().description.find("almost as bad as the Criminals") != std::string::npos,
		"prestige P0 description present");

	// Every notoriety and prestige tier carries a non-empty description.
	CHECK(w.threat.criminal_tiers.size() == 6 && w.threat.enforcer_tiers.size() == 6,
		"six notoriety + six prestige tiers");
	for (const auto& t : w.threat.criminal_tiers)
		CHECK(!t.description.empty(), "every notoriety tier has a description");
	for (const auto& t : w.threat.enforcer_tiers)
		CHECK(!t.description.empty(), "every prestige tier has a description");

	// DomainSnapshot HUD bridge surfaces the current tier level, name and description.
	WorldService w2;
	CHECK(w2.InitFromDataDir(DataDir()), "heat snapshot init");
	CHECK(w2.CreateCharacter("Heat", Faction::Criminal), "heat char");
	w2.threat.points = 2000; // ~ notoriety L3/L4
	DomainSnapshot s = w2.CaptureSnapshot();
	CHECK(s.threat_tier_name.find("NotorietyLevel") == 0, "snapshot exposes notoriety tier name");
	CHECK(!s.threat_tier_description.empty(), "snapshot exposes heat description");
	CHECK(s.threat_level == w2.threat.CurrentTier().level, "snapshot threat level matches current tier");
}

void TestOrganisationsFromRetail() {
	// Organisation catalog parsed from Content/Data/organisations.json (extracted from the
	// retail Organisations.INT, mirror of SDD table Organisation): the contact gangs, the
	// Joker weapon vendors and the Armas store fronts. Names are verbatim from the INT;
	// faction/kind are canonical APB classification (SDD Organisation.Faction is cooked away).
	OrganisationCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\organisations.json")),
		"organisations.json parses");
	CHECK(cat.Count() == 19, "loaded all 19 named organisations (empty 'None' skipped)");

	// Display names verbatim from Organisations.INT.
	CHECK(cat.Name("GKings") == "G-Kings", "G-Kings display name");
	CHECK(cat.Name("Praetorian") == "Praetorians", "Praetorians display name");
	CHECK(cat.Name("RIOT") == "Red Hill Institute of Technology", "RIOT full display name");
	CHECK(cat.Name("JokerDistribution") == "Joker Distribution", "Joker Distribution display name");

	// Faction affiliation.
	CHECK(cat.Find("BloodRoses") != nullptr && cat.Find("BloodRoses")->faction == "Criminal",
		"Blood Roses are Criminal");
	CHECK(cat.Find("SPPD") != nullptr && cat.Find("SPPD")->faction == "Enforcer",
		"SPPD is Enforcer");
	CHECK(cat.Find("Armas") != nullptr && cat.Find("Armas")->faction == "None",
		"Armas store is neutral");

	// Kind classification (contact grouping vs store filter).
	CHECK(cat.Find("GKings")->kind == "gang", "G-Kings is a gang");
	CHECK(cat.Find("ArmasNTJB")->kind == "store", "Joker Box No-Trade is a store front");
	CHECK(cat.Find("JokerDistribution")->kind == "vendor", "Joker Distribution is a vendor");

	// Faction grouping (rank-ordered) for the contact UI: 6 criminal + 6 enforcer + 7 neutral.
	auto crim = cat.ForFaction(Faction::Criminal);
	auto enf  = cat.ForFaction(Faction::Enforcer);
	CHECK(crim.size() == 6 && (int32_t)crim.size() == cat.CountForFaction("Criminal"),
		"six criminal-affiliated organisations");
	CHECK(enf.size() == 6 && (int32_t)enf.size() == cat.CountForFaction("Enforcer"),
		"six enforcer-affiliated organisations");
	CHECK(cat.CountForFaction("None") == 7, "seven neutral (store/vendor/tutorial/seasonal) orgs");
	for (size_t i = 1; i < crim.size(); ++i)
		CHECK(crim[i - 1]->rank < crim[i]->rank, "criminal orgs stay rank-ordered");

	// Kind grouping for the Armas store filter.
	CHECK(cat.CountOfKind("gang") == 8, "eight gangs classified");
	CHECK(cat.CountOfKind("store") == 3, "three Armas store fronts");

	// Rank ordering: entry 0 is the criminal default.
	CHECK(!cat.organisations.empty() && cat.organisations[0].rank == 0 &&
		cat.organisations[0].id == "CriminalDefault", "organisations sorted by rank");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Nope") == nullptr, "missing org Find is null");
	CHECK(cat.Name("Nope", "?") == "?", "missing org returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "organisations world init");
	CHECK(w.organisations.Count() == 19, "world loaded organisations");
	CHECK(w.organisations.Name("PrentissTigers") == "Prentiss Tigers", "world org lookup resolves");
}

void TestMedalsFromRetail() {
	// Medal / award catalog parsed from Content/Data/medals.json (extracted from the retail
	// Medals.INT, mirror of SDD table Medal): the post-mission / profile achievements
	// (kill streaks, mission-win awards, situational feats, timed multi-kills) and the
	// negative "Demerit" dishonours. Category = the id's first token; titles/descriptions
	// are verbatim from the INT.
	MedalCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\medals.json")), "medals.json parses");
	CHECK(cat.Count() == 82, "loaded all 82 medals");

	// Titles + descriptions verbatim from Medals.INT.
	CHECK(cat.Title("KillStreak_05") == "Kill Streak Rank 1", "kill streak rank 1 title");
	CHECK(cat.Description("KillStreak_05") == "Kill 5 opponents without dying",
		"kill streak rank 1 criteria");
	CHECK(cat.Title("BigWin_All") == "Champion", "Champion title");
	CHECK(cat.Title("Situational_VIPKill") == "Assassination", "VIP kill titled Assassination");

	// Apostrophe round-trips with no \uXXXX mangling.
	CHECK(cat.Title("Situational_SuperKill") == "Kill 'Em All", "apostrophe title round-trips");
	CHECK(cat.Title("Situational_SuperKill").find("\\u") == std::string::npos, "no leftover \\u escape");

	// Category classification (id's first token).
	CHECK(cat.Find("BigWin_Arrest_10_100") != nullptr &&
		cat.Find("BigWin_Arrest_10_100")->category == "BigWin", "BigWin category");
	CHECK(cat.Find("TimeLimit_Kills_2") != nullptr &&
		cat.Find("TimeLimit_Kills_2")->category == "TimeLimit", "TimeLimit category");
	CHECK(cat.Find("KillBehind") != nullptr && cat.Find("KillBehind")->category == "KillBehind",
		"standalone KillBehind category");

	// Dishonour demerits are flagged negatively.
	CHECK(cat.Find("Dishonour_AFK") != nullptr && cat.Find("Dishonour_AFK")->IsDishonour(),
		"AFK is a demerit dishonour");
	CHECK(!cat.Find("BigWin_All")->IsDishonour(), "Champion is not a dishonour");
	// SelfKill 13 ranks + FriendlyKill 10 + FriendlyStun 5 + ArrestedKill 10 + AFK = 39 dishonours.
	CHECK(cat.Dishonours() == 39, "thirty-nine demerit dishonours");
	CHECK(cat.Dishonours() == cat.CountForCategory("Dishonour"), "dishonour count matches category count");

	// Category grouping, order preserved.
	auto streaks = cat.ForCategory("KillStreak"); // 05,10,15,20,25,30 = 6 ranks
	CHECK(streaks.size() == 6, "six kill-streak ranks");
	for (size_t i = 1; i < streaks.size(); ++i)
		CHECK(streaks[i - 1]->order < streaks[i]->order, "kill-streak medals stay ordered");
	CHECK(cat.CountForCategory("BigWin") >= 10, "big-win medal family present");

	// Six distinct categories, first is KillStreak (file order).
	auto cats = cat.Categories();
	CHECK(cats.size() == 6, "six distinct medal categories");
	CHECK(!cats.empty() && cats[0] == "KillStreak", "first category is KillStreak");

	// Rank ordering: entry 0 is the first medal in the file.
	CHECK(!cat.medals.empty() && cat.medals[0].order == 0 && cat.medals[0].id == "KillStreak_05",
		"medals sorted by order");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Nope") == nullptr, "missing medal Find is null");
	CHECK(cat.Title("Nope", "?") == "?", "missing medal returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "medals world init");
	CHECK(w.medals.Count() == 82, "world loaded medals");
	CHECK(w.medals.Title("Situational_MeleeKill") == "Battery", "world medal lookup resolves");
}

void TestStreetNamesFromRetail() {
	// Street-name catalog parsed from Content/Data/street_names.json (extracted from the retail
	// StreetName.INT, mirror of SDD table StreetName): the world-map / minimap location labels
	// and intersection callouts used by mission waypoints ("meet at Shianxi Boulevard").
	StreetNameCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\street_names.json")), "street_names.json parses");
	CHECK(cat.Count() == 191, "loaded all 191 street names");

	// Named streets and intersection labels resolve verbatim.
	CHECK(cat.Name("FinancialShianxi") == "Shianxi Boulevard", "named street resolves");
	CHECK(cat.Name("Waterfront_X_HaeinsaAbrams") == "Haeinsa & Abrams", "ampersand intersection round-trips");

	// Kind classification: keys containing "_X_" are intersections, others are streets.
	CHECK(cat.Find("FinancialShianxi")->kind == "street", "named street kind");
	CHECK(!cat.Find("FinancialShianxi")->IsIntersection(), "named street is not an intersection");
	CHECK(cat.Find("Waterfront_X_HaeinsaAbrams")->IsIntersection(), "junction is an intersection");

	// The retail typo key ("Financia_X_..." missing the trailing 'l') is still classified as a
	// Financial-district intersection, with its label preserved verbatim.
	const StreetName* typo = cat.Find("Financia_X_BankBreakwater");
	CHECK(typo != nullptr, "retail typo key present");
	CHECK(typo->district == "Financial", "typo key classified Financial");
	CHECK(typo->IsIntersection(), "typo key is an intersection");
	CHECK(typo->name == "Bank & Breakwater", "typo key label verbatim");

	// District split and kind split (measured from the extracted data).
	CHECK(cat.CountForDistrict("Financial") == 84, "financial street count");
	CHECK(cat.CountForDistrict("Waterfront") == 107, "waterfront street count");
	CHECK(cat.CountOfKind("street") == 78, "named-street count");
	CHECK(cat.CountOfKind("intersection") == 113, "intersection count");
	CHECK(cat.CountForDistrict("Financial") + cat.CountForDistrict("Waterfront") == cat.Count(),
		"every street is in a known district");

	// Two districts, Financial listed first (file order).
	auto dists = cat.Districts();
	CHECK(dists.size() == 2, "two districts");
	CHECK(dists[0] == "Financial", "financial district first");
	CHECK(cat.ForDistrict("Financial").size() == 84, "ForDistrict count matches");

	// Accented labels round-trip to UTF-8 without a stray \u escape (Malaga Drive has an accent).
	CHECK(cat.Name("FinancialMalaga").find("laga Drive") != std::string::npos, "accented label round-trips");
	CHECK(cat.Name("FinancialMalaga").find("\\u") == std::string::npos, "no stray unicode escape");

	// Stable file order.
	CHECK(!cat.streets.empty() && cat.streets[0].order == 0 && cat.streets[0].id == "FinancialShianxi",
		"streets sorted by order");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Nope") == nullptr, "missing street Find is null");
	CHECK(cat.Name("Nope", "?") == "?", "missing street returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "street names world init");
	CHECK(w.street_names.Count() == 191, "world loaded street names");
	CHECK(w.street_names.Name("FinancialMain") == "Main Street", "world street lookup resolves");
}

void TestAmmoCategoriesFromRetail() {
	// Ammunition-category catalog parsed from Content/Data/ammo_categories.json (extracted from
	// the retail AmmoCategories.INT, mirror of SDD table AmmoCategories): the weapon ammo pools
	// and the HUD ammo-counter text ("<Num> bullets") shown next to the equipped weapon.
	AmmoCategoryCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\ammo_categories.json")), "ammo_categories.json parses");
	CHECK(cat.Count() == 24, "loaded all 24 ammo categories");

	// Verbatim field lookups.
	CHECK(cat.Name("Rifle") == "Rifle Ammo", "rifle full name");
	CHECK(cat.Abbreviated("44Magnum") == "Magnum", "magnum abbreviated label");
	CHECK(cat.QuantityText("Shotgun") == "<Num> shells", "shotgun quantity template");
	CHECK(cat.Find("Sniper")->name_abbreviated == "High-velocity", "sniper abbreviated label");

	// The "None" no-ammo sentinel is kept (all fields "Not Currently Available").
	CHECK(cat.Find("None") != nullptr, "None sentinel present");
	CHECK(cat.Name("None") == "Not Currently Available", "None sentinel name");

	// The unused empty-name Blowtorch_Fuel row is dropped by the extractor.
	CHECK(cat.Find("Blowtorch_Fuel") == nullptr, "empty-name row dropped");

	// FormatQuantity substitutes the live count into the "<Num>" token, exactly as the HUD does.
	CHECK(cat.FormatQuantity("Rifle", 30) == "30 rounds", "rifle counter formats");
	CHECK(cat.FormatQuantity("Shotgun", 6) == "6 shells", "shotgun counter formats");
	CHECK(cat.FormatQuantity("GrenadeFrag", 2) == "2 grenades", "grenade counter formats");

	// Angle-bracket "<Num>" token survives extraction (not mangled into a \u escape).
	CHECK(cat.QuantityText("9mmPistol").find("<Num>") != std::string::npos, "num token preserved");

	// Stable file order: None is first.
	CHECK(!cat.ammo.empty() && cat.ammo[0].order == 0 && cat.ammo[0].id == "None",
		"ammo sorted by order");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Nope") == nullptr, "missing ammo Find is null");
	CHECK(cat.Name("Nope", "?") == "?", "missing ammo returns default");
	CHECK(cat.FormatQuantity("Nope", 5, "?") == "?", "missing ammo format default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "ammo categories world init");
	CHECK(w.ammo_categories.Count() == 24, "world loaded ammo categories");
	CHECK(w.ammo_categories.FormatQuantity("MachineGun", 90) == "90 bullets", "world ammo format resolves");
}

void TestScoreboardDescriptionsFromRetail() {
	// Scoreboard-column tooltip catalog parsed from Content/Data/scoreboard_descriptions.json
	// (extracted from the retail ScoreboardDescriptions.INT, mirror of SDD table
	// ScoreboardDescription): the hover text for each end-of-mission / chaos scoreboard column.
	ScoreboardDescriptionCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\scoreboard_descriptions.json")), "scoreboard_descriptions.json parses");
	CHECK(cat.Count() == 22, "loaded all 22 scoreboard columns");

	// Verbatim tooltip lookups.
	CHECK(cat.DisplayText("Arrests") == "Number of arrests during the mission", "arrests tooltip");
	CHECK(cat.DisplayText("Kills") == "Number of kills during the mission", "kills tooltip");
	CHECK(cat.DisplayText("PlayerName") == "Player Name", "player-name tooltip");

	// Apostrophe round-trips through the \u-restore (not left as a \u escape).
	CHECK(cat.DisplayText("Threat") == "Player's threat level", "threat tooltip apostrophe");
	CHECK(cat.DisplayText("Threat").find("\\u") == std::string::npos, "no literal \\u in tooltip");

	// Premium columns carry the bonus-cash / bonus-standing wording.
	CHECK(cat.DisplayText("CashPremium").find("Premium") != std::string::npos, "cash-premium tooltip");

	// Stable file order: Arrests is first (alphabetical in the INT).
	CHECK(!cat.columns.empty() && cat.columns[0].order == 0 && cat.columns[0].id == "Arrests",
		"scoreboard sorted by order");

	// Missing id yields the caller default, not a crash.
	CHECK(cat.Find("Nope") == nullptr, "missing column Find is null");
	CHECK(cat.DisplayText("Nope", "?") == "?", "missing column returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "scoreboard world init");
	CHECK(w.scoreboard_descriptions.Count() == 22, "world loaded scoreboard columns");
	CHECK(w.scoreboard_descriptions.DisplayText("Score") == "Amount of score awarded", "world scoreboard resolves");
}

void TestHUDCombatMessagesFromRetail() {
	// On-screen combat score-feed catalog parsed from Content/Data/hud_combat_messages.json
	// (extracted from the retail HUDCombatMessages.INT, mirror of SDD table HUDCombatMessage):
	// the two-line floating messages ("Enemy Killed", "Objective Complete", "Demerit!", ...).
	HUDCombatMessageCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\hud_combat_messages.json")), "hud_combat_messages.json parses");
	CHECK(cat.Count() == 145, "loaded all 145 combat feed messages");

	// Verbatim two-line lookups.
	CHECK(cat.Line2("Score_Combat_KillEnemy") == "Enemy Killed", "kill-enemy bottom line");
	CHECK(cat.Line2("Score_Combat_KillEnemyAssist") == "Kill Assist", "kill-assist bottom line");
	CHECK(cat.Line2("Score_Combat_ArrestEnemy") == "Enemy Arrested", "arrest bottom line");
	CHECK(cat.Line2("Score_Mission_CSA_Arson") == "Objective Complete", "objective-complete bottom line");
	CHECK(cat.Line0("Score_Mission_CSA_Arson") == "Arson", "arson top-line label");

	// Angle-bracket tokens survive extraction (not mangled into a \u escape).
	CHECK(cat.Line0("Score_Combat_KillEnemy") == "<CharacterNameA>", "character-name token preserved");
	CHECK(cat.Line0("Score_Earned_MedalBigWin_All") == "<MedalName>", "medal-name token preserved");
	CHECK(cat.Line0("Score_Combat_KillEnemy").find("\\u") == std::string::npos, "no literal \\u in token");

	// The "($<Score>)" bribe line keeps both the '$' and the token.
	CHECK(cat.Line2("Minigame_Survival_Economy_Cash_Delivered") == "($<Score>) remaining team cash.",
		"score+dollar token preserved");

	// Dishonour feed messages read "Demerit!".
	CHECK(cat.Line2("Score_KillTeam_Chaos") == "Demerit!", "teamkill demerit line");

	// Entries with one empty line are kept (e.g. Match Won has an empty top line).
	CHECK(cat.Find("Score_Match_Won") != nullptr && cat.Line0("Score_Match_Won").empty(), "match-won empty top line kept");
	CHECK(cat.Line2("Score_Match_Won") == "Match Won", "match-won bottom line");

	// Unused both-empty Easter placeholders are dropped by the extractor.
	CHECK(cat.Find("Minigame_Mugging_Easter_GainedItems") == nullptr, "both-empty placeholder dropped");

	// FormatLine0 substitutes the single "<...>" token with a live value, as the HUD does.
	CHECK(cat.FormatLine0("Score_Combat_KillEnemy", "xoified") == "xoified", "kill-enemy name substituted");
	CHECK(cat.FormatLine2("Minigame_Survival_Economy_Cash_Delivered", "1234") == "($1234) remaining team cash.",
		"score token substituted");

	// Stable file order + missing-id safety.
	CHECK(!cat.messages.empty() && cat.messages[0].order == 0 && cat.messages[0].id == "Score_Combat_ArrestEnemy",
		"combat feed sorted by order");
	CHECK(cat.Find("Nope") == nullptr, "missing feed Find is null");
	CHECK(cat.Line2("Nope", "?") == "?", "missing feed returns default");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "hud combat world init");
	CHECK(w.hud_combat_messages.Count() == 145, "world loaded combat feed messages");
	CHECK(w.hud_combat_messages.Line2("Score_Combat_StunnedEnemy") == "Enemy Stunned", "world combat feed resolves");
}

static void TestModifierEffectsFromRetail() {
	std::cout << "-- ModifierEffects (mod tooltips) from retail --\n";
	// Character / vehicle / weapon / consumable modification tooltips parsed from
	// Content/Data/modifier_effects.json (extracted from the retail ModifierEffects.INT, mirror of
	// the cooked SDD table "ModifierEffect"). Each mod owns one or more colour-marked-up lines whose
	// keys are scattered across the INT (Description, Description_2, Description_3, ...).
	ModifierEffectCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\modifier_effects.json")), "modifier_effects.json parses");
	CHECK(cat.Count() == 163, "loaded all 163 modifier effects");

	// Four mod categories, sorted distinct.
	std::vector<std::string> cats = cat.Categories();
	CHECK(cats.size() == 4 && cats[0] == "Character" && cats[1] == "Usable" && cats[2] == "Vehicle" && cats[3] == "Weapon",
		"four mod categories present and sorted");
	CHECK((int)cat.ForCategory("Weapon").size() == 92, "92 weapon mods");
	CHECK((int)cat.ForCategory("Vehicle").size() == 40, "40 vehicle mods");
	CHECK((int)cat.ForCategory("Character").size() == 27, "27 character mods");
	CHECK((int)cat.ForCategory("Usable").size() == 4, "4 usable/consumable mods");

	// Verbatim single-line lookup (markup preserved exactly as in the INT).
	CHECK(cat.LineCount("Character_JumpControl") == 1, "jump-control has one line");
	CHECK(cat.Lines("Character_JumpControl")[0] == "<Color:R=0 G=1 B=0>+400%<Color:R=1 G=1 B=1> air control",
		"jump-control markup line verbatim");

	// Multi-line mods keep every line, in order (scattered _Description_N keys reassembled).
	CHECK(cat.LineCount("Character_HardLanding") == 2, "hard-landing has two lines");
	CHECK(cat.LineCount("Usable_Consumable_Epinephrine") == 4, "epinephrine has four lines");
	CHECK(cat.LineCount("Character_FieldSupplier_Health") == 3, "field-supplier-health reassembled to three lines");

	// Angle-bracket markup survives extraction (never mangled into a \u escape).
	CHECK(cat.Lines("Character_JumpControl")[0].find("\\u") == std::string::npos, "no literal \\u in markup");

	// ParseSegments turns a raw line into coloured runs: "+400%" green, then " air control" white.
	std::vector<ColorSegment> segs = ModifierEffectCatalog::ParseSegments(cat.Lines("Character_JumpControl")[0]);
	CHECK(segs.size() == 2, "jump-control parses into two colour runs");
	CHECK(segs[0].text == "+400%" && segs[0].r == 0.0f && segs[0].g == 1.0f && segs[0].b == 0.0f,
		"first run is green +400%");
	CHECK(segs[1].text == " air control" && segs[1].r == 1.0f && segs[1].g == 1.0f && segs[1].b == 1.0f,
		"second run is white air control");

	// PlainText strips all markup down to readable text.
	CHECK(ModifierEffectCatalog::PlainText(cat.Lines("Character_JumpControl")[0]) == "+400% air control",
		"plain text strips colour markup");

	// A malformed tag with no closing '>' is treated as literal text (retail INT quirk preserved).
	CHECK(ModifierEffectCatalog::PlainText(cat.Lines("Character_FieldSupplier_Health")[0]) == "+100%<Color:R=1 G=1 B=1 health",
		"malformed no-'>' tag kept literal");

	// Stable file order + missing-id safety.
	CHECK(!cat.effects.empty() && cat.effects[0].order == 0 && cat.effects[0].id == "Character_JumpControl",
		"mods sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing mod Find is null");
	CHECK(cat.LineCount("Nope") == 0, "missing mod has zero lines");
	CHECK(cat.Lines("Nope").empty(), "missing mod Lines is empty");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "modifier effects world init");
	CHECK(w.modifier_effects.Count() == 163, "world loaded modifier effects");
	CHECK(w.modifier_effects.Lines("Weapon_Bandolier1").size() == 2, "world modifier effect resolves");
}

static void TestModifierItemTypesFromRetail() {
	std::cout << "-- ModifierItemTypes (mod items) from retail --\n";
	// The purchasable / equippable modification items parsed from Content/Data/modifier_item_types.json
	// (extracted from the retail ModifierItemTypes.INT, mirror of the cooked SDD table
	// "ModifierItemType"). Each item carries a type label + flavour description, and its id maps to a
	// ModifierEffects stat row via EffectId() (strip "FnMod_"/"FNMod_" prefix + trailing "_Tutorial").
	ModifierItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\modifier_item_types.json")), "modifier_item_types.json parses");
	CHECK(cat.Count() == 284, "loaded all 284 modifier item types");

	// Four categories, sorted distinct (Character/Special/Vehicle/Weapon).
	std::vector<std::string> cats = cat.Categories();
	CHECK(cats.size() == 4 && cats[0] == "Character" && cats[1] == "Special" && cats[2] == "Vehicle" && cats[3] == "Weapon",
		"four item categories present and sorted");
	CHECK((int)cat.ForCategory("Weapon").size() == 160, "160 weapon items");
	CHECK((int)cat.ForCategory("Vehicle").size() == 69, "69 vehicle items");
	CHECK((int)cat.ForCategory("Character").size() == 53, "53 character items");
	CHECK((int)cat.ForCategory("Special").size() == 2, "2 special/placeholder items");

	// Type label + description are split on the U+21B5 glyph, trailing " -" stripped from the label.
	CHECK(cat.TypeLabel("FnMod_Character_Kevlar1") == "Health Modification", "kevlar1 type label");
	CHECK(cat.Description("FnMod_Character_Kevlar1") ==
		"Subdermal Kevlar Implants slightly decrease the amount of damage you take from incoming fire.",
		"kevlar1 flavour description verbatim");
	CHECK(cat.Category("FnMod_Vehicle_Nitro3") == "Vehicle", "nitro3 category");

	// Apostrophes/other printable punctuation survive extraction (no literal \u escape).
	CHECK(cat.Description("FnMod_Vehicle_Explosives1").find("vehicle's") != std::string::npos,
		"apostrophe preserved in description");
	CHECK(cat.Description("FnMod_Vehicle_Explosives1").find("\\u") == std::string::npos, "no literal \\u in description");

	// Flavour-only entries (no type label in the INT) keep an empty label + full description.
	CHECK(cat.TypeLabel("FnMod_Weapon_ExtendedBarrel").empty(), "flavour-only item has no type label");
	CHECK(cat.Description("FnMod_Weapon_ExtendedBarrel").find("ACES Extended Barrel") != std::string::npos,
		"flavour-only description kept");

	// EffectId() binds an item to its ModifierEffects stat row.
	CHECK(ModifierItemTypeCatalog::EffectId("FnMod_Vehicle_Explosives1") == "Vehicle_Explosives1",
		"EffectId strips FnMod_ prefix");
	CHECK(ModifierItemTypeCatalog::EffectId("FnMod_Weapon_Rifling3_Tutorial") == "Weapon_Rifling3",
		"EffectId strips prefix + _Tutorial suffix");
	CHECK(ModifierItemTypeCatalog::EffectId("Mod_None") == "Mod_None", "placeholder id passes through");
	CHECK(cat.EffectIdFor("FnMod_Vehicle_Explosives1") == "Vehicle_Explosives1", "EffectIdFor resolves stored item");

	// The binding actually resolves in the ModifierEffects catalog (the two catalogs join up).
	ModifierEffectCatalog eff;
	CHECK(eff.LoadFromJsonFile(DataDir() + std::string("\\modifier_effects.json")), "modifier_effects.json parses");
	const ModifierItemType* explo = cat.Find("FnMod_Vehicle_Explosives1");
	CHECK(explo != nullptr, "explosives1 item found");
	CHECK(eff.Find(ModifierItemTypeCatalog::EffectId(explo->id)) != nullptr,
		"item binds to a real modifier effect row");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0 && cat.items[0].id == "Mod_None", "items sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing item Find is null");
	CHECK(cat.TypeLabel("Nope", "?") == "?", "missing item returns default label");
	CHECK(cat.Description("Nope").empty(), "missing item description empty");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "modifier item types world init");
	CHECK(w.modifier_item_types.Count() == 284, "world loaded modifier item types");
	CHECK(w.modifier_item_types.TypeLabel("FnMod_Vehicle_Nitro1") == "Activated Modification",
		"world modifier item resolves");
}

static void TestRoleMilestonesFromRetail() {
	std::cout << "-- RoleMilestones (per-rank role progression) from retail --\n";
	// The individual role ranks/steps parsed from Content/Data/role_milestones.json (extracted from
	// the retail RoleMilestones.INT, mirror of the cooked SDD table "RoleMilestones"). Each milestone
	// has a display Title and, when it grants loot, a reward-mail Subject + Body. Its id maps back to a
	// player_roles id via RoleId() (strip the trailing "_<NN>" rank), with Rank() parsing the number.
	RoleMilestoneCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\role_milestones.json")), "role_milestones.json parses");
	CHECK(cat.Count() == 705, "loaded all 705 role milestones");

	// Title is the display string for the rank.
	CHECK(cat.Title("15th_Anniversary_Celebrations_01") == "15th Year Anniversary Celebrations - Rank 1",
		"anniversary rank 1 title verbatim");
	CHECK(cat.Title("15th_Anniversary_Celebrations_05") == "15th Year Anniversary Celebrations - Rank 5 (Complete)",
		"anniversary rank 5 title verbatim");

	// RoleId() strips the trailing "_<NN>" rank; Rank() parses the number. Ids with no numeric
	// suffix pass through unchanged with rank -1.
	CHECK(RoleMilestoneCatalog::RoleId("15th_Anniversary_Celebrations_01") == "15th_Anniversary_Celebrations",
		"RoleId strips trailing rank");
	CHECK(RoleMilestoneCatalog::Rank("15th_Anniversary_Celebrations_01") == 1, "Rank parses trailing number");
	CHECK(RoleMilestoneCatalog::Rank("15th_Anniversary_Celebrations_05") == 5, "Rank parses rank 5");
	CHECK(RoleMilestoneCatalog::RoleId("NoRankHere") == "NoRankHere", "RoleId leaves suffix-less id alone");
	CHECK(RoleMilestoneCatalog::Rank("NoRankHere") == -1, "Rank is -1 with no numeric suffix");
	CHECK(cat.RoleIdFor("15th_Anniversary_Celebrations_03") == "15th_Anniversary_Celebrations",
		"RoleIdFor resolves stored milestone");
	CHECK(cat.RankFor("15th_Anniversary_Celebrations_03") == 3, "RankFor resolves stored milestone");

	// ForRole groups a role's milestones, sorted ascending by rank.
	std::vector<const RoleMilestone*> anni = cat.ForRole("15th_Anniversary_Celebrations");
	CHECK(anni.size() == 5, "anniversary role has 5 milestones");
	CHECK(!anni.empty() && RoleMilestoneCatalog::Rank(anni.front()->id) == 1, "ForRole sorted ascending, rank 1 first");
	CHECK(anni.size() == 5 && RoleMilestoneCatalog::Rank(anni.back()->id) == 5, "ForRole last is rank 5");

	// Reward mail: only some milestones grant loot. U+21B5 line-breaks in the body collapse to spaces.
	CHECK(cat.HasReward("Ach_BackUp_01"), "backup milestone grants a reward");
	CHECK(cat.RewardBody("Ach_BackUp_01") ==
		"Congratulations! You have successfully called for backup 101 times. Your rewards are attached.",
		"reward body verbatim, U+21B5 collapsed to space");
	CHECK(!cat.HasReward("15th_Anniversary_Celebrations_01"), "title-only milestone has no reward");

	// Apostrophes/other printable punctuation survive extraction (no literal \u escape).
	CHECK(cat.RewardSubject("NewVDayPrimary_01_2024") == "Someone's Favourite: Rank 1",
		"reward subject apostrophe preserved");
	CHECK(cat.RewardSubject("NewVDayPrimary_01_2024").find("\\u") == std::string::npos, "no literal \\u in subject");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "milestones sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing milestone Find is null");
	CHECK(cat.Title("Nope", "?") == "?", "missing milestone returns default title");
	CHECK(cat.RewardBody("Nope").empty(), "missing milestone body empty");

	// End-to-end via WorldService, incl. the milestone -> player_roles binding actually resolving.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "role milestones world init");
	CHECK(w.role_milestones.Count() == 705, "world loaded role milestones");
	const std::string roleId = RoleMilestoneCatalog::RoleId("15th_Anniversary_Celebrations_01");
	CHECK(w.progression.FindRole(roleId) != nullptr, "milestone binds to a real player_roles row");
}

static void TestHUDMessagesFromRetail() {
	std::cout << "-- HUDMessages (on-screen HUD notifications) from retail --\n";
	// The broad on-screen HUD notifications/prompts/error banners parsed from Content/Data/hud_messages.json
	// (extracted from the retail HUDMessages.INT, mirror of the cooked SDD table "HUDMessage"). Each
	// message has a display_text (with <col:NAME>...</col> colour spans + <Token> placeholders) and a
	// usually-empty chat_text. Distinct from the combat score-feed (HUDCombatMessages).
	HUDMessageCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\hud_messages.json")), "hud_messages.json parses");
	CHECK(cat.Count() == 882, "loaded all 882 HUD messages");

	// display_text is verbatim, markup + tokens preserved (angle brackets survived extraction).
	CHECK(cat.DisplayText("AM_Abandon_Match_Fail") == "<col:HUDMessage_Error>You cannot abandon opposed missions.</col>",
		"error banner colour span verbatim");
	CHECK(cat.DisplayText("AM_AdHocEnemySideDeliverItem") == "<CharacterNameA> has delivered a stolen item.",
		"token banner verbatim");
	CHECK(cat.DisplayText("AM_Abandon_Match_Fail").find("\\u") == std::string::npos, "no literal \\u in display text");

	// StripColor drops the <col:...>/</col> wrappers but keeps the inner text and other tokens.
	CHECK(HUDMessageCatalog::StripColor("<col:HUDMessage_Error>You cannot abandon opposed missions.</col>")
		== "You cannot abandon opposed missions.", "StripColor removes colour wrappers");
	CHECK(cat.PlainDisplayText("AM_Abandon_Match_Fail") == "You cannot abandon opposed missions.",
		"PlainDisplayText strips colour span");
	CHECK(HUDMessageCatalog::StripColor("<CharacterNameA> has <col:X>delivered</col> a stolen item.")
		== "<CharacterNameA> has delivered a stolen item.", "StripColor keeps substitution tokens");

	// Format substitutes a named <Token>; multiple tokens are filled by repeated calls.
	CHECK(cat.FormatDisplay("AM_AdHocEnemySideDeliverItem", "CharacterNameA", "xoified")
		== "xoified has delivered a stolen item.", "FormatDisplay substitutes token");
	std::string two = cat.FormatDisplay("AM_AdHocSideDeliverVehicleDamaged", "CharacterNameA", "xoified");
	two = HUDMessageCatalog::Format(two, "VehicleName", "Jericho");
	CHECK(two == "xoified delivered the Jericho", "two tokens filled by repeated Format");

	// chat_text is populated for some messages.
	CHECK(cat.ChatText("AM_FameYouContactGainLevel") == "<ContactName> Contact Level <ContactLevel>",
		"chat text verbatim where present");

	// Stable file order + missing-id safety.
	CHECK(!cat.messages.empty() && cat.messages[0].order == 0, "messages sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing message Find is null");
	CHECK(cat.DisplayText("Nope", "?") == "?", "missing message returns default display");
	CHECK(cat.PlainDisplayText("Nope").empty(), "missing message plain text empty");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "hud messages world init");
	CHECK(w.hud_messages.Count() == 882, "world loaded HUD messages");
	CHECK(w.hud_messages.PlainDisplayText("AM_Abandon_Match_Fail") == "You cannot abandon opposed missions.",
		"world HUD message resolves + strips colour");
}

static void TestRewardPackagesFromRetail() {
	std::cout << "-- RewardPackages (reward-bundle display text) from retail --\n";
	// The reward-package display descriptions parsed from Content/Data/reward_packages.json (extracted
	// from the retail RewardPackages.INT, mirror of the cooked SDD table "RewardPackages"). Each package
	// has a player-facing description; the payload itself lives in the cooked SDD. Rows where both
	// description fields are empty (e.g. RewardPackages_None) are dropped.
	RewardPackageCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\reward_packages.json")), "reward_packages.json parses");
	CHECK(cat.Count() == 1661, "loaded all 1661 reward packages");

	// Descriptions are verbatim prose: apostrophes and the "APB$" text survive extraction (no \u).
	CHECK(cat.Description("Ach_BackUp_01") == "An Achievement Reward", "achievement reward desc verbatim");
	CHECK(cat.Description("Challenges_Silver_ShotgunCSG_Vintage") == "10-Day Lease: Colby CSG-20 'Country-Gent'",
		"lease desc apostrophe verbatim");
	CHECK(cat.Description("Clothing_Handwraps").find("APB$") != std::string::npos, "APB$ text preserved");
	CHECK(cat.Description("Challenges_Silver_ShotgunCSG_Vintage").find("\\u") == std::string::npos,
		"no literal \\u in description");

	// OutOfSeasonDescription is empty in the current retail build (kept for schema/event rotation).
	CHECK(cat.OutOfSeasonDescription("Ach_BackUp_01").empty(), "out-of-season desc empty in retail");
	CHECK(cat.DescriptionFor("Ach_BackUp_01", true) == "An Achievement Reward",
		"DescriptionFor falls back to regular desc when OOS empty");

	// HasDescription + Category family helper.
	CHECK(cat.HasDescription("Ach_BackUp_01"), "package has description");
	CHECK(!cat.HasDescription("Nope"), "missing package has no description");
	CHECK(RewardPackageCatalog::Category("Challenges_Silver_ShotgunCSG_Vintage") == "Challenges",
		"Category is the family token before first underscore");
	CHECK(RewardPackageCatalog::Category("NoUnderscore") == "NoUnderscore", "Category of id with no underscore is whole id");
	CHECK(!cat.ForCategory("Challenges").empty(), "ForCategory returns the Challenges family");

	// All-empty rows dropped; stable file order + missing-id safety.
	CHECK(cat.Find("None") == nullptr, "empty-description package (None) dropped");
	CHECK(!cat.packages.empty() && cat.packages[0].order == 0, "packages sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing package Find is null");
	CHECK(cat.Description("Nope", "?") == "?", "missing package returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "reward packages world init");
	CHECK(w.reward_packages.Count() == 1661, "world loaded reward packages");
	CHECK(w.reward_packages.Description("Ach_BackUp_01") == "An Achievement Reward",
		"world reward package resolves description");
}

static void TestWeightedRewardsFromRetail() {
	std::cout << "-- WeightedRewards (reward-mail subject/body) from retail --\n";
	// The weighted-reward MAIL text parsed from Content/Data/weighted_rewards.json (extracted from the
	// retail WeightedRewards.INT, mirror of the cooked SDD table "WeightedRewards"). Each reward has a
	// mail subject/body (+ out-of-season variants, empty in retail); U+21B5 paragraph breaks become
	// '\n'. This is the mail-body counterpart to the reward-package display descriptions. Rows where
	// all four fields are empty (the E_*/C_* pool placeholders) are dropped.
	WeightedRewardCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\weighted_rewards.json")), "weighted_rewards.json parses");
	CHECK(cat.Count() == 189, "loaded all 189 weighted-reward mails");

	// Subject/body verbatim; body carries apostrophes and a real newline from the U+21B5 paragraph
	// break; no literal \u survives extraction.
	CHECK(cat.RewardSubject("Bio_Agrotech") == "Biography: Agrotech", "bio reward subject verbatim");
	CHECK(cat.RewardBody("Bio_Agrotech").compare(0, 8, "Agrotech") == 0, "bio reward body starts verbatim");
	CHECK(cat.RewardBody("Bio_Agrotech").find("world's") != std::string::npos, "bio reward body apostrophe preserved");
	CHECK(cat.RewardBody("Bio_Agrotech").find('\n') != std::string::npos, "U+21B5 paragraph break became newline");
	CHECK(cat.RewardSubject("Bio_Agrotech").find("\\u") == std::string::npos, "no literal \\u in subject");
	CHECK(cat.RewardSubject("Legendary_Corsair_JT") == "Congrats", "legendary reward subject verbatim");

	// Out-of-season variants empty in retail; MailSubjectFor falls back to the regular subject.
	CHECK(cat.OutOfSeasonSubject("Bio_Agrotech").empty(), "out-of-season subject empty in retail");
	CHECK(cat.MailSubjectFor("Bio_Agrotech", true) == "Biography: Agrotech",
		"MailSubjectFor falls back to regular subject when OOS empty");
	CHECK(cat.MailBodyFor("Bio_Agrotech", true) == cat.RewardBody("Bio_Agrotech"),
		"MailBodyFor falls back to regular body when OOS empty");

	// HasReward + Category family helper.
	CHECK(cat.HasReward("Bio_Agrotech"), "reward has mail text");
	CHECK(!cat.HasReward("Nope"), "missing reward has no mail text");
	CHECK(WeightedRewardCatalog::Category("Bio_Agrotech") == "Bio", "Category is the family token before first underscore");
	CHECK(!cat.ForCategory("Bio").empty(), "ForCategory returns the Bio family");

	// Stable file order + missing-id safety.
	CHECK(!cat.rewards.empty() && cat.rewards[0].order == 0, "rewards sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing reward Find is null");
	CHECK(cat.RewardSubject("Nope", "?") == "?", "missing reward returns default subject");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "weighted rewards world init");
	CHECK(w.weighted_rewards.Count() == 189, "world loaded weighted rewards");
	CHECK(w.weighted_rewards.RewardSubject("Bio_Agrotech") == "Biography: Agrotech",
		"world weighted reward resolves subject");
}

static void TestRedeemableRewardsFromRetail() {
	std::cout << "-- RedeemableRewards (player-choice confirmation mails) from retail --\n";
	// The redeemable-reward MAIL text parsed from Content/Data/redeemable_rewards.json (extracted from
	// the retail RedeemableRewards.INT, mirror of the cooked SDD table "RedeemableRewards"). A
	// redeemable reward is a player-CHOICE reward (Retail/Leased weapon presets, clothing, titles,
	// skins, vehicles, emotes, bundles); each id has a mail subject and often a body. U+21B5 paragraph
	// breaks become '\n'. Rows where both subject and body are empty are dropped.
	RedeemableRewardCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\redeemable_rewards.json")), "redeemable_rewards.json parses");
	CHECK(cat.Count() == 1471, "loaded all 1471 redeemable-reward mails");

	// Subject/body verbatim; the Retail_Shotgun body carries a real newline from the U+21B5 paragraph
	// break; no literal \u survives extraction.
	CHECK(cat.MailSubject("Retail_Shotgun") == "Colby CSG-20 RT1", "retail preset subject verbatim");
	CHECK(cat.MailBody("Retail_Shotgun").compare(0, 21, "You have chosen the C") == 0, "retail preset body starts verbatim");
	CHECK(cat.MailBody("Retail_Shotgun").find('\n') != std::string::npos, "U+21B5 paragraph break became newline");
	CHECK(cat.MailSubject("Retail_Shotgun").find("\\u") == std::string::npos, "no literal \\u in subject");

	// Many Leased/preset ids are subject-only (empty body).
	CHECK(cat.MailSubject("Leased_Magnum_Preset_FN1") == "ACT44 Golden Marksman PR1", "leased preset subject verbatim");
	CHECK(cat.MailBody("Leased_Magnum_Preset_FN1").empty(), "leased preset body empty in retail");
	CHECK(cat.HasReward("Leased_Magnum_Preset_FN1"), "subject-only reward still counts as having mail text");
	CHECK(!cat.HasBody("Leased_Magnum_Preset_FN1"), "subject-only reward has no body");
	CHECK(cat.HasBody("Retail_Shotgun"), "retail preset has a body");

	// HasReward + Category family helper.
	CHECK(!cat.HasReward("Nope"), "missing reward has no mail text");
	CHECK(RedeemableRewardCatalog::Category("Retail_Shotgun") == "Retail", "Category is the family token before first underscore");
	CHECK(!cat.ForCategory("Weapon").empty(), "ForCategory returns the Weapon family");

	// Stable file order + missing-id safety.
	CHECK(!cat.rewards.empty() && cat.rewards[0].order == 0, "rewards sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing reward Find is null");
	CHECK(cat.MailSubject("Nope", "?") == "?", "missing reward returns default subject");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "redeemable rewards world init");
	CHECK(w.redeemable_rewards.Count() == 1471, "world loaded redeemable rewards");
	CHECK(w.redeemable_rewards.MailSubject("Retail_Shotgun") == "Colby CSG-20 RT1",
		"world redeemable reward resolves subject");
}

static void TestRewardItemTypesFromRetail() {
	std::cout << "-- RewardPackageItemTypes (per-component descriptions + mails) from retail --\n";
	// The reward-package ITEM-TYPE text parsed from Content/Data/reward_item_types.json (extracted from
	// the retail RewardPackageItemTypes.INT, mirror of the cooked SDD table "RewardPackageItemTypes").
	// Each item type is a per-component entry (vehicle customization kits, clothing/outfit/title/
	// weapon-skin components) carrying a rewards-UI description AND (for many) a confirmation mail
	// subject/body. All ids share the "RewardPackage_" prefix so the family is the SECOND token.
	// U+21B5 paragraph breaks become '\n'. Rows where all three fields are empty are dropped.
	RewardItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\reward_item_types.json")), "reward_item_types.json parses");
	CHECK(cat.Count() == 139, "loaded all 139 reward item types");

	// Description verbatim; a desc-only component has no mail.
	CHECK(cat.Description("RewardPackage_Components_Espacio_Kit1") ==
		"This will allow you to use the Seiyo Espacio customization Kit 1. Check out the new scoops on the roof and hood.",
		"component description verbatim");
	CHECK(cat.HasDescription("RewardPackage_Components_Espacio_Kit1"), "component has a description");
	CHECK(!cat.HasMail("RewardPackage_Components_Espacio_Kit1"), "desc-only component has no mail");
	CHECK(cat.Description("RewardPackage_Components_Espacio_Kit1").find("\\u") == std::string::npos, "no literal \\u in description");

	// A component with a confirmation mail: subject carries embedded double-quotes (JSON-escaped then
	// un-escaped verbatim); body has a real newline from the U+21B5 paragraph break.
	CHECK(cat.MailSubject("RewardPackage_Outfit_CSASting_Male") == "Joker Distribution: \"C.S.A. Sting\" Outfit!",
		"mail subject verbatim incl. embedded quotes");
	CHECK(cat.MailBody("RewardPackage_Outfit_CSASting_Male").compare(0, 20, "Thanks for Shopping ") == 0, "mail body starts verbatim");
	CHECK(cat.MailBody("RewardPackage_Outfit_CSASting_Male").find('\n') != std::string::npos, "U+21B5 paragraph break became newline");
	CHECK(cat.HasMail("RewardPackage_Outfit_CSASting_Male"), "outfit component has confirmation mail");

	// Category = second token; ForCategory.
	CHECK(RewardItemTypeCatalog::Category("RewardPackage_Components_Espacio_Kit1") == "Components",
		"Category is the second token (families share the RewardPackage_ prefix)");
	CHECK(RewardItemTypeCatalog::Category("RewardPackage_Outfit_CSASting_Male") == "Outfit", "Category resolves Outfit family");
	CHECK(!cat.ForCategory("Clothing").empty(), "ForCategory returns the Clothing family");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope") == nullptr, "missing item Find is null");
	CHECK(cat.Description("Nope", "?") == "?", "missing item returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "reward item types world init");
	CHECK(w.reward_item_types.Count() == 139, "world loaded reward item types");
	CHECK(w.reward_item_types.Description("RewardPackage_Components_Espacio_Kit1").compare(0, 13, "This will all") == 0,
		"world reward item type resolves description");
}

static void TestInventoryItemTypesFromRetail() {
	std::cout << "-- InventoryItemTypes (master id -> display-name dictionary) from retail --\n";
	// The master inventory ITEM-TYPE dictionary parsed from Content/Data/inventory_item_types.json
	// (extracted from the retail InventoryItemTypes.INT, mirror of the cooked SDD table
	// "InventoryItemTypes"). id -> player-facing DisplayName + CreatorName. Placeholder rows with an
	// empty DisplayName are dropped, so every stored row renders a real name. Backed by an internal
	// id -> index hash map for O(1) Find over ~13k rows.
	InventoryItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\inventory_item_types.json")), "inventory_item_types.json parses");
	CHECK(cat.Count() == 12997, "loaded all 12997 inventory item types");

	// Display names + creator verbatim.
	CHECK(cat.DisplayName("Mod_None") == "Modification None", "Mod_None display name verbatim");
	CHECK(cat.DisplayName("Mod_Vacant") == "Modification Vacant", "Mod_Vacant display name verbatim");
	CHECK(cat.DisplayName("Reward_GenericReward") == "Reward Package", "Reward_GenericReward display name verbatim");
	CHECK(cat.CreatorName("Mod_None") == "Reloaded Productions", "Mod_None creator verbatim");
	CHECK(cat.HasDisplayName("Mod_None"), "Mod_None has a display name");
	CHECK(cat.DisplayName("Mod_None").find("\\u") == std::string::npos, "no literal \\u in display name");

	// Category = first token; ForCategory.
	CHECK(InventoryItemTypeCatalog::Category("Mod_None") == "Mod", "Category is the first token");
	CHECK(InventoryItemTypeCatalog::Category("Reward_GenericReward") == "Reward", "Category resolves Reward family");
	CHECK(!cat.ForCategory("Mod").empty(), "ForCategory returns the Mod family");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Item") == nullptr, "missing item Find is null");
	CHECK(cat.DisplayName("Nope_Missing_Item", "?") == "?", "missing item returns default display name");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "inventory item types world init");
	CHECK(w.inventory_item_types.Count() == 12997, "world loaded inventory item types");
	CHECK(w.inventory_item_types.DisplayName("Reward_GenericReward") == "Reward Package",
		"world inventory item type resolves display name");
}

static void TestUnlockItemTypesFromRetail() {
	std::cout << "-- UnlockItemTypes (unlock-item id -> description) from retail --\n";
	// The unlock item-type descriptions parsed from Content/Data/unlock_item_types.json (extracted from
	// the retail UnlockItemTypes.INT, mirror of the cooked SDD table "UnlockItemTypes"). Unlock items are
	// tokens/entitlements granted through progression/Armas/Joker Store/events (emotes, inventory-capacity
	// unlocks, daily-activity tokens, ...). Of ~8655 ids only 1972 carry a description (empty-description
	// rows dropped); embedded double-quotes round-trip through JSON \".
	UnlockItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\unlock_item_types.json")), "unlock_item_types.json parses");
	CHECK(cat.Count() == 1972, "loaded all 1972 unlock item types");

	// Description verbatim, including embedded double-quotes (JSON-escaped then un-escaped verbatim).
	CHECK(cat.Description("Unlock_Emote_Angry") == "Unlocks the Angry Emote - \"/angry\".",
		"unlock description verbatim incl. embedded quotes");
	CHECK(cat.Description("Unlock_Emote_Bow") == "Unlocks the Bow Emote - \"/bow\".", "second unlock description verbatim");
	CHECK(cat.HasDescription("Unlock_Emote_Angry"), "unlock has a description");
	CHECK(cat.Description("Unlock_Emote_Angry").find("\\u") == std::string::npos, "no literal \\u in description");

	// Category = first token; ForCategory.
	CHECK(UnlockItemTypeCatalog::Category("Unlock_Emote_Angry") == "Unlock", "Category is the first token");
	CHECK(!cat.ForCategory("Unlock").empty(), "ForCategory returns the Unlock family");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Unlock") == nullptr, "missing unlock Find is null");
	CHECK(cat.Description("Nope_Missing_Unlock", "?") == "?", "missing unlock returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "unlock item types world init");
	CHECK(w.unlock_item_types.Count() == 1972, "world loaded unlock item types");
	CHECK(w.unlock_item_types.Description("Unlock_Emote_Bow").compare(0, 15, "Unlocks the Bow") == 0,
		"world unlock item type resolves description");
}

static void TestInventoryInfraCategoriesFromRetail() {
	std::cout << "-- InventoryItemInfraCategories (item category taxonomy) from retail --\n";
	// The inventory-item category taxonomy parsed from Content/Data/inventory_infra_categories.json
	// (extracted from the retail InventoryItemInfraCategories.INT, mirror of the cooked SDD table
	// "InventoryItemInfraCategories"). These are the category buckets the inventory/Armas/store UI uses to
	// GROUP and LABEL the 13k items: each carries a UI header (DisplayName), a plural label (Description)
	// and a singular label (SingularName). The all-empty placeholder "None" id is dropped -> 149 rows.
	InventoryInfraCategoryCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\inventory_infra_categories.json")), "inventory_infra_categories.json parses");
	CHECK(cat.Count() == 149, "loaded all 149 inventory infra categories");

	// Three labels per category, verbatim.
	CHECK(cat.DisplayName("MarketplaceCash") == "Marketplace Cash", "display name verbatim");
	CHECK(cat.DisplayName("ClothingAccessoryClothing") == "Clothing: Accessories (Clothing)", "compound display name verbatim");
	CHECK(cat.Description("ClothingAccessoryClothing") == "Accessories (Clothing)", "plural label verbatim");
	CHECK(cat.SingularName("ClothingAccessoryClothing") == "Accessory (Clothing)", "singular label verbatim");
	CHECK(cat.Has("MarketplaceCash"), "category exists");
	CHECK(cat.DisplayName("ClothingAccessoryClothing").find("\\u") == std::string::npos, "no literal \\u in label");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "placeholder None dropped");
	CHECK(cat.Find("Nope_Missing_Category") == nullptr, "missing category Find is null");
	CHECK(cat.DisplayName("Nope_Missing_Category", "?") == "?", "missing category returns default label");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "inventory infra categories world init");
	CHECK(w.inventory_infra_categories.Count() == 149, "world loaded inventory infra categories");
	CHECK(w.inventory_infra_categories.DisplayName("MarketplaceCash") == "Marketplace Cash",
		"world inventory infra category resolves display name");
}

static void TestWeaponItemTypesFromRetail() {
	std::cout << "-- WeaponItemTypes (weapon id -> rich description) from retail --\n";
	// The weapon description catalog parsed from Content/Data/weapon_item_types.json (extracted from the
	// retail WeaponItemTypes.INT, mirror of the cooked SDD table "WeaponItemTypes"). This is the flavour +
	// role blurb the Armas / weapon-select / inventory UI shows -- the DESCRIPTION leg of the weapon-info
	// triple (weapons_catalog = stats, weapon_display_names = names, this = descriptions). Of ~936 ids only
	// 839 carry a description (empty-description rows dropped).
	WeaponItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\weapon_item_types.json")), "weapon_item_types.json parses");
	CHECK(cat.Count() == 839, "loaded all 839 weapon item types");

	// Description verbatim.
	CHECK(cat.Description("Weapon_SniperRifle_DMR").compare(0, 38, "The Agrotech Designated Marksman Rifle") == 0,
		"weapon description begins verbatim");
	CHECK(cat.Description("Weapon_SniperRifle_DMR_PR1") == "A modified DMR-SD with quicker equip time.",
		"short weapon description verbatim");
	CHECK(cat.HasDescription("Weapon_SniperRifle_DMR"), "weapon has a description");
	CHECK(cat.Description("Weapon_SniperRifle_DMR").find("\\u") == std::string::npos, "no literal \\u in description");

	// Category = first token ("Weapon"); Class = second token (weapon class).
	CHECK(WeaponItemTypeCatalog::Category("Weapon_SniperRifle_DMR") == "Weapon", "Category is the first token");
	CHECK(WeaponItemTypeCatalog::Class("Weapon_SniperRifle_DMR") == "SniperRifle", "Class is the second token");
	CHECK(!cat.ForCategory("Weapon").empty(), "ForCategory returns the Weapon family");
	CHECK(!cat.ForClass("SniperRifle").empty(), "ForClass returns the SniperRifle class");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Weapon") == nullptr, "missing weapon Find is null");
	CHECK(cat.Description("Nope_Missing_Weapon", "?") == "?", "missing weapon returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "weapon item types world init");
	CHECK(w.weapon_item_types.Count() == 839, "world loaded weapon item types");
	CHECK(w.weapon_item_types.Description("Weapon_SniperRifle_DMR_PR1") == "A modified DMR-SD with quicker equip time.",
		"world weapon item type resolves description");
}

static void TestVehicleItemTypesFromRetail() {
	std::cout << "-- VehicleItemTypes (vehicle id -> rich description) from retail --\n";
	// The vehicle description catalog parsed from Content/Data/vehicle_item_types.json (extracted from the
	// retail VehicleItemTypes.INT, mirror of the cooked SDD table "VehicleItemTypes"). This is the flavour +
	// role blurb the Armas / vehicle-select / inventory UI shows -- the DESCRIPTION leg of the vehicle-info
	// pairing (vehicles_catalog = stats, this = descriptions). Of ~580 ids only 569 carry a description
	// (empty-description rows dropped).
	VehicleItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\vehicle_item_types.json")), "vehicle_item_types.json parses");
	CHECK(cat.Count() == 569, "loaded all 569 vehicle item types");

	// Description verbatim.
	CHECK(cat.Description("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker").compare(0, 21, "Jacked up by Ophelia,") == 0,
		"vehicle description begins verbatim");
	CHECK(cat.Description("Vehicle_Ambient_A_ClassicMuscle_Armas_BirthRacing").compare(0, 15, "Back when Birth") == 0,
		"second vehicle description begins verbatim");
	CHECK(cat.HasDescription("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker"), "vehicle has a description");
	CHECK(cat.Description("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker").find("\\u") == std::string::npos, "no literal \\u in description");

	// Category = first token ("Vehicle"); Class = second token (vehicle class: Car/Truck/Van/...).
	CHECK(VehicleItemTypeCatalog::Category("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker") == "Vehicle", "Category is the first token");
	CHECK(VehicleItemTypeCatalog::Class("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker") == "Ambient", "Class is the second token");
	CHECK(!cat.ForCategory("Vehicle").empty(), "ForCategory returns the Vehicle family");
	CHECK(!cat.ForClass("Car").empty(), "ForClass returns the Car class");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Vehicle") == nullptr, "missing vehicle Find is null");
	CHECK(cat.Description("Nope_Missing_Vehicle", "?") == "?", "missing vehicle returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "vehicle item types world init");
	CHECK(w.vehicle_item_types.Count() == 569, "world loaded vehicle item types");
	CHECK(w.vehicle_item_types.Description("Vehicle_Ambient_A_ClassicMuscle_slot_1_Joker").compare(0, 21, "Jacked up by Ophelia,") == 0,
		"world vehicle item type resolves description");
}

static void TestClothingItemTypesFromRetail() {
	std::cout << "-- ClothingItemTypes (clothing id -> rich description) from retail --\n";
	// The clothing description catalog parsed from Content/Data/clothing_item_types.json (extracted from the
	// retail ClothingItemTypes.INT, mirror of the cooked SDD table "ClothingItemTypes"). This is the flavour
	// + role blurb the Armas / character-customization / inventory UI shows -- the DESCRIPTION leg of the
	// clothing-info pairing. Largest of the ItemTypes description tables; of ~2058 ids only 1836 carry a
	// description (empty-description rows dropped).
	ClothingItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\clothing_item_types.json")), "clothing_item_types.json parses");
	CHECK(cat.Count() == 1836, "loaded all 1836 clothing item types");

	// Description verbatim.
	CHECK(cat.Description("Clothing_F_Armpads_Armoured").compare(0, 9, "Arm Pads.") == 0,
		"clothing description begins verbatim");
	CHECK(cat.Description("Clothing_F_Armpads_Impact").compare(0, 16, "Impact Arm Pads.") == 0,
		"second clothing description begins verbatim");
	CHECK(cat.HasDescription("Clothing_F_Armpads_Armoured"), "clothing has a description");
	CHECK(cat.Description("Clothing_F_Armpads_Armoured").find("\\u") == std::string::npos, "no literal \\u in description");

	// Category = first token ("Clothing"); Class = second token (gender-slot "F"/"M" or "Preset").
	CHECK(ClothingItemTypeCatalog::Category("Clothing_F_Armpads_Armoured") == "Clothing", "Category is the first token");
	CHECK(ClothingItemTypeCatalog::Class("Clothing_F_Armpads_Armoured") == "F", "Class is the second token");
	CHECK(!cat.ForCategory("Clothing").empty(), "ForCategory returns the Clothing family");
	CHECK(!cat.ForClass("Preset").empty(), "ForClass returns the Preset class");
	CHECK(!cat.ForClass("M").empty(), "ForClass returns the M (male) class");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Clothing") == nullptr, "missing clothing Find is null");
	CHECK(cat.Description("Nope_Missing_Clothing", "?") == "?", "missing clothing returns default description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "clothing item types world init");
	CHECK(w.clothing_item_types.Count() == 1836, "world loaded clothing item types");
	CHECK(w.clothing_item_types.Description("Clothing_F_Armpads_Armoured").compare(0, 9, "Arm Pads.") == 0,
		"world clothing item type resolves description");
}

static void TestContactsCatalogFromRetail() {
	std::cout << "-- Contacts (authoritative retail name + untruncated bio) from retail --\n";
	// The authoritative retail contact catalog parsed from Content/Data/contacts_catalog.json (extracted
	// from the retail Contacts.INT, mirror of the cooked SDD table "Contacts"). Contacts are the
	// mission-giver NPCs that drive all of APB progression. Unlike the apbdb-scraped contacts_lore.json
	// (bios truncated ~500 chars), these bios are the full untruncated retail text (up to ~7300 chars). Of
	// the 98 retail ids the "None" DNT placeholder is dropped, leaving 97.
	ContactCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\contacts_catalog.json")), "contacts_catalog.json parses");
	CHECK(cat.Count() == 97, "loaded all 97 contacts");

	// Display name + bio verbatim.
	CHECK(cat.Title("Financial_C1") == "Double-B", "contact display name verbatim");
	CHECK(cat.HasDescription("Financial_C1"), "contact has a bio");
	CHECK(cat.Description("Financial_C1").find("\\u") == std::string::npos, "no literal \\u in bio");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");

	// District = first token.
	CHECK(ContactCatalog::District("Financial_C1") == "Financial", "District is the first token");
	CHECK(!cat.ForDistrict("Financial").empty(), "ForDistrict returns the Financial contacts");
	CHECK(!cat.ForDistrict("Waterfront").empty(), "ForDistrict returns the Waterfront contacts");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Contact") == nullptr, "missing contact Find is null");
	CHECK(cat.Title("Nope_Missing_Contact", "?") == "?", "missing contact returns default title");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "contacts catalog world init");
	CHECK(w.contacts_catalog.Count() == 97, "world loaded contacts catalog");
	CHECK(w.contacts_catalog.Title("Financial_C1") == "Double-B", "world contact resolves display name");
}

static void TestTutorialsFromRetail() {
	std::cout << "-- Tutorials (in-game City Guide onboarding text) from retail --\n";
	// The retail tutorial / "City Guide" catalog parsed from Content/Data/tutorials.json (extracted from
	// the retail Tutorials.INT, mirror of the cooked SDD table "Tutorials"). This is the new-player
	// onboarding help shown in the tutorial book UI. 120 topics, each with Title + SubTitle + an HTML Body
	// (kept verbatim so the UE5 UI renders <br>/<b>/... exactly like retail).
	TutorialCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\tutorials.json")), "tutorials.json parses");
	CHECK(cat.Count() == 120, "loaded all 120 tutorials");

	// Title / SubTitle / Body verbatim; the '&' proves \u restore (would be \u0026 otherwise).
	CHECK(cat.Title("Root") == "Welcome to San Paro", "tutorial title verbatim");
	CHECK(cat.SubTitle("Root") == "City Guide", "tutorial subtitle verbatim");
	CHECK(cat.Title("MovementActions") == "Movement & Actions", "tutorial title keeps literal '&'");
	CHECK(cat.HasBody("Root"), "tutorial has a body");
	CHECK(cat.Body("Root").find("<br>") != std::string::npos, "HTML markup kept verbatim in body");
	CHECK(cat.Body("Root").find("\\u") == std::string::npos, "no literal \\u in body");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Tut") == nullptr, "missing tutorial Find is null");
	CHECK(cat.Title("Nope_Missing_Tut", "?") == "?", "missing tutorial returns default title");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "tutorials world init");
	CHECK(w.tutorials.Count() == 120, "world loaded tutorials");
	CHECK(w.tutorials.Title("Root") == "Welcome to San Paro", "world tutorial resolves title");
}

static void TestLoadingTipsFromRetail() {
	std::cout << "-- LoadingMovieTips (loading-screen gameplay hints) from retail --\n";
	// The retail loading-screen tip catalog parsed from Content/Data/loading_tips.json (extracted from the
	// retail LoadingMovieTips.INT, mirror of the cooked SDD table "LoadingMovieTips"). These are the hints
	// shown over the loading movie while a district streams in. 134 tips, single Message per id.
	LoadingTipCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\loading_tips.json")), "loading_tips.json parses");
	CHECK(cat.Count() == 134, "loaded all 134 loading tips");

	// Message verbatim.
	CHECK(cat.Message("E_Move") == "In the Customization Editors, press 1 to select the Move tool.",
		"loading tip message verbatim");
	CHECK(cat.Message("GL_AccountDetails").compare(0, 26, "Never give your account de") == 0,
		"second loading tip begins verbatim");
	CHECK(cat.Message("E_Move").find("\\u") == std::string::npos, "no literal \\u in message");

	// Category = first token; ForCategory groups tips.
	CHECK(LoadingTipCatalog::Category("GL_AccountDetails") == "GL", "Category is the first token");
	CHECK(!cat.ForCategory("GP").empty(), "ForCategory returns the GP tips");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Tip") == nullptr, "missing tip Find is null");
	CHECK(cat.Message("Nope_Missing_Tip", "?") == "?", "missing tip returns default message");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "loading tips world init");
	CHECK(w.loading_tips.Count() == 134, "world loaded loading tips");
	CHECK(w.loading_tips.Message("E_Move").compare(0, 30, "In the Customization Editors, ") == 0,
		"world loading tip resolves message");
}

static void TestSubtitlesFromRetail() {
	std::cout << "-- Subtitles (voice-line captions) from retail --\n";
	// The retail voice-line subtitle catalog parsed from Content/Data/subtitles.json (extracted from the
	// retail Subtitles_MASC.int, UTF-16LE mirror of the cooked SDD subtitle table). These are the on-screen
	// captions spoken by NPCs / contacts / enforcers / criminals during missions, greetings, taunts, radio
	// chatter. 8864 kv lines in the source; 20 empty-value rows dropped, leaving 8844 rows. One id
	// (ORL_Dispatch_Bounty_1) appears twice with DIFFERENT text; merge-by-id keeps the last (order 7007),
	// so the catalog holds 8843 distinct ids. Masc/fem are byte-identical in this build -> one flat catalog.
	SubtitleCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\subtitles.json")), "subtitles.json parses");
	CHECK(cat.Count() == 8843, "loaded all 8843 distinct voice-line subtitles (1 duplicate id merged)");

	// Text verbatim.
	CHECK(cat.Text("CHA_Greeting_Known_1") == "Ok, so you're here. What's up?", "greeting caption verbatim");
	CHECK(cat.Text("WLD_Greeting_1").compare(0, 22, "Looking for a new gun?") == 0,
		"world/vendor caption begins verbatim");
	CHECK(cat.Text("CHA_Greeting_Known_1").find("\\u") == std::string::npos, "no literal \\u in caption");

	// Duplicate id merged to last (order 7007 -> "lowlifes", not order 7006 -> "scumbags").
	CHECK(cat.Text("ORL_Dispatch_Bounty_1").find("lowlifes") != std::string::npos,
		"duplicate id merged to last (lowlifes)");
	CHECK(cat.Text("ORL_Dispatch_Bounty_1").find("scumbags") == std::string::npos,
		"duplicate id first value dropped (scumbags)");

	// Category = first token; ForCategory groups captions.
	CHECK(SubtitleCatalog::Category("CHA_Greeting_Known_1") == "CHA", "Category is the first token");
	CHECK(!cat.ForCategory("CHA").empty(), "ForCategory returns the CHA captions");
	CHECK((int)cat.ForCategory("CHA").size() == 121, "121 CHA greeting captions grouped");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "no None DNT placeholder in this table");
	CHECK(cat.Find("Nope_Missing_Line") == nullptr, "missing caption Find is null");
	CHECK(cat.Text("Nope_Missing_Line", "?") == "?", "missing caption returns default text");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "subtitles world init");
	CHECK(w.subtitles.Count() == 8843, "world loaded subtitles");
	CHECK(w.subtitles.Text("CHA_Greeting_Known_1") == "Ok, so you're here. What's up?",
		"world subtitle resolves caption");
}

static void TestDisplayPointsFromRetail() {
	std::cout << "-- DisplayPoint (collectible/achievement/progression display entries) from retail --\n";
	// The retail display-point catalog parsed from Content/Data/display_points.json (extracted from the
	// retail DisplayPoint.INT, mirror of the cooked SDD table "DisplayPoint"). These are the progression /
	// collectible entries surfaced in the UI (e.g. the "Graffiti - Empire Slipway" spray-tag collectibles
	// found around each district) each with Title/ShortTitle/Description/ObtainedBy. 277 points (the "None"
	// DNT placeholder row is dropped from the 278 retail ids).
	DisplayPointCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\display_points.json")), "display_points.json parses");
	CHECK(cat.Count() == 277, "loaded all 277 display points");

	// Title/ObtainedBy verbatim.
	CHECK(cat.Title("Financial_Display_Graffiti_01") == "Graffiti - Empire Slipway, South",
		"display point title verbatim");
	CHECK(cat.ObtainedBy("Financial_Display_Graffiti_01").compare(0, 28, "Being the first in the distr") == 0,
		"display point obtained-by begins verbatim");
	// RETAIL QUIRK preserved 1:1: many ShortTitle fields carry the literal "DNT - DO NOT TRANSLATE" placeholder.
	CHECK(cat.ShortTitle("Financial_Display_Graffiti_01") == "DNT - DO NOT TRANSLATE",
		"retail ShortTitle DNT placeholder preserved verbatim");
	CHECK(cat.Title("Financial_Display_Graffiti_01").find("\\u") == std::string::npos, "no literal \\u in title");

	// District = first token; ForDistrict groups points.
	CHECK(DisplayPointCatalog::District("Financial_Display_Graffiti_01") == "Financial", "District is the first token");
	CHECK(!cat.ForDistrict("Financial").empty(), "ForDistrict returns the Financial points");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder row dropped");
	CHECK(cat.Find("Nope_Missing_Point") == nullptr, "missing point Find is null");
	CHECK(cat.Title("Nope_Missing_Point", "?") == "?", "missing point returns default title");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "display points world init");
	CHECK(w.display_points.Count() == 277, "world loaded display points");
	CHECK(w.display_points.Title("Financial_Display_Graffiti_01") == "Graffiti - Empire Slipway, South",
		"world display point resolves title");
}

static void TestPopupDialogsFromRetail() {
	std::cout << "-- PopupDialogs (in-game advisory / help popups) from retail --\n";
	// The retail popup-dialog catalog parsed from Content/Data/popup_dialogs.json (extracted from the
	// retail PopupDialogs.INT, mirror of the cooked SDD table "PopupDialogs"). These are the advisory / help
	// popups shown to the player during play (ammo-low advice, arrest rules, vehicle controls, ...). 165
	// popups (the "None" DNT placeholder + empty-body rows dropped), single Body per id.
	PopupDialogCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\popup_dialogs.json")), "popup_dialogs.json parses");
	CHECK(cat.Count() == 165, "loaded all 165 popup dialogs");

	// Body verbatim.
	CHECK(cat.Body("AD_AmmoLow_Chat") == "Ammo: Locate an Ammo Vending Machine to resupply ammunition.",
		"popup body verbatim");
	CHECK(cat.Body("AD_Arrested_Chat").compare(0, 8, "Arrests:") == 0, "second popup body begins verbatim");
	CHECK(cat.Body("AD_AmmoLow_Chat").find("\\u") == std::string::npos, "no literal \\u in body");

	// Category = first token; ForCategory groups popups.
	CHECK(PopupDialogCatalog::Category("AD_AmmoLow_Chat") == "AD", "Category is the first token");
	CHECK(!cat.ForCategory("AD").empty(), "ForCategory returns the AD popups");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Popup") == nullptr, "missing popup Find is null");
	CHECK(cat.Body("Nope_Missing_Popup", "?") == "?", "missing popup returns default body");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "popup dialogs world init");
	CHECK(w.popup_dialogs.Count() == 165, "world loaded popup dialogs");
	CHECK(w.popup_dialogs.Body("AD_AmmoLow_Chat").compare(0, 5, "Ammo:") == 0,
		"world popup dialog resolves body");
}

static void TestHUDMarkerTextFromRetail() {
	std::cout << "-- HUDMarkerVisualText (role-dependent HUD marker labels) from retail --\n";
	// The retail HUD marker text catalog parsed from Content/Data/hud_marker_text.json (extracted from the
	// retail HUDMarkerVisualText.INT, mirror of the cooked SDD table "HUDMarkerVisualText"). Each marker
	// (mission objective / spawn / item) carries up to six role-dependent labels and the HUD picks one based
	// on the local player's relationship to the marker + mission phase. 112 markers with at least one label.
	HUDMarkerTextCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\hud_marker_text.json")), "hud_marker_text.json parses");
	CHECK(cat.Count() == 112, "loaded all 112 HUD marker texts");

	// Role labels verbatim; <Color:...> markup preserved.
	CHECK(cat.Label("Elective_MissionSpawn", MarkerRole::OwnerAttack) == "Active Spawn",
		"owner-attack label verbatim");
	CHECK(cat.Label("Elective_MissionSpawn", MarkerRole::OppositionAttack).compare(0, 8, "<Color:R") == 0,
		"opposition-attack label keeps <Color:...> markup verbatim");
	CHECK(cat.Label("Elective_MissionSpawn", MarkerRole::OwnerAttack).find("\\u") == std::string::npos,
		"no literal \\u in label");

	// Family = first token.
	CHECK(HUDMarkerTextCatalog::Family("Elective_MissionSpawn") == "Elective", "Family is the first token");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Marker") == nullptr, "missing marker Find is null");
	CHECK(cat.Label("Nope_Missing_Marker", MarkerRole::Misc, "?") == "?", "missing marker returns default label");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "hud marker text world init");
	CHECK(w.hud_marker_text.Count() == 112, "world loaded HUD marker texts");
	CHECK(w.hud_marker_text.Label("Elective_MissionSpawn", MarkerRole::OwnerAttack) == "Active Spawn",
		"world HUD marker resolves owner-attack label");
}





static void TestChatMessageCategoriesFromRetail() {
	std::cout << "-- ChatMessageCategories (chat channel slash commands + help) from retail --\n";
	ChatMessageCategoryCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\chat_message_categories.json")),
		"chat_message_categories.json parses");
	CHECK(cat.Count() == 21, "loaded all 21 chat message categories");
	CHECK(cat.PlayerChannelCount() == 10, "10 player-usable slash-command channels");
	CHECK(cat.SlashCommand("Clan") == "/c", "Clan primary slash command");
	CHECK(cat.SecondarySlashCommand("Clan") == "/clan", "Clan secondary slash command");
	CHECK(cat.SlashCommand("District") == "/d", "District primary slash command");
	CHECK(cat.SlashCommand("Whisper") == "/w", "Whisper primary slash command");
	CHECK(cat.SlashCommand("Trade") == "/tr", "Trade primary is /tr (not /trade)");
	CHECK(cat.SecondarySlashCommand("Trade") == "/trade", "Trade secondary is /trade");
	CHECK(cat.SlashCommand("Officer") == "/o", "Officer primary slash command");
	CHECK(cat.SlashCommand("Reply") == "/r", "Reply primary slash command");
	CHECK(cat.SlashCommand("Yell") == "/y", "Yell primary slash command");
	CHECK(cat.SlashCommand("Team") == "/t", "Team primary slash command");
	CHECK(cat.Tag("Clan") == "Clan", "Clan tag verbatim");
	CHECK(cat.Tag("Whisper_Sent") == "To", "Whisper_Sent tag is To");
	CHECK(cat.Tag("Minigame") == "Event", "Minigame tag is Event");
	CHECK(cat.Tag("Challenge") == "Challenge!", "Challenge tag keeps exclamation");
	CHECK(cat.Description("Clan").find("restrict your chat to members of your clan") != std::string::npos,
		"Clan description content");
	CHECK(cat.Description("Officer").find("leader or fellow officers") != std::string::npos,
		"Officer description content");
	CHECK(cat.SyntaxExample("Clan") == "/c message", "Clan syntax example");
	CHECK(cat.SyntaxExample("District") == "/d Hello everyone!", "District syntax example");
	CHECK(!ChatMessageCategoryCatalog::HasSlashCommand(*cat.Find("Broadcast_System")),
		"Broadcast_System has no player slash command");
	CHECK(!ChatMessageCategoryCatalog::HasSlashCommand(*cat.Find("Combat")),
		"Combat has no player slash command");
	CHECK(cat.Tag("Broadcast_System") == "Broadcast", "Broadcast_System tag still valid");
	CHECK(cat.FindBySlashCommand("/c") != nullptr && cat.FindBySlashCommand("/c")->id == "Clan",
		"FindBySlashCommand /c -> Clan");
	CHECK(cat.FindBySlashCommand("/clan") != nullptr && cat.FindBySlashCommand("/clan")->id == "Clan",
		"FindBySlashCommand /clan -> Clan");
	CHECK(cat.FindBySlashCommand("/TR") != nullptr && cat.FindBySlashCommand("/TR")->id == "Trade",
		"FindBySlashCommand /TR -> Trade (case-insensitive)");
	CHECK(cat.FindBySlashCommand("/w") != nullptr && cat.FindBySlashCommand("/w")->id == "Whisper",
		"FindBySlashCommand /w -> Whisper");
	CHECK(cat.FindBySlashCommand("/nonexistent") == nullptr, "FindBySlashCommand unknown is null");
	CHECK(!cat.channels.empty() && cat.channels[0].order == 0, "channels sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None DNT placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Channel") == nullptr, "missing channel Find is null");
	CHECK(cat.Description("Clan").find("\\u") == std::string::npos, "no literal \\u in description");
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "chat message categories world init");
	CHECK(w.chat_message_categories.Count() == 21, "world loaded chat message categories");
	CHECK(w.chat_message_categories.SlashCommand("Trade") == "/tr", "world resolves Trade slash");
}

static void TestEmoteCommandsFromRetail() {
	std::cout << "-- EmoteCommands (emote slash commands + display names) from retail --\n";
	EmoteCommandCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\emote_commands.json")),
		"emote_commands.json parses");
	CHECK(cat.Count() == 50, "loaded all 50 emote commands");
	CHECK(cat.DanceCount() == 15, "15 dance emotes");
	CHECK(cat.SlashCommand("Dance") == "/dance", "Dance slash command");
	CHECK(cat.SlashCommand("Wave") == "/wave", "Wave slash command");
	CHECK(cat.SlashCommand("Bow") == "/bow", "Bow slash command");
	CHECK(cat.SlashCommand("Surrender") == "/surrender", "Surrender slash command");
	CHECK(cat.SlashCommand("Taunt") == "/taunt", "Taunt slash command");
	CHECK(cat.DisplayName("Body Pop") == "Body Pop", "Body Pop display name");
	CHECK(cat.DisplayName("Dance 80s") == "Dance 80s", "Dance 80s display name");
	CHECK(cat.DisplayName("Strike A Pose 1") == "Strike A Pose 1", "Strike A Pose 1 display name");
	CHECK(cat.DisplayName("Thumbs Up") == "Thumbs Up", "Thumbs Up display name");
	CHECK(cat.DisplayName("Coin Toss") == "Coin Toss", "Coin Toss display name");
	CHECK(cat.FindBySlashCommand("/dance") != nullptr && cat.FindBySlashCommand("/dance")->id == "Dance",
		"FindBySlashCommand /dance -> Dance");
	CHECK(cat.FindBySlashCommand("/WAVE") != nullptr && cat.FindBySlashCommand("/WAVE")->id == "Wave",
		"FindBySlashCommand /WAVE -> Wave (case-insensitive)");
	CHECK(cat.FindBySlashCommand("/bodypop") != nullptr && cat.FindBySlashCommand("/bodypop")->id == "Body Pop",
		"FindBySlashCommand /bodypop -> Body Pop");
	CHECK(cat.FindBySlashCommand("/nonexistent") == nullptr, "FindBySlashCommand unknown is null");
	CHECK(EmoteCommandCatalog::IsDance(*cat.Find("Dance")), "Dance is dance");
	CHECK(EmoteCommandCatalog::IsDance(*cat.Find("Dance 80s")), "Dance 80s is dance");
	CHECK(EmoteCommandCatalog::IsDance(*cat.Find("Dance Urban")), "Dance Urban is dance");
	CHECK(!EmoteCommandCatalog::IsDance(*cat.Find("Wave")), "Wave is not dance");
	CHECK(!EmoteCommandCatalog::IsDance(*cat.Find("Bow")), "Bow is not dance");
	CHECK(!cat.emotes.empty() && cat.emotes[0].order == 0, "emotes sorted by file order");
	CHECK(cat.Find("Nope_Missing_Emote") == nullptr, "missing emote Find is null");
	CHECK(cat.DisplayName("Nope_Missing_Emote", "?") == "?", "missing emote returns default name");
	CHECK(cat.DisplayName("Dance 80s").find("\\u") == std::string::npos, "no literal \\u in display name");
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "emote commands world init");
	CHECK(w.emote_commands.Count() == 50, "world loaded emote commands");
	CHECK(w.emote_commands.SlashCommand("Dance") == "/dance", "world resolves Dance slash");
}


static void TestCeremonyMsgsFromRetail() {
	std::cout << "-- CeremonyMsgs (big on-screen celebration popup titles) from retail --\n";
	CeremonyMsgCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\hud_ceremony_msgs.json")),
		"hud_ceremony_msgs.json parses");
	CHECK(cat.Count() == 93, "loaded all 93 ceremony messages");

	// Iconic APB ceremony title anchors.
	CHECK(cat.Title("AM_CombatYouStreakKillOn") == "KILL STREAK!", "kill streak title");
	CHECK(cat.Title("AM_CombatYouStreakArrestOn") == "ARREST STREAK!", "arrest streak title");
	CHECK(cat.Title("AM_CombatYouStreakWinOn") == "WIN STREAK!", "win streak title");
	CHECK(cat.Title("AM_FameMatchEarnedMedal") == "MEDAL EARNED!", "medal earned title");
	CHECK(cat.Title("AM_FameYouContactGainLevel") == "STANDING LEVEL UP!", "contact level up title");
	CHECK(cat.Title("AM_FameYouContactLevelMax") == "CONTACT MAXED!", "contact maxed title");
	CHECK(cat.Title("AM_FameYouRoleGainLevel") == "ROLE LEVEL UP!", "role level up title");
	CHECK(cat.Title("AM_HeatYouNotorietyLevelRaise") == "NOTORIETY LEVEL UP", "notoriety up title");
	CHECK(cat.Title("AM_HeatYouNotorietyLevelDrops") == "NOTORIETY LEVEL DOWN", "notoriety down title");
	CHECK(cat.Title("AM_HeatYouPrestigeLevelRaise") == "PRESTIGE LEVEL UP", "prestige up title");
	CHECK(cat.Title("AM_RatingYouChanged") == "RANK UP!", "rank up title");

	// Bounty ceremony titles.
	CHECK(cat.Title("AM_Heat_BountyClaimed_YouEnfKillCrim") == "BOUNTY CLAIMED", "bounty claimed title");
	CHECK(cat.Title("AM_Heat_BountyClaimed_YouEnfKillEnf") == "BAD SHOT!", "bad shot title");
	CHECK(cat.Title("AM_Heat_Bounty_YouNotorietyLevel5") == "NOTORIETY LEVEL 5!", "notoriety 5 title");
	CHECK(cat.Title("AM_Heat_Bounty_YouPrestigeLevel5") == "PRESTIGE LEVEL 5!", "prestige 5 title");

	// Reward unlock ceremony titles.
	CHECK(cat.Title("AM_RewardUnlockClothing") == "NEW CLOTHING!", "unlock clothing title");
	CHECK(cat.Title("AM_RewardUnlockWeapon") == "NEW WEAPONS!", "unlock weapons title");
	CHECK(cat.Title("AM_RewardUnlockVehicle") == "NEW VEHICLES!", "unlock vehicles title");
	CHECK(cat.Title("AM_RewardUnlockEmotes") == "NEW EMOTES!", "unlock emotes title");
	CHECK(cat.Title("AM_RewardUnlockSymbol") == "NEW SYMBOL!", "unlock symbol title");
	CHECK(cat.Title("AM_RewardUnlockTitle") == "NEW TITLE!", "unlock title title");

	// Category classification.
	CHECK(cat.Category("AM_CombatYouStreakKillOn") == "AM", "AM category");
	CHECK(cat.Category("Minigame_WeaponDrop_Started") == "Minigame", "Minigame category");
	CHECK(cat.Category("ProvingGrounds_Challenge_New") == "ProvingGrounds", "ProvingGrounds category");

	// ForCategory grouping.
	CHECK(cat.ForCategory("AM").size() == 54, "54 AM ceremonies");
	CHECK(cat.ForCategory("Minigame").size() == 15, "15 Minigame ceremonies");
	CHECK(!cat.ForCategory("ProvingGrounds").empty(), "ProvingGrounds ceremonies exist");

	// Placeholder detection.
	CHECK(CeremonyMsgCatalog::HasPlaceholder(*cat.Find("Weapon_Pickup")), "Weapon_Pickup has placeholder");
	CHECK(!CeremonyMsgCatalog::HasPlaceholder(*cat.Find("AM_CombatYouStreakKillOn")), "kill streak no placeholder");

	// Trade ceremonies.
	CHECK(cat.Title("TradeCompleted") == "TRADE SUCCESS", "trade completed title");
	CHECK(cat.Title("TradeCanceled") == "TRADE CANCELLED", "trade cancelled title");

	// Stable file order + missing-id safety.
	CHECK(!cat.msgs.empty() && cat.msgs[0].order == 0, "msgs sorted by file order");
	CHECK(cat.Find("Nope_Missing_Msg") == nullptr, "missing msg Find is null");
	CHECK(cat.Title("Nope_Missing_Msg", "?") == "?", "missing msg returns default title");

	// No literal \u in any field.
	CHECK(cat.Title("AM_CombatYouStreakKillOn").find("\\u") == std::string::npos, "no literal \\u in title");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "ceremony msgs world init");
	CHECK(w.ceremony_msgs.Count() == 93, "world loaded ceremony msgs");
	CHECK(w.ceremony_msgs.Title("AM_CombatYouStreakKillOn") == "KILL STREAK!", "world resolves title");
}

static void TestTaskTargetTypesFromRetail() {
	std::cout << "-- TaskTargetTypes (mission objective display names) from retail --\n";
	TaskTargetTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\task_target_types.json")),
		"task_target_types.json parses");
	CHECK(cat.Count() == 118, "loaded all 118 task target types");

	// Known display-name anchors.
	CHECK(cat.DisplayName("BankMachine") == "ATM", "BankMachine -> ATM");
	CHECK(cat.DisplayName("Graffiti_Default") == "Graffiti Point", "Graffiti_Default -> Graffiti Point");
	CHECK(cat.DisplayName("Alarms") == "Alarm", "Alarms -> Alarm");
	CHECK(cat.DisplayName("BackDoor") == "Back door", "BackDoor -> Back door");
	CHECK(cat.DisplayName("BusShelter") == "Bus shelter", "BusShelter -> Bus shelter");
	CHECK(cat.DisplayName("ElectricalBox") == "Electrical box", "ElectricalBox -> Electrical box");
	CHECK(cat.DisplayName("FireHydrant") == "Fire hydrant", "FireHydrant -> Fire hydrant");
	CHECK(cat.DisplayName("MailBox") == "Mail box", "MailBox -> Mail box");
	CHECK(cat.DisplayName("ParkBench") == "Bench", "ParkBench -> Bench");
	CHECK(cat.DisplayName("PayPhone") == "Pay Phone", "PayPhone -> Pay Phone");

	// NPC target display names.
	CHECK(cat.DisplayName("NPC_Standard_Male") == "Pedestrian", "NPC_Standard_Male -> Pedestrian");
	CHECK(cat.DisplayName("NPC_Pimp_Male") == "Drug Mule", "NPC_Pimp_Male -> Drug Mule");

	// Checkpoint variants all show Checkpoint.
	CHECK(cat.DisplayName("Checkpoint_Race") == "Checkpoint", "Checkpoint_Race -> Checkpoint");
	CHECK(cat.DisplayName("Checkpoint_TerritoryControl") == "Checkpoint", "TerritoryControl -> Checkpoint");

	// Vehicle + dropoff targets.
	CHECK(cat.DisplayName("ParkedVehicleSpawn") == "Vehicle", "ParkedVehicleSpawn -> Vehicle");
	CHECK(cat.DisplayName("OpenWorldDropoff_Criminals") == "Fence", "Criminal dropoff -> Fence");
	CHECK(cat.DisplayName("OpenWorldDropoff_Enforcers") == "Secure Lockup", "Enforcer dropoff -> Secure Lockup");

	// Seasonal / event targets.
	CHECK(cat.DisplayName("NPC_Easter_Bunny") == "Mr Bunny", "Easter Bunny -> Mr Bunny");
	CHECK(cat.DisplayName("NPC_Easter_Chicken") == "Mr Chicken", "Easter Chicken -> Mr Chicken");
	CHECK(cat.DisplayName("PumpkinRed") == "Red Pumpkin", "PumpkinRed -> Red Pumpkin");

	// Riot (newer content) targets.
	CHECK(cat.DisplayName("RiotBombTarget") == "RIOT Device", "RiotBombTarget -> RIOT Device");
	CHECK(cat.DisplayName("RiotVan") == "RIOT Unit", "RiotVan -> RIOT Unit");

	// Sub-classification.
	CHECK(TaskTargetTypeCatalog::IsNPCTarget(*cat.Find("NPC_Standard_Male")), "NPC_Standard_Male is NPC");
	CHECK(!TaskTargetTypeCatalog::IsNPCTarget(*cat.Find("BankMachine")), "BankMachine is not NPC");
	CHECK(TaskTargetTypeCatalog::IsCheckpoint(*cat.Find("Checkpoint_Race")), "Checkpoint_Race is checkpoint");
	CHECK(!TaskTargetTypeCatalog::IsCheckpoint(*cat.Find("BankMachine")), "BankMachine is not checkpoint");

	// ByDisplayName groups variants.
	CHECK(cat.ByDisplayName("Checkpoint").size() > 10, "many Checkpoint variants");
	CHECK(cat.ByDisplayName("Pedestrian").size() >= 6, "multiple Pedestrian NPCs");

	// Distinct display names.
	CHECK(!cat.DistinctDisplayNames().empty(), "has distinct display names");

	// Stable file order + missing-id safety.
	CHECK(!cat.targets.empty() && cat.targets[0].order == 0, "targets sorted by file order");
	CHECK(cat.Find("A_None") == nullptr, "A_None empty name dropped");
	CHECK(cat.Find("Nope_Missing_Target") == nullptr, "missing target Find is null");
	CHECK(cat.DisplayName("Nope_Missing_Target", "?") == "?", "missing target returns default");

	// No literal \u in any field.
	CHECK(cat.DisplayName("BankMachine").find("\\u") == std::string::npos, "no literal \\u in name");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "task target types world init");
	CHECK(w.task_target_types.Count() == 118, "world loaded task target types");
	CHECK(w.task_target_types.DisplayName("BankMachine") == "ATM", "world resolves display name");
}

static void TestRewardPackageItemTypesFromRetail() {
	std::cout << "-- RewardPackageItemTypes (reward-package description/mail text) from retail --\n";
	RewardPackageItemTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\reward_package_item_types.json")),
		"reward_package_item_types.json parses");
	CHECK(cat.Count() == 139, "loaded all 139 reward-package item types");

	// Description anchor (component customization kit).
	CHECK(cat.Description("RewardPackage_Components_Espacio_Kit1").rfind("This will allow you to use the Seiyo Espacio", 0) == 0,
		"Espacio Kit1 description prefix");

	// Mail subject with embedded double-quotes round-trips through JSON \".
	CHECK(cat.MailSubject("RewardPackage_Outfit_CSASting_Male") == "Joker Distribution: \"C.S.A. Sting\" Outfit!",
		"CSA Sting mail subject with embedded quotes");
	CHECK(cat.MailSubject("RewardPackage_Outfit_CSASting_Male").find('"') != std::string::npos,
		"mail subject retains a literal quote");

	// Category = SECOND token (every id is prefixed RewardPackage_).
	CHECK(RewardPackageItemTypeCatalog::Category("RewardPackage_Outfit_CSASting_Male") == "Outfit",
		"category is second token Outfit");
	CHECK(RewardPackageItemTypeCatalog::Category("RewardPackage_Components_Espacio_Kit1") == "Components",
		"category is second token Components");
	CHECK(!cat.ForCategory("Outfit").empty(), "ForCategory Outfit non-empty");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None placeholder dropped");
	CHECK(cat.Find("Nope_Missing_Package") == nullptr, "missing package Find is null");
	CHECK(cat.Description("Nope_Missing_Package", "?") == "?", "missing package returns default");
	CHECK(cat.MailSubject("RewardPackage_Components_Espacio_Kit1", "?") == "?", "absent mail subject returns default");

	// No literal \u in any populated field.
	CHECK(cat.Description("RewardPackage_Components_Espacio_Kit1").find("\\u") == std::string::npos,
		"no literal \\u in description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "reward package item types world init");
	CHECK(w.reward_package_item_types.Count() == 139, "world loaded reward-package item types");
	CHECK(w.reward_package_item_types.MailSubject("RewardPackage_Outfit_CSASting_Male") == "Joker Distribution: \"C.S.A. Sting\" Outfit!",
		"world resolves mail subject");
}

static void TestDailyActivityContactsFromRetail() {
	std::cout << "-- DailyActivityContacts (daily-activity objective text + flavour variants) from retail --\n";
	DailyActivityContactCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\daily_activity_contacts.json")),
		"daily_activity_contacts.json parses");
	CHECK(cat.Count() == 133, "loaded all 133 (id,variant) rows");
	CHECK(cat.ActivityCount() == 85, "85 distinct activities");

	// Variant 1 (unnumbered retail key) anchors.
	CHECK(cat.Title("DestroyEnemyVehicles") == "Casamajor Car-nage", "v1 title");
	CHECK(cat.HUDDescription("DestroyEnemyVehicles") == "Blow up <col: Yellow>3</col> enemy vehicles.",
		"v1 HUD desc keeps <col:> markup verbatim");

	// Variant 2 has different flavour text but the same objective.
	CHECK(cat.VariantCount("DestroyEnemyVehicles") == 2, "DestroyEnemyVehicles has 2 variants");
	CHECK(cat.Title("DestroyEnemyVehicles", 2) == "Vroom Vroom Boom", "v2 title differs");
	CHECK(cat.Find("DestroyEnemyVehicles", 2) != nullptr, "v2 entry exists");
	CHECK(cat.Variants("DestroyEnemyVehicles").size() == 2, "Variants() returns both");
	CHECK(cat.Variants("DestroyEnemyVehicles")[0]->variant == 1
		&& cat.Variants("DestroyEnemyVehicles")[1]->variant == 2, "Variants() variant-sorted");

	// The single activity that ships a 4th variant.
	CHECK(cat.VariantCount("Mission_MVP_Waterfront") == 4, "Mission_MVP_Waterfront has 4 variants");
	CHECK(cat.Title("Mission_MVP_Waterfront", 4) == "Tiger Territory", "v4 title");

	// LongDescription present + apostrophes round-trip.
	CHECK(cat.LongDescription("DestroyEnemyVehicles").find('\'') != std::string::npos,
		"long description keeps apostrophe");

	// Stable file order + missing / out-of-range safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None placeholder dropped");
	CHECK(cat.Find("DestroyEnemyVehicles", 9) == nullptr, "absent variant Find is null");
	CHECK(cat.Find("Nope_Missing_Activity") == nullptr, "missing activity Find is null");
	CHECK(cat.Title("Nope_Missing_Activity", 1, "?") == "?", "missing activity returns default");
	CHECK(cat.VariantCount("Nope_Missing_Activity") == 0, "missing activity has 0 variants");

	// No literal \u in any populated field.
	CHECK(cat.HUDDescription("DestroyEnemyVehicles").find("\\u") == std::string::npos,
		"no literal \\u in HUD description");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "daily activity contacts world init");
	CHECK(w.daily_activity_contacts.Count() == 133, "world loaded daily activity rows");
	CHECK(w.daily_activity_contacts.ActivityCount() == 85, "world sees 85 activities");
	CHECK(w.daily_activity_contacts.Title("DestroyEnemyVehicles", 2) == "Vroom Vroom Boom",
		"world resolves variant title");
}

static void TestTaskOperationUIProfilesFromRetail() {
	std::cout << "-- TaskOperationUIProfile (per-tracked-value HUD labels per mission-operation) from retail --\n";
	TaskOperationUIProfileCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\task_operation_ui_profiles.json")),
		"task_operation_ui_profiles.json parses");
	CHECK(cat.Count() == 178, "loaded all 178 non-empty profiles");

	// Slot-0 primary labels Ã¢â‚¬â€ ids share the TaskOperation id space 1:1.
	CHECK(cat.PrimaryDescription("AntiGraffiti") == "Cover Graffiti:", "AntiGraffiti primary label");
	CHECK(cat.PrimaryDescription("ArmedGuard") == "Guard Targets:", "ArmedGuard primary label");
	CHECK(cat.TrackedValueDescription("AntiGraffitiOpposition", 0) == "Defend Graffiti:",
		"opposition primary label via slot index");

	// A multi-counter operation fills more than slot 0.
	CHECK(cat.TrackedValueDescription("BombDisposal", 0) == "Bombs Armed:", "BombDisposal slot 0");
	CHECK(cat.TrackedValueDescription("BombDisposal", 1) == "Bombs Disarmed:", "BombDisposal slot 1");
	CHECK(cat.TrackedValueCount("BombDisposal") == 2, "BombDisposal has 2 tracked values");
	CHECK(cat.Descriptions("BombDisposal").size() == 2, "Descriptions() returns both slots");
	CHECK(cat.Descriptions("BombDisposal")[0] == "Bombs Armed:", "Descriptions() slot-ordered");

	// Single-slot operation reports one tracked value.
	CHECK(cat.TrackedValueCount("AntiGraffiti") == 1, "single-slot op has 1 tracked value");

	// Stable file order + placeholder / out-of-range safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("None") == nullptr, "None placeholder dropped");
	CHECK(cat.Find("Simple") == nullptr, "all-empty Simple placeholder dropped");
	CHECK(cat.TrackedValueDescription("AntiGraffiti", 4, "?") == "?", "out-of-range slot returns default");
	CHECK(cat.TrackedValueDescription("AntiGraffiti", -1, "?") == "?", "negative slot returns default");
	CHECK(cat.Find("Nope_Missing_Op") == nullptr, "missing op Find is null");
	CHECK(cat.PrimaryDescription("Nope_Missing_Op", "?") == "?", "missing op returns default");
	CHECK(cat.TrackedValueCount("Nope_Missing_Op") == 0, "missing op has 0 tracked values");

	// No literal \u in any populated field.
	CHECK(cat.PrimaryDescription("AntiGraffiti").find("\\u") == std::string::npos,
		"no literal \\u in label");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "task operation ui profiles world init");
	CHECK(w.task_operation_ui_profiles.Count() == 178, "world loaded ui profiles");
	CHECK(w.task_operation_ui_profiles.PrimaryDescription("ArmedGuard") == "Guard Targets:",
		"world resolves primary tracked-value label");
}

static void TestTooltipsFromRetail() {
	std::cout << "-- Tooltips (frontend/menu UI hover tooltips, section-scoped Scene@Widget) from retail --\n";
	TooltipCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\tooltips.json")),
		"tooltips.json parses");
	CHECK(cat.Count() == 409, "loaded all 409 (scene,widget) rows");
	CHECK(cat.SceneCount() == 53, "53 distinct scenes with tooltips");

	// (scene, widget) anchors.
	CHECK(cat.TooltipFor("Login_Scene", "UILabelButton_TOS") == "Create a new APB Account.",
		"Login_Scene TOS tooltip");
	CHECK(cat.TooltipFor("Lobby_Scene", "UILabelButton_Logout") == "Log out of this account.",
		"Lobby_Scene logout tooltip");
	CHECK(cat.Find("Login_Scene", "UILabelButton_TOS") != nullptr, "Find resolves the pair");

	// Same widget id in a different scene is a distinct entry (scene-scoped lookup).
	CHECK(cat.Find("Login_Scene", "UILabelButton_Logout") == nullptr,
		"widget is scoped to its scene, not global");

	// ForScene returns the scene's tooltips in file order.
	CHECK(cat.ForScene("Login_Scene").size() >= 1, "ForScene returns Login tooltips");
	CHECK(cat.SceneTooltipCount("SymbolEditor_0001") == 57, "SymbolEditor scene tooltip count");

	// Stable file order + missing safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Nope_Missing_Scene", "Whatever") == nullptr, "missing scene Find is null");
	CHECK(cat.TooltipFor("Nope_Missing_Scene", "Whatever", "?") == "?", "missing pair returns default");
	CHECK(cat.SceneTooltipCount("Nope_Missing_Scene") == 0, "missing scene has 0 tooltips");

	// Apostrophes round-trip; no literal \u.
	CHECK(cat.TooltipFor("Login_Scene", "UILabelButton_TOS").find("\\u") == std::string::npos,
		"no literal \\u in tooltip text");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "tooltips world init");
	CHECK(w.tooltips.Count() == 409, "world loaded tooltip rows");
	CHECK(w.tooltips.TooltipFor("Lobby_Scene", "UILabelButton_Logout") == "Log out of this account.",
		"world resolves a scene tooltip");
}


static void TestEquipmentTypesFromRetail() {
	std::cout << "-- EquipmentTypes (mission toolkit item descriptions) from retail --\n";
	EquipmentTypeCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\equipment_types.json")),
		"equipment_types.json parses");
	CHECK(cat.Count() == 60, "loaded all 60 equipment types");
	CHECK(cat.BaseCount() == 15, "15 equipment families");

	// Battering Ram family â€” 4 tiers.
	CHECK(cat.Description("Equipment_BatteringRam") == "A Battering Ram used to breach doors.", "battering ram base desc");
	CHECK(cat.Description("Equipment_BatteringRam_Mk2") == "An improved Battering Ram used to breach doors quickly.", "battering ram mk2 desc");
	CHECK(cat.Description("Equipment_BatteringRam_Mk3") == "A significantly improved Battering Ram used to breach doors very quickly.", "battering ram mk3 desc");
	CHECK(cat.Description("Equipment_BatteringRam_Mk4") == "A high-efficiency Battering Ram used to breach doors as quickly as possible.", "battering ram mk4 desc");

	// Mark tiers parsed correctly.
	CHECK(cat.Find("Equipment_BatteringRam")->mk == 0, "base mk=0");
	CHECK(cat.Find("Equipment_BatteringRam_Mk2")->mk == 2, "mk2=2");
	CHECK(cat.Find("Equipment_BatteringRam_Mk3")->mk == 3, "mk3=3");
	CHECK(cat.Find("Equipment_BatteringRam_Mk4")->mk == 4, "mk4=4");
	CHECK(cat.Find("Equipment_BatteringRam_Mk2")->base == "Equipment_BatteringRam", "mk2 base correct");

	// ForBase grouping.
	CHECK(cat.ForBase("Equipment_BatteringRam").size() == 4, "battering ram has 4 tiers");
	CHECK(cat.ForBase("Equipment_Handcuffs").size() == 4, "handcuffs has 4 tiers");

	// FindBase resolves from upgraded id.
	CHECK(cat.FindBase("Equipment_BatteringRam_Mk3") != nullptr, "find base from mk3");
	CHECK(cat.FindBase("Equipment_BatteringRam_Mk3")->mk == 0, "base entry has mk=0");
	CHECK(cat.FindBase("Equipment_BatteringRam") != nullptr, "find base from base id");

	// IsUpgrade classification.
	CHECK(!EquipmentTypeCatalog::IsUpgrade(*cat.Find("Equipment_BatteringRam")), "base is not upgrade");
	CHECK(EquipmentTypeCatalog::IsUpgrade(*cat.Find("Equipment_BatteringRam_Mk2")), "mk2 is upgrade");
	CHECK(EquipmentTypeCatalog::IsUpgrade(*cat.Find("Equipment_BatteringRam_Mk4")), "mk4 is upgrade");

	// Handcuffs (Enforcer arrest tool).
	CHECK(cat.Description("Equipment_Handcuffs") == "Standard-issue handcuffs for arresting Criminals.", "handcuffs base desc");
	CHECK(cat.Description("Equipment_Handcuffs_Mk4") == "FBI-issue handcuffs for arresting Criminals as quickly as possible.", "handcuffs mk4 desc");

	// Brass knuckles (Criminal mug tool).
	CHECK(cat.Description("Equipment_BrassKnuckles") == "Light brass knuckles, for mugging the citizens of San Paro.", "brass knuckles base desc");
	CHECK(cat.Description("Equipment_BrassKnuckles_Mk4") == "Brutal brass knuckles, for mugging the citizens of San Paro as quickly as possible.", "brass knuckles mk4 desc");

	// SprayCan (graffiti tool).
	CHECK(cat.Description("Equipment_SprayCan") == "A can of spray paint for putting up graffiti.", "spraycan base desc");
	CHECK(cat.Description("Equipment_SprayCan_Mk4") == "A top-quality can of spray paint for putting up graffiti as quickly as possible.", "spraycan mk4 desc");

	// Bases list.
	CHECK(cat.Bases().size() == 15, "15 distinct bases");

	// Stable file order + missing-id safety.
	CHECK(!cat.items.empty() && cat.items[0].order == 0, "items sorted by file order");
	CHECK(cat.Find("Equipment_None") == nullptr, "DNT None row dropped");
	CHECK(cat.Find("Equipment_AmmoCarrier") == nullptr, "DNT AmmoCarrier row dropped");
	CHECK(cat.Find("Nope_Missing_Equip") == nullptr, "missing equip Find is null");
	CHECK(cat.Description("Nope_Missing_Equip", "?") == "?", "missing equip returns default");

	// No literal \u in any field.
	CHECK(cat.Description("Equipment_BatteringRam").find("\\u") == std::string::npos, "no literal \\u in desc");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "equipment types world init");
	CHECK(w.equipment_types.Count() == 60, "world loaded equipment types");
	CHECK(w.equipment_types.Description("Equipment_BatteringRam") == "A Battering Ram used to breach doors.", "world resolves desc");
	CHECK(w.equipment_types.BaseCount() == 15, "world has 15 families");
}


static void TestGameplayObjectsFromRetail() {
	std::cout << "-- GameplayObjects (context-sensitive HUD interaction labels) from retail --\n";
	GameplayObjectCatalog cat;
	CHECK(cat.LoadFromJsonFile(DataDir() + std::string("\\gameplay_objects.json")),
		"gameplay_objects.json parses");
	CHECK(cat.Count() == 44, "loaded all 44 gameplay objects");

	// Prop labels.
	CHECK(cat.Description("Prop_Bench") == "Bench", "Prop_Bench -> Bench");
	CHECK(cat.Description("Prop_ElectricalBox") == "Electrical Box", "Prop_ElectricalBox");
	CHECK(cat.Description("Prop_FireHydrant") == "Fire Hydrant", "Prop_FireHydrant");
	CHECK(cat.Description("Prop_MailBox") == "MailBox", "Prop_MailBox");
	CHECK(cat.Description("Prop_ParkingMeter") == "Parking Meter", "Prop_ParkingMeter");
	CHECK(cat.Description("Prop_PayPhone") == "Payphone", "Prop_PayPhone");
	CHECK(cat.Description("Prop_TrashCan") == "Trash Can", "Prop_TrashCan");
	CHECK(cat.Description("Prop_VendingMachine") == "Vending Machine", "Prop_VendingMachine");
	CHECK(cat.Description("Prop_NewsStand") == "News Stand", "Prop_NewsStand");
	CHECK(cat.Description("Prop_ShopFront") == "Shop Front", "Prop_ShopFront");

	// Halloween seasonal props.
	CHECK(cat.Description("Prop_Halloween_Pumpkin") == "Pumpkin", "Halloween Pumpkin");
	CHECK(cat.Description("Prop_Halloween_Pumpkin_Purple") == "Purple Pumpkin", "Purple Pumpkin");
	CHECK(cat.Description("Prop_Halloween_Pumpkin_Red") == "Red Pumpkin", "Red Pumpkin");

	// Player character labels.
	CHECK(cat.Description("PlayerCharacter_Criminal") == "Criminal", "Criminal label");
	CHECK(cat.Description("PlayerCharacter_Enforcer") == "Enforcer", "Enforcer label");

	// Pedestrian.
	CHECK(cat.Description("Pedestrian_LivingCity") == "Pedestrian", "Pedestrian label");

	// Ambient vehicle labels.
	CHECK(cat.Description("Vehicle_Ambient_Taxi") == "Taxi", "Taxi label");
	CHECK(cat.Description("Vehicle_Ambient_Truck") == "Truck", "Truck label");
	CHECK(cat.Description("Vehicle_Ambient_Van") == "Van", "Van label");
	CHECK(cat.Description("Vehicle_Ambient_Car_Cheap") == "Cheap Car", "Cheap Car");
	CHECK(cat.Description("Vehicle_Ambient_Car_Luxury") == "Luxury Car", "Luxury Car");
	CHECK(cat.Description("Vehicle_Ambient_SUV_MidRange") == "Mid-Range SUV", "Mid-Range SUV");
	CHECK(cat.Description("Vehicle_Ambient_ArmouredVan") == "Armoured Van", "Armoured Van");

	// Player vehicles.
	CHECK(cat.Description("Vehicle_Player_Basic") == "Basic Player Vehicle", "Basic player vehicle");
	CHECK(cat.Description("Vehicle_Player_Advanced") == "Advanced Player Vehicle", "Advanced player vehicle");

	// Task items.
	CHECK(cat.Description("TaskItem_OpenWorld_CashPool") == "Cash Pool", "Cash Pool task item");
	CHECK(cat.Description("TaskItem_OpenWorld_Small") == "Small Open World", "Small task item");
	CHECK(cat.Description("TaskItem_OpenWorld_Large") == "Large Open World", "Large task item");
	CHECK(cat.Description("TaskItem_OpenWorld_TV") == "TV", "TV task item");

	// Display points + checkpoints + graffiti.
	CHECK(cat.Description("DisplayPoint_Statue") == "Statue", "Statue display point");
	CHECK(cat.Description("Checkpoint_Misc") == "Checkpoint", "Checkpoint label");
	CHECK(cat.Description("Graffiti_TaskTarget") == "Graffiti", "Graffiti label");

	// Category classification.
	CHECK(cat.Category("Prop_Bench") == "Prop", "Prop category");
	CHECK(cat.Category("Vehicle_Ambient_Taxi") == "Vehicle", "Vehicle category");
	CHECK(cat.Category("PlayerCharacter_Criminal") == "PlayerCharacter", "PlayerCharacter category");

	// ForCategory grouping.
	CHECK(cat.ForCategory("Prop").size() == 16, "16 props");
	CHECK(cat.ForCategory("Vehicle").size() == 13, "13 vehicles");
	CHECK(cat.ForCategory("TaskItem").size() == 7, "7 task items");
	CHECK(cat.ForCategory("DisplayPoint").size() == 3, "3 display points");
	CHECK(cat.ForCategory("PlayerCharacter").size() == 2, "2 player chars");

	// Distinct categories.
	CHECK(cat.Categories().size() == 8, "8 distinct categories");

	// Sub-classification.
	CHECK(GameplayObjectCatalog::IsProp(*cat.Find("Prop_Bench")), "Prop_Bench is prop");
	CHECK(!GameplayObjectCatalog::IsProp(*cat.Find("Vehicle_Ambient_Taxi")), "Taxi is not prop");
	CHECK(GameplayObjectCatalog::IsVehicle(*cat.Find("Vehicle_Ambient_Taxi")), "Taxi is vehicle");
	CHECK(GameplayObjectCatalog::IsAmbientVehicle(*cat.Find("Vehicle_Ambient_Taxi")), "Taxi is ambient");
	CHECK(!GameplayObjectCatalog::IsAmbientVehicle(*cat.Find("Vehicle_Player_Basic")), "Player vehicle is not ambient");
	CHECK(GameplayObjectCatalog::IsPlayerCharacter(*cat.Find("PlayerCharacter_Criminal")), "Criminal is player char");

	// Empty-desc rows dropped (WeaponPickup_Box/Dropped/Mission have empty descriptions).
	CHECK(cat.Find("WeaponPickup_Box") == nullptr, "empty desc WeaponPickup_Box dropped");
	CHECK(cat.Find("WeaponPickup_Mission") == nullptr, "empty desc WeaponPickup_Mission dropped");
	// DNT row dropped.
	CHECK(cat.Find("Pedestrian_LivingCity_MugTarget") == nullptr, "DNT MugTarget dropped");

	// Stable file order + missing-id safety.
	CHECK(!cat.objects.empty() && cat.objects[0].order == 0, "objects sorted by file order");
	CHECK(cat.Find("Nope_Missing_Obj") == nullptr, "missing obj Find is null");
	CHECK(cat.Description("Nope_Missing_Obj", "?") == "?", "missing obj returns default");

	// No literal \u in any field.
	CHECK(cat.Description("Prop_Bench").find("\\u") == std::string::npos, "no literal \\u in desc");

	// End-to-end via WorldService.
	WorldService w;
	CHECK(w.InitFromDataDir(DataDir()), "gameplay objects world init");
	CHECK(w.gameplay_objects.Count() == 44, "world loaded gameplay objects");
	CHECK(w.gameplay_objects.Description("Prop_Bench") == "Bench", "world resolves desc");
	CHECK(w.gameplay_objects.Categories().size() == 8, "world has 8 categories");
}

void TestCaptureDomainSnapshotMissionFields() {
	// RED→GREEN: verifies that CaptureSnapshot exposes all 7 M11 mission race fields.
	// Before W1-A these fields don't exist on DomainSnapshot → compilation fails (RED).
	WorldService ws;
	CHECK(ws.InitFromDataDir(DataDir()), "mission_fields init");
	CHECK(ws.RegisterAccount("snap_user", "pw"), "register snap_user");
	CHECK(ws.LoginAccount("snap_user", "pw"), "login snap_user");
	ws.CreateCharacter("snap_char", Faction::Enforcer);

	apb::DomainSnapshot snap = ws.CaptureSnapshot();
	// All 7 fields must be accessible (compile-time + value-initialized defaults)
	CHECK(snap.mission_stage_progress >= 0.0,         "snap.mission_stage_progress exists");
	CHECK(snap.mission_opp_stage_progress >= 0.0,     "snap.mission_opp_stage_progress exists");
	CHECK(!snap.mission_opposition_contesting,         "snap.mission_opposition_contesting exists");
	CHECK(!snap.mission_opposition_won,                "snap.mission_opposition_won exists");
	CHECK(!snap.mission_timed_out,                     "snap.mission_timed_out exists");
	CHECK(snap.mission_stage_time_limit_sec >= 0.0,   "snap.mission_stage_time_limit_sec exists");
	// mission_stage_deadline_server_sec is the absolute server-clock deadline sourced from
	// MissionRun::current_stage_deadline_sec (armed by TickMission); 0 when no timer is armed.
	CHECK(snap.mission_stage_deadline_server_sec == 0.0, "snap.mission_stage_deadline_server_sec exists (unarmed=0)");
	std::cout << "MISSION_FIELDS_SNAPSHOT all 7 domain fields accessible\n";
}

int main() {
	std::cout << "APB Domain tests (shipped WorldService)\n";
	TestLoginSuccessAndFail();
	TestLoginWorldDistrict();
	TestEconomy();
	TestDistrictLoop();
	TestDomainSnapshotParity();
	TestOppositionRace();
	TestMissionStageTimeout();
	TestContactLevelsFromRetail();
	TestPlayerRolesFromRetail();
	TestMissionTemplatesFromRetail();
	TestTaskObjectivesFromRetail();
	TestTaskOperationsFromRetail();
	TestMissionResultReasonsFromRetail();
	TestThreatRatingsFromRetail();
	TestFactionInfoFromRetail();
	TestHeatLevelDescriptionsFromRetail();
	TestOrganisationsFromRetail();
	TestMedalsFromRetail();
	TestStreetNamesFromRetail();
	TestAmmoCategoriesFromRetail();
	TestScoreboardDescriptionsFromRetail();
	TestHUDCombatMessagesFromRetail();
	TestModifierEffectsFromRetail();
	TestModifierItemTypesFromRetail();
	TestRoleMilestonesFromRetail();
	TestHUDMessagesFromRetail();
	TestRewardPackagesFromRetail();
	TestWeightedRewardsFromRetail();
	TestRedeemableRewardsFromRetail();
	TestRewardItemTypesFromRetail();
	TestInventoryItemTypesFromRetail();
	TestUnlockItemTypesFromRetail();
	TestInventoryInfraCategoriesFromRetail();
	TestWeaponItemTypesFromRetail();
	TestVehicleItemTypesFromRetail();
	TestClothingItemTypesFromRetail();
	TestContactsCatalogFromRetail();
	TestTutorialsFromRetail();
	TestLoadingTipsFromRetail();
	TestSubtitlesFromRetail();
	TestDisplayPointsFromRetail();
	TestPopupDialogsFromRetail();
	TestHUDMarkerTextFromRetail();
	TestChatMessageCategoriesFromRetail();
	TestEmoteCommandsFromRetail();
	TestCeremonyMsgsFromRetail();
	TestTaskTargetTypesFromRetail();
	TestEquipmentTypesFromRetail();
	TestGameplayObjectsFromRetail();
	TestRewardPackageItemTypesFromRetail();
	TestDailyActivityContactsFromRetail();
	TestTaskOperationUIProfilesFromRetail();
	TestTooltipsFromRetail();
	TestCaptureDomainSnapshotMissionFields();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
