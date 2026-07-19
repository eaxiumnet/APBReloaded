#include "APBWorldService.h"
#include <fstream>
#include <sstream>
namespace apb {
namespace {
std::string ReadFile(const std::string& path) {
	std::ifstream in(path, std::ios::binary); if (!in) return {};
	std::ostringstream ss; ss << in.rdbuf(); return ss.str();
}
bool LoadStreamPlan(const std::string& path, DistrictStreamPlan& plan) {
	std::string text = ReadFile(path); if (text.empty()) return false;
	plan.active_district_id = JsonGetString(text, "active_district_id", "Financial");
	plan.chunk_size = JsonGetNumber(text, "chunk_size", 64);
	plan.stream_radius_chunks = (int32_t)JsonGetNumber(text, "stream_radius_chunks", 2);
	plan.chunks.clear();
	// Prefer the "chunks" array body so root object fields are not mistaken for chunks.
	std::string body = text;
	size_t chunksKey = text.find("\"chunks\"");
	if (chunksKey != std::string::npos) {
		size_t lb = text.find('[', chunksKey);
		size_t rb = text.rfind(']');
		if (lb != std::string::npos && rb != std::string::npos && rb > lb)
			body = text.substr(lb, rb - lb + 1);
	}
	for (const auto& obj : JsonSplitObjects(body)) {
		StreamChunk c;
		c.id = JsonGetString(obj, "id");
		if (c.id.empty()) continue;
		// skip accidental root capture
		if (c.id == plan.active_district_id && obj.find("\"origin\"") == std::string::npos) continue;
		c.size = JsonGetNumber(obj, "size", plan.chunk_size);
		c.building_density = JsonGetNumber(obj, "building_density", 0.5);
		size_t op = obj.find("\"origin\"");
		if (op != std::string::npos) {
			size_t obr = obj.find('[', op);
			if (obr != std::string::npos) {
				c.origin_x = strtod(obj.c_str() + obr + 1, nullptr);
				size_t comma = obj.find(',', obr);
				if (comma != std::string::npos) c.origin_y = strtod(obj.c_str() + comma + 1, nullptr);
			}
		}
		if (obj.find("Praetorian") != std::string::npos) c.landmarks.push_back("Praetorian HQ approach");
		if (c.id.find("spawn") != std::string::npos) c.landmarks.push_back("Faction spawn");
		if (c.id.find("mission") != std::string::npos) c.landmarks.push_back("Objective");
		plan.chunks.push_back(c);
	}
	return !plan.chunks.empty();
}
}

bool WorldService::InitFromDataDir(const std::string& dir) {
	bool ok = catalog.LoadAllFromDir(dir);
	ok = mission_scripts.LoadFromJsonFile(dir + "/missions.json") || ok;
	ok = mission_scripts.LoadFromJsonFile(dir + "/mission_scripts.json") || ok;
	catalog.LoadClothingJson(dir + "/clothing.json");
	models.LoadFromFile(dir + "/model_reference_catalog.json");
	LoadStreamPlan(dir + "/district_stream.json", stream_plan);
	// apbdb heat / notoriety / prestige thresholds (Content/Data/threat_table.json)
	const bool threatOk = threat.LoadFromThreatTableJson(dir + "/threat_table.json");
	armas.catalog = &catalog; auction.catalog = &catalog; customization.catalog = &catalog;
	world_dir.EnsureDefault();
	phase = SessionPhase::Boot;
	log.push_back("INIT items=" + std::to_string(catalog.items.size())
		+ " districts=" + std::to_string(catalog.districts.size())
		+ " scripts=" + std::to_string(mission_scripts.scripts.size())
		+ " stream_chunks=" + std::to_string(stream_plan.chunks.size())
		+ " models=" + std::to_string(models.VehicleCount() + models.CharacterCount())
		+ " threat_table=" + (threatOk ? "1" : "0"));
	return ok;
}
bool WorldService::RegisterAccount(const std::string& user, const std::string& pass) {
	bool r = login.Register(user, pass);
	if (r) {
		auto it = login.accounts.find(user);
		if (it != login.accounts.end() && it->second.created_utc.empty())
			it->second.created_utc = NowUtcIso();
		PersistAccounts();
	}
	log.push_back(r ? ("REGISTER " + user) : "REGISTER_FAIL");
	return r;
}
bool WorldService::LoginAccount(const std::string& user, const std::string& pass) {
	bool r = login.Login(user, pass);
	if (r) {
		phase = SessionPhase::LoggedIn;
		const std::string now = NowUtcIso();
		auto it = login.accounts.find(user);
		if (it != login.accounts.end()) it->second.last_login_utc = now;
		if (login.session) login.session->last_login_utc = now;
		PersistAccounts();
		TryLoadPersistedCharacter();
	}
	log.push_back(r ? ("LOGIN " + user) : "LOGIN_FAIL");
	return r;
}
bool WorldService::EnterWorld(const std::string& world_id) {
	if (!login.IsLoggedIn() && !character) {
		// allow tests without explicit login if character already exists
	}
	int32_t pop = 0;
	if (!world_dir.EnterWorld(world_id, pop)) return false;
	active_world_id = world_id;
	phase = SessionPhase::WorldLobby;
	log.push_back("ENTER_WORLD " + world_id + " pop=" + std::to_string(pop));
	return true;
}
bool WorldService::CreateCharacter(const std::string& name, Faction faction) {
	if (name.empty()) return false;
	CharacterProfile p; p.name = name; p.faction = faction; p.cash = 10000; p.g1c = 5000; character = p;
	threat.faction = faction; threat.points = 0; inventory.slots.clear();
	appearance = CustomizationService::DefaultForFaction(faction);
	for (const auto& kv : catalog.items) if (kv.second.category == "Weapon") { inventory.Grant(kv.first, 1); break; }
	if (phase == SessionPhase::Boot || phase == SessionPhase::LoggedIn) phase = SessionPhase::WorldLobby;
	log.push_back(std::string("CHAR ") + name + " " + FactionName(faction));
	PersistCharacter();
	return true;
}
bool WorldService::ApplyAppearance(const CharacterAppearance& app) {
	if (!character) return false; appearance = app;
	log.push_back("APPEARANCE_APPLY slots=" + std::to_string(appearance.clothing.size())
		+ " height=" + std::to_string(appearance.body.height)
		+ " bulk=" + std::to_string(appearance.body.bulk));
	PersistCharacter();
	return true;
}
bool WorldService::EquipClothing(const std::string& slot, const std::string& item_id, int32_t c0, int32_t c1, const std::string& decal) {
	if (!character) return false;
	auto r = customization.EquipFromCatalog(appearance, slot, item_id, c0, c1, decal);
	log.push_back(r.ok ? ("EQUIP " + slot + "=" + item_id) : ("EQUIP_FAIL " + r.error));
	if (r.ok) PersistCharacter();
	return r.ok;
}
std::string WorldService::SaveAppearanceBlob() const { return appearance.Serialize(); }
bool WorldService::LoadAppearanceBlob(const std::string& blob) {
	CharacterAppearance app; if (!CharacterAppearance::Deserialize(blob, app)) return false;
	appearance = app; log.push_back("APPEARANCE_LOAD"); return true;
}
bool WorldService::SaveCharacterConfig() {
	if (!character) return false;
	return config_blobs.Save(character->name, SaveAppearanceBlob());
}
bool WorldService::LoadCharacterConfig() {
	if (!character) return false;
	std::string blob;
	if (!config_blobs.Load(character->name, blob)) return false;
	return LoadAppearanceBlob(blob);
}
std::vector<DistrictInfo> WorldService::ListDistricts() const { return catalog.JoinableDistricts(); }
DistrictReservation WorldService::ReserveDistrict(const std::string& district_id, const std::string& player_name) {
	std::string who = player_name;
	if (who.empty() && character) who = character->name;
	return district_router.ReserveOrQueue(district_id, who, 64);
}
bool WorldService::JoinDistrict(const std::string& district_id, const std::string& player_name) {
	if (!character) return false;
	const DistrictInfo* found = nullptr;
	for (const auto& d : catalog.districts) if (d.id == district_id || d.name == district_id) { found = &d; break; }
	if (!found || !found->joinable) return false;
	std::string who = player_name.empty() ? character->name : player_name;
	auto res = district_router.ReserveOrQueue(found->id, who, found->max_players > 0 ? found->max_players : 64);
	if (res.state == DistrictQueueState::Queued) {
		log.push_back("QUEUE " + found->id + " pos=" + std::to_string(res.queue_position));
		return false;
	}
	if (!district_router.Enter(who)) return false;
	DistrictSession s; s.session_id = res.session_id.empty() ? ("DS-" + found->id + "-1") : res.session_id;
	s.district_id = found->id;
	s.map_name = found->map_name.empty() ? "Lvl_APB_Financial_Freeroam" : found->map_name;
	s.players.push_back(who);
	s.population = district_router.population_by_district[found->id];
	district = s; phase = SessionPhase::District;
	if (stream_plan.active_district_id != found->id) stream_plan.active_district_id = found->id;
	log.push_back("JOIN " + s.session_id + " map=" + s.map_name + " look=" + appearance.Serialize());
	return true;
}
bool WorldService::JoinDistrictAsPeer(const std::string& session_id, const std::string& player_name) {
	if (!district || district->session_id != session_id) return false;
	for (const auto& p : district->players) if (p == player_name) return true;
	district->players.push_back(player_name);
	district_router.population_by_district[district->district_id] = (int32_t)district->players.size();
	district->population = (int32_t)district->players.size();
	log.push_back("PEER " + player_name);
	return true;
}
bool WorldService::ExitDistrict() {
	if (!district || !character) return false;
	district_router.Exit(character->name);
	log.push_back("EXIT_DISTRICT " + district->district_id);
	PersistCharacter();
	district.reset(); vehicle.reset();
	phase = SessionPhase::WorldLobby;
	return true;
}
ArmasResult WorldService::ArmasBuy(const std::string& item_id) {
	ArmasResult r; if (!character) { r.error = "no_character"; return r; }
	r = armas.Purchase(*character, inventory, item_id);
	log.push_back(r.ok ? ("ARMAS_BUY " + item_id) : ("ARMAS_FAIL " + r.error));
	if (r.ok) PersistCharacter();
	return r;
}
AuctionResult WorldService::AuctionList(const std::string& item_id, int32_t qty, int64_t price) {
	AuctionResult r; if (!character) { r.error = "no_character"; return r; }
	r = auction.ListItem(character->name, inventory, *character, item_id, qty, price);
	log.push_back(r.ok ? ("AUCTION_LIST " + std::to_string(r.listing_id)) : ("AUCTION_LIST_FAIL " + r.error));
	if (r.ok) { PersistCharacter(); PersistAuction(); }
	return r;
}
AuctionResult WorldService::AuctionBuy(int64_t listing_id, CharacterProfile& seller_profile, Inventory& seller_inv) {
	AuctionResult r; if (!character) { r.error = "no_character"; return r; }
	r = auction.Buyout(character->name, *character, inventory, seller_profile, seller_inv, listing_id);
	log.push_back(r.ok ? ("AUCTION_BUY " + std::to_string(listing_id)) : ("AUCTION_BUY_FAIL " + r.error));
	// Seller state is caller-owned (may belong to another account); only the
	// buyer aggregate and the listing store are persisted here.
	if (r.ok) { PersistCharacter(); PersistAuction(); }
	return r;
}
bool WorldService::StartMissionScript(const std::string& mission_id) {
	if (!character) return false;
	const MissionScriptDef* script = nullptr;
	if (!mission_id.empty()) script = mission_scripts.Find(mission_id);
	if (!script) script = mission_scripts.Find("JG_BCS4_Bom1");
	if (!script && !mission_scripts.scripts.empty()) script = &mission_scripts.scripts.begin()->second;
	if (!script) {
		mission = MissionRun::MakeDefault("mission_default", "Opposition Job", character->faction);
		mission->Start(); log.push_back("MISSION_START_DEFAULT"); return true;
	}
	mission = MissionRun::FromScript(*script, character->faction); mission->Start();
	// threat-driven opposition pressure scales contesting intensity
	if (threat.CurrentTier().opposition_multiplier >= 1.25) mission->opposition_contesting = true;
	log.push_back("MISSION_START " + mission->title + " stages=" + std::to_string(mission->StageCount())
		+ " contact=" + mission->contact_id + " pressure=" + std::to_string(OppositionPressure()));
	return true;
}
void WorldService::StartMission(const std::string& mission_id) { StartMissionScript(mission_id); }
bool WorldService::AdvanceMission(double amount) {
	if (!mission) return false;
	// scale progress difficulty slightly by opposition pressure
	double scaled = amount / std::max(0.5, OppositionPressure() * 0.85);
	bool done = mission->Progress(scaled);
	if (done) threat.ApplyMissionObjective();
	if (mission->status == MissionStatus::Completed) { threat.ApplyMissionComplete(); log.push_back("MISSION_COMPLETE"); }
	return done;
}
void WorldService::OppositionTakeout() {
	if (!mission) return;
	mission->RegisterOppositionTakeout();
	if (mission->status == MissionStatus::Failed) { threat.ApplyMissionFail(); log.push_back("MISSION_FAILED_TAKEOUTS"); }
}
ShotResult WorldService::FireWeapon(const std::string& weapon_id, CombatantState& shooter, CombatantState& target, double aim_x, double aim_y) {
	ShotResult r; const ItemDef* w = catalog.FindItem(weapon_id); if (!w) { r.reason = "unknown_weapon"; return r; }
	r = ResolveShot(*w, shooter, target, aim_x, aim_y, 1.0); if (r.killed) OnHostileKill(); return r;
}
void WorldService::OnHostileKill() { threat.ApplyKillOpponent(); log.push_back("THREAT " + std::to_string(threat.points)); }
std::vector<std::string> WorldService::ListMissionScriptIds() const { return mission_scripts.ListIds(); }
bool WorldService::SpawnVehicle(const std::string& vehicle_id) {
	if (!character || phase != SessionPhase::District) return false;
	// Prefer requested id; if missing/wrong category, fall back to first Vehicle catalog entry
	// so probes and freeroam never depend on stale hardcoded IDs.
	std::string id = vehicle_id;
	const ItemDef* v = catalog.FindItem(id);
	if (!v || v->category != "Vehicle") {
		id.clear();
		for (const auto& kv : catalog.items) {
			if (kv.second.category == "Vehicle") { id = kv.first; v = &kv.second; break; }
		}
	}
	if (!v || id.empty() || v->category != "Vehicle") return false;
	VehicleInstance inst;
	inst.vehicle_id = id;
	inst.package_family = id;
	// bind model registry if present (package / family hints)
	std::string fam = id;
	if (fam.rfind("Vehicle_", 0) == 0) fam = fam.substr(8);
	if (const ModelRef* m = models.FindVehiclePackage(fam)) inst.package_family = m->package;
	else if (const ModelRef* m2 = models.FindByFamily(fam)) inst.package_family = m2->package;
	else inst.package_family = fam;
	vehicle = inst;
	log.push_back("VEHICLE_SPAWN " + id + " pkg=" + inst.package_family);
	return true;
}
bool WorldService::PossessVehicle(const std::string& player_name) {
	if (!vehicle) return false;
	vehicle->driver = player_name.empty() && character ? character->name : player_name;
	vehicle->possessed = true;
	log.push_back("VEHICLE_POSSESS " + vehicle->driver);
	return true;
}
bool WorldService::ExitVehicle() {
	if (!vehicle || !vehicle->possessed) return false;
	vehicle->possessed = false; vehicle->driver.clear();
	log.push_back("VEHICLE_EXIT");
	return true;
}
std::vector<std::string> WorldService::StreamChunksNear(double x, double y) const {
	return stream_plan.ChunksNear(x, y);
}

// ---------------------------------------------------------------- persistence
bool WorldService::InitPersistence(const std::string& dir) {
	if (!store.Init(dir)) { log.push_back("PERSIST_INIT_FAIL " + dir); return false; }
	bool accOk = store.LoadAccounts(login);
	store.LoadAuction(auction);
	store.LoadMail(mail);
	log.push_back("PERSIST_INIT dir=" + dir
		+ " accounts=" + std::to_string(login.accounts.size())
		+ " listings=" + std::to_string(auction.listings.size())
		+ " mail=" + std::to_string(mail.messages.size())
		+ (accOk ? "" : " (fresh)"));
	return true;
}
void WorldService::SaveAllNow() {
	PersistAccounts();
	PersistCharacter();
	PersistAuction();
	PersistMail();
}
void WorldService::LogoutAccount() {
	PersistCharacter();
	log.push_back("LOGOUT" + (login.session ? (" " + login.session->username) : ""));
	district.reset(); vehicle.reset();
	login.Logout();
	character.reset();
	inventory.slots.clear();
	appearance = CharacterAppearance{};
	phase = SessionPhase::Boot;
}
bool WorldService::GrantItem(const std::string& item_id, int32_t qty) {
	if (!character) return false;
	bool ok = inventory.Grant(item_id, qty);
	if (ok) PersistCharacter();
	return ok;
}
bool WorldService::SendMail(const std::string& to, const std::string& subject, const std::string& body, int64_t cash) {
	if (!character) return false;
	bool ok = mail.SendMail(character->name, to, subject, body, cash);
	log.push_back(ok ? ("MAIL_SEND to=" + to) : "MAIL_SEND_FAIL");
	if (ok) PersistMail();
	return ok;
}
bool WorldService::MarkMailRead(int64_t id) {
	bool ok = mail.MarkRead(id);
	if (ok) PersistMail();
	return ok;
}
std::vector<const MailMessage*> WorldService::MailInbox() const {
	if (!character) return {};
	return mail.InboxFor(character->name);
}
void WorldService::PersistAccounts() {
	if (store.IsActive()) store.SaveAccounts(login);
}
void WorldService::PersistCharacter() {
	if (!store.IsActive() || !character || !login.session) return;
	store.SaveCharacter(login.session->username, character_slot, *character, appearance, inventory, threat.points);
}
void WorldService::PersistAuction() {
	if (store.IsActive()) store.SaveAuction(auction);
}
void WorldService::PersistMail() {
	if (store.IsActive()) store.SaveMail(mail);
}
void WorldService::TryLoadPersistedCharacter() {
	if (!store.IsActive() || !login.session) return;
	const std::string& acct = login.session->username;
	if (!store.HasCharacter(acct, character_slot)) return;
	CharacterProfile p; CharacterAppearance app; Inventory inv; double tp = 0;
	if (!store.LoadCharacter(acct, character_slot, p, app, inv, tp)) return;
	character = p; appearance = app; inventory = inv;
	threat.points = tp; threat.faction = p.faction;
	log.push_back("PERSIST_LOAD_CHAR " + p.name + " cash=" + std::to_string(p.cash));
}

DomainSnapshot WorldService::CaptureSnapshot() const {
	DomainSnapshot s;
	if (character) {
		s.has_character = true;
		s.character_name = character->name;
		s.faction = character->faction;
		s.cash = character->cash;
		s.g1c = character->g1c;
	}
	s.inventory_slot_count = (int32_t)inventory.slots.size();
	for (const auto& slot : inventory.slots) s.inventory_total_qty += slot.quantity;
	s.threat_points = threat.points;
	s.threat_bots = threat.CurrentTier().bot_count;
	if (mission) {
		s.mission_id = mission->id;
		s.mission_title = mission->title;
		s.mission_stage_index = mission->current_index;
		s.mission_stage_count = mission->StageCount();
		switch (mission->status) {
		case MissionStatus::Inactive: s.mission_status = "Inactive"; break;
		case MissionStatus::Active: s.mission_status = "Active"; break;
		case MissionStatus::Completed: s.mission_status = "Completed"; break;
		case MissionStatus::Failed: s.mission_status = "Failed"; break;
		}
	}
	if (district) {
		s.session_id = district->session_id;
		s.district_id = district->district_id;
		s.district_players = (int32_t)district->players.size();
	}
	return s;
}
}
