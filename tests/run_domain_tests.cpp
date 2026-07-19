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
	CHECK(sellerProf.cash == 1000, "seller received cash");

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
	int safety=0;
	while (w.mission->status == MissionStatus::Active && safety++ < 80) {
		w.AdvanceMission(1.0);
	}
	CHECK(w.mission->status == MissionStatus::Completed, "mission completed multi-stage");
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
}

int main() {
	std::cout << "APB Domain tests (shipped WorldService)\n";
	TestLoginSuccessAndFail();
	TestLoginWorldDistrict();
	TestEconomy();
	TestDistrictLoop();
	TestDomainSnapshotParity();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
