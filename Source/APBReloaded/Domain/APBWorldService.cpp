#include "APBWorldService.h"
#include "APBHandoff.h"
#include <algorithm>
#include <cmath>
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
	// Canonical retail mission titles (mirror of SDD table MissionTemplate), extracted from
	// MissionTemplates.INT by extract_mission_templates.ps1. Keyed by template id.
	mission_titles.LoadFromJsonFile(dir + "/mission_templates.json");
	// Mission-script ids share the MissionTemplate id space 1:1, so stamp the canonical retail
	// display titles onto the loaded scripts (verified: all shipped script ids resolve).
	const int32_t titledScripts = mission_titles.ApplyTo(mission_scripts);
	// Per-stage owner/dispatch mission briefs (mirror of SDD table TaskObjective), extracted from
	// TaskObjectives.INT by extract_task_objectives.ps1. Keyed by "<template_id>_Stage<NN>".
	mission_briefs.LoadFromJsonFile(dir + "/task_objectives.json");
	// Per-operation objective-type HUD labels (mirror of SDD table TaskOperation), extracted from
	// TaskOperations.INT by extract_task_operations.ps1. Keyed by operation id.
	mission_ops.LoadFromJsonFile(dir + "/task_operations.json");
		// Authentic retail mission end-screen Win/Lose/Draw messages (mirror of SDD table
		// MissionResultReason), extracted from MissionResultReasons.INT by
		// extract_mission_result_reasons.ps1. Keyed by result-reason id.
		mission_result_reasons.LoadFromJsonFile(dir + "/mission_result_reasons.json");
	catalog.LoadClothingJson(dir + "/clothing.json");
	models.LoadFromFile(dir + "/model_reference_catalog.json");
	LoadStreamPlan(dir + "/district_stream.json", stream_plan);
	// apbdb heat / notoriety / prestige thresholds (Content/Data/threat_table.json)
	const bool threatOk = threat.LoadFromThreatTableJson(dir + "/threat_table.json");
	// Matchmaking threat-rating tiers (In Training/Green/Bronze/Silver/Gold) + the
	// AllowedDistrictThreats district-join gating, extracted from ThreatLevels.INT by
	// extract_threat_ratings.ps1 (mirror of SDD table ThreatLevel). Distinct from heat above.
	const bool threatRatingsOk = threat_ratings.LoadFromJsonFile(dir + "/threat_ratings.json");
		// Faction-selection screen content (display names + General Info/Enforcer/Criminal
		// lore) extracted from Factions.INT by extract_factions.ps1 (mirror of SDD table
		// Faction). Pure UI/reference content for the character-create faction picker.
		const bool factionInfoOk = faction_info.LoadFromJsonFile(dir + "/factions.json");
	// Organisation catalog (contact gangs + Joker vendors + Armas store fronts) extracted
	// from Organisations.INT by extract_organisations.ps1 (mirror of SDD table Organisation).
	// Authoritative list the Armas store filters and contact UI group by.
	const bool organisationsOk = organisations.LoadFromJsonFile(dir + "/organisations.json");
	// Medal / award catalog (post-mission + profile achievements and demerit dishonours)
	// extracted from Medals.INT by extract_medals.ps1 (mirror of SDD table Medal).
	const bool medalsOk = medals.LoadFromJsonFile(dir + "/medals.json");
	// Street-name catalog (world-map / minimap location + intersection labels) extracted from
	// StreetName.INT by extract_street_names.ps1 (mirror of SDD table StreetName).
	const bool streetNamesOk = street_names.LoadFromJsonFile(dir + "/street_names.json");
	// Ammunition-category catalog (weapon ammo pools + HUD ammo-counter text) extracted from
	// AmmoCategories.INT by extract_ammo_categories.ps1 (mirror of SDD table AmmoCategories).
	const bool ammoCategoriesOk = ammo_categories.LoadFromJsonFile(dir + "/ammo_categories.json");
	// Scoreboard-column tooltip catalog extracted from ScoreboardDescriptions.INT by
	// extract_scoreboard_descriptions.ps1 (mirror of SDD table ScoreboardDescription).
	const bool scoreboardOk = scoreboard_descriptions.LoadFromJsonFile(dir + "/scoreboard_descriptions.json");
	// On-screen combat score-feed messages extracted from HUDCombatMessages.INT by
	// extract_hud_combat_messages.ps1 (mirror of SDD table HUDCombatMessage).
	const bool hudCombatOk = hud_combat_messages.LoadFromJsonFile(dir + "/hud_combat_messages.json");
	// Broad on-screen HUD notifications/prompts/error banners (with <col:> spans + <Token>
	// placeholders) extracted from HUDMessages.INT by extract_hud_messages.ps1 (SDD table HUDMessage).
	const bool hudMessagesOk = hud_messages.LoadFromJsonFile(dir + "/hud_messages.json");
	// Character/vehicle/weapon/consumable modification effect tooltips (multi-line, colour-marked-up)
	// extracted from ModifierEffects.INT by extract_modifier_effects.ps1 (mirror of SDD table ModifierEffect).
	const bool modifierEffectsOk = modifier_effects.LoadFromJsonFile(dir + "/modifier_effects.json");
	// Purchasable/equippable modification items (type label + flavour, id maps to a ModifierEffects
	// row via ModifierItemTypeCatalog::EffectId) extracted from ModifierItemTypes.INT (SDD table ModifierItemType).
	const bool modifierItemTypesOk = modifier_item_types.LoadFromJsonFile(dir + "/modifier_item_types.json");
	// Per-rank role progression: milestone titles + reward-mail text (id maps to a player_roles id
	// via RoleMilestoneCatalog::RoleId) extracted from RoleMilestones.INT (SDD table RoleMilestones).
	const bool roleMilestonesOk = role_milestones.LoadFromJsonFile(dir + "/role_milestones.json");
	// Reward-bundle display descriptions (rewards UI / reward-mail body) extracted from
	// RewardPackages.INT (SDD table RewardPackages) by extract_reward_packages.ps1.
	const bool rewardPackagesOk = reward_packages.LoadFromJsonFile(dir + "/reward_packages.json");
	// Reward-mail subject/body for granted weighted rewards (bios/weapons/consumables/seasonal)
	// extracted from WeightedRewards.INT (SDD table WeightedRewards) by extract_weighted_rewards.ps1.
	const bool weightedRewardsOk = weighted_rewards.LoadFromJsonFile(dir + "/weighted_rewards.json");
		// Player-choice reward confirmation mails (Retail/Leased weapon presets, clothing, titles, skins,
		// vehicles, emotes, bundles) extracted from RedeemableRewards.INT by extract_redeemable_rewards.ps1.
		const bool redeemableRewardsOk = redeemable_rewards.LoadFromJsonFile(dir + "/redeemable_rewards.json");
			// Per-component reward item descriptions + confirmation mails (vehicle customization kits,
			// clothing/outfit/title/weapon-skin components) extracted from RewardPackageItemTypes.INT by
			// extract_reward_item_types.ps1.
			const bool rewardItemTypesOk = reward_item_types.LoadFromJsonFile(dir + "/reward_item_types.json");
						// Master inventory item-type dictionary (id -> display name + creator) extracted from
						// InventoryItemTypes.INT by extract_inventory_item_types.ps1.
						const bool inventoryItemTypesOk = inventory_item_types.LoadFromJsonFile(dir + "/inventory_item_types.json");
						// Unlock item-type descriptions (emotes, capacity unlocks, daily-activity tokens, ...)
						// extracted from UnlockItemTypes.INT by extract_unlock_item_types.ps1.
						const bool unlockItemTypesOk = unlock_item_types.LoadFromJsonFile(dir + "/unlock_item_types.json");
						// Inventory item CATEGORY taxonomy (UI grouping/labels) extracted from
						// InventoryItemInfraCategories.INT by extract_inventory_infra_categories.ps1.
						const bool inventoryInfraCategoriesOk = inventory_infra_categories.LoadFromJsonFile(dir + "/inventory_infra_categories.json");
						// Weapon rich descriptions (Armas/weapon-select flavour + role blurb) extracted from
						// WeaponItemTypes.INT by extract_weapon_item_types.ps1.
						const bool weaponItemTypesOk = weapon_item_types.LoadFromJsonFile(dir + "/weapon_item_types.json");
							// Vehicle rich descriptions (Armas/vehicle-select flavour + role blurb) extracted from
						// VehicleItemTypes.INT by extract_vehicle_item_types.ps1.
						const bool vehicleItemTypesOk = vehicle_item_types.LoadFromJsonFile(dir + "/vehicle_item_types.json");
							// Clothing rich descriptions (Armas/customization flavour + role blurb) extracted from
						// ClothingItemTypes.INT by extract_clothing_item_types.ps1.
						const bool clothingItemTypesOk = clothing_item_types.LoadFromJsonFile(dir + "/clothing_item_types.json");
							// Authoritative retail contact name + untruncated bio (Contacts.INT). Separate from the apbdb-scraped
							// contacts_lore.json (whose bios are truncated ~500 chars) so both coexist merge-safely.
							const bool contactsCatalogOk = contacts_catalog.LoadFromJsonFile(dir + "/contacts_catalog.json");
							// In-game City Guide / tutorial onboarding text (Tutorials.INT) — Title/SubTitle/HTML Body per topic.
							const bool tutorialsOk = tutorials.LoadFromJsonFile(dir + "/tutorials.json");
							// Loading-screen gameplay hints (LoadingMovieTips.INT) shown over the loading movie during streaming.
							const bool loadingTipsOk = loading_tips.LoadFromJsonFile(dir + "/loading_tips.json");
				// Voice-line captions spoken by NPCs / contacts / enforcers / criminals during missions, greetings,
				// taunts, radio chatter (Subtitles_MASC.int, SDD subtitle table). Masc/fem are byte-identical in this
				// build, so one flat id->text catalog is ported from the MASC file.
				const bool subtitlesOk = subtitles.LoadFromJsonFile(dir + "/subtitles.json");
							// Collectible/achievement/progression display entries (DisplayPoint.INT, SDD DisplayPoint).
							const bool displayPointsOk = display_points.LoadFromJsonFile(dir + "/display_points.json");
							// In-game advisory / help popups shown during play (PopupDialogs.INT, SDD PopupDialogs).
							const bool popupDialogsOk = popup_dialogs.LoadFromJsonFile(dir + "/popup_dialogs.json");
							// Role-dependent HUD marker labels for mission objectives / spawns (HUDMarkerVisualText.INT, SDD HUDMarkerVisualText).
							const bool hudMarkerTextOk = hud_marker_text.LoadFromJsonFile(dir + "/hud_marker_text.json");
							// Chat-channel slash commands + help text (ChatMessageCategories.INT, SDD ChatMessageCategory).
							const bool chatMsgCatOk = chat_message_categories.LoadFromJsonFile(dir + "/chat_message_categories.json");
							// Emote slash commands + display names for the emote wheel UI (EmoteCommands.INT, SDD EmoteCommand).
							const bool emoteCmdsOk = emote_commands.LoadFromJsonFile(dir + "/emote_commands.json");
							// Big on-screen celebration popup titles (HUDCeremonyMsg.INT, SDD HUDCeremonyMsg).
							const bool ceremonyMsgsOk = ceremony_msgs.LoadFromJsonFile(dir + "/hud_ceremony_msgs.json");
							// Mission objective display names (TaskTargetTypes.INT, SDD TaskTargetType).
							const bool taskTargetTypesOk = task_target_types.LoadFromJsonFile(dir + "/task_target_types.json");
							// Context-sensitive HUD interaction labels (GameplayObjects.INT, SDD GameplayObject).
							const bool gameplayObjectsOk = gameplay_objects.LoadFromJsonFile(dir + "/gameplay_objects.json");
							// Mission toolkit item descriptions (EquipmentTypes.INT, SDD EquipmentType).
							const bool equipmentTypesOk = equipment_types.LoadFromJsonFile(dir + "/equipment_types.json");
							// Inventory capacity expansion descriptions (CapacityItemTypes.INT, SDD CapacityItemType).
							const bool capacityItemsOk = capacity_items.LoadFromJsonFile(dir + "/capacity_item_types.json");
							// Reward-package description/mail text (RewardPackageItemTypes.INT, SDD RewardPackageItemTypes).
							const bool rewardPackageItemTypesOk = reward_package_item_types.LoadFromJsonFile(dir + "/reward_package_item_types.json");
							// Daily-activity objective text + flavour variants (DailyActivityContacts.INT, SDD DailyActivityContacts).
							const bool dailyActivityContactsOk = daily_activity_contacts.LoadFromJsonFile(dir + "/daily_activity_contacts.json");
							// Per-tracked-value HUD labels per mission-operation type (TaskOperationUIProfile.INT, SDD TaskOperationUIProfile).
							const bool taskOperationUIProfilesOk = task_operation_ui_profiles.LoadFromJsonFile(dir + "/task_operation_ui_profiles.json");
							// Frontend/menu UI hover tooltips keyed by (scene, widget) (Tooltips.INT, section-scoped Scene@Widget).
							const bool tooltipsOk = tooltips.LoadFromJsonFile(dir + "/tooltips.json");
	// M15 progression reference data: contacts + roles + real retail per-contact level counts
	// (contact_levels.json is extracted from ContactLevels.INT by extract_contact_levels.ps1).
	progression.LoadContactsFromFile(dir + "/contacts_lore.json");
	progression.LoadRolesFromFile(dir + "/roles.json");
	// player_roles.json is the full retail roster (canonical display names + descriptions),
	// extracted from PlayerRoles.INT by extract_player_roles.ps1. Loaded AFTER roles.json so the
	// retail-canonical entries merge on top of the partial apbdb seed (merge-by-id, retail wins).
	progression.LoadRolesFromFile(dir + "/player_roles.json");
	const bool contactLevelsOk = progression.LoadContactLevelsFromFile(dir + "/contact_levels.json");
	armas.catalog = &catalog; auction.catalog = &catalog; customization.catalog = &catalog;
	world_dir.EnsureDefault();
	phase = SessionPhase::Boot;
	log.push_back("INIT items=" + std::to_string(catalog.items.size())
		+ " districts=" + std::to_string(catalog.districts.size())
		+ " scripts=" + std::to_string(mission_scripts.scripts.size())
		+ " stream_chunks=" + std::to_string(stream_plan.chunks.size())
		+ " models=" + std::to_string(models.VehicleCount() + models.CharacterCount())
		+ " threat_table=" + (threatOk ? "1" : "0")
		+ " threat_ratings=" + std::to_string(threat_ratings.Count()) + (threatRatingsOk ? "" : " (threat_ratings_missing)")
				+ " factions=" + std::to_string(faction_info.Count()) + (factionInfoOk ? "" : " (factions_missing)")
		+ " organisations=" + std::to_string(organisations.Count()) + (organisationsOk ? "" : " (organisations_missing)")
		+ " medals=" + std::to_string(medals.Count()) + (medalsOk ? "" : " (medals_missing)")
		+ " street_names=" + std::to_string(street_names.Count()) + (streetNamesOk ? "" : " (street_names_missing)")
		+ " ammo_categories=" + std::to_string(ammo_categories.Count()) + (ammoCategoriesOk ? "" : " (ammo_categories_missing)")
		+ " scoreboard_descriptions=" + std::to_string(scoreboard_descriptions.Count()) + (scoreboardOk ? "" : " (scoreboard_descriptions_missing)")
		+ " hud_combat_messages=" + std::to_string(hud_combat_messages.Count()) + (hudCombatOk ? "" : " (hud_combat_messages_missing)")
		+ " hud_messages=" + std::to_string(hud_messages.Count()) + (hudMessagesOk ? "" : " (hud_messages_missing)")
		+ " modifier_effects=" + std::to_string(modifier_effects.Count()) + (modifierEffectsOk ? "" : " (modifier_effects_missing)")
		+ " modifier_item_types=" + std::to_string(modifier_item_types.Count()) + (modifierItemTypesOk ? "" : " (modifier_item_types_missing)")
		+ " role_milestones=" + std::to_string(role_milestones.Count()) + (roleMilestonesOk ? "" : " (role_milestones_missing)")
		+ " reward_packages=" + std::to_string(reward_packages.Count()) + (rewardPackagesOk ? "" : " (reward_packages_missing)")
		+ " weighted_rewards=" + std::to_string(weighted_rewards.Count()) + (weightedRewardsOk ? "" : " (weighted_rewards_missing)")
				+ " redeemable_rewards=" + std::to_string(redeemable_rewards.Count()) + (redeemableRewardsOk ? "" : " (redeemable_rewards_missing)")
						+ " reward_item_types=" + std::to_string(reward_item_types.Count()) + (rewardItemTypesOk ? "" : " (reward_item_types_missing)")
								+ " inventory_item_types=" + std::to_string(inventory_item_types.Count()) + (inventoryItemTypesOk ? "" : " (inventory_item_types_missing)")
		+ " unlock_item_types=" + std::to_string(unlock_item_types.Count()) + (unlockItemTypesOk ? "" : " (unlock_item_types_missing)")
		+ " inventory_infra_categories=" + std::to_string(inventory_infra_categories.Count()) + (inventoryInfraCategoriesOk ? "" : " (inventory_infra_categories_missing)")
		+ " weapon_item_types=" + std::to_string(weapon_item_types.Count()) + (weaponItemTypesOk ? "" : " (weapon_item_types_missing)")
				+ " vehicle_item_types=" + std::to_string(vehicle_item_types.Count()) + (vehicleItemTypesOk ? "" : " (vehicle_item_types_missing)")
						+ " clothing_item_types=" + std::to_string(clothing_item_types.Count()) + (clothingItemTypesOk ? "" : " (clothing_item_types_missing)")
								+ " contacts_catalog=" + std::to_string(contacts_catalog.Count()) + (contactsCatalogOk ? "" : " (contacts_catalog_missing)")
								+ " tutorials=" + std::to_string(tutorials.Count()) + (tutorialsOk ? "" : " (tutorials_missing)")
								+ " loading_tips=" + std::to_string(loading_tips.Count()) + (loadingTipsOk ? "" : " (loading_tips_missing)")
					+ " subtitles=" + std::to_string(subtitles.Count()) + (subtitlesOk ? "" : " (subtitles_missing)")
								+ " display_points=" + std::to_string(display_points.Count()) + (displayPointsOk ? "" : " (display_points_missing)")
								+ " popup_dialogs=" + std::to_string(popup_dialogs.Count()) + (popupDialogsOk ? "" : " (popup_dialogs_missing)")
								+ " hud_marker_text=" + std::to_string(hud_marker_text.Count()) + (hudMarkerTextOk ? "" : " (hud_marker_text_missing)")
							+ " chat_message_categories=" + std::to_string(chat_message_categories.Count()) + (chatMsgCatOk ? "" : " (chat_message_categories_missing)")
							+ " emote_commands=" + std::to_string(emote_commands.Count()) + (emoteCmdsOk ? "" : " (emote_commands_missing)")
							+ " ceremony_msgs=" + std::to_string(ceremony_msgs.Count()) + (ceremonyMsgsOk ? "" : " (ceremony_msgs_missing)")
							+ " task_target_types=" + std::to_string(task_target_types.Count()) + (taskTargetTypesOk ? "" : " (task_target_types_missing)")
							+ " gameplay_objects=" + std::to_string(gameplay_objects.Count()) + (gameplayObjectsOk ? "" : " (gameplay_objects_missing)")
							+ " equipment_types=" + std::to_string(equipment_types.Count()) + (equipmentTypesOk ? "" : " (equipment_types_missing)")
							+ " capacity_items=" + std::to_string(capacity_items.Count()) + (capacityItemsOk ? "" : " (capacity_items_missing)")
							+ " reward_package_item_types=" + std::to_string(reward_package_item_types.Count()) + (rewardPackageItemTypesOk ? "" : " (reward_package_item_types_missing)")
							+ " daily_activity_contacts=" + std::to_string(daily_activity_contacts.Count()) + (dailyActivityContactsOk ? "" : " (daily_activity_contacts_missing)")
							+ " task_operation_ui_profiles=" + std::to_string(task_operation_ui_profiles.Count()) + (taskOperationUIProfilesOk ? "" : " (task_operation_ui_profiles_missing)")
							+ " tooltips=" + std::to_string(tooltips.Count()) + (tooltipsOk ? "" : " (tooltips_missing)")
		+ " contact_levels=" + std::to_string(progression.ContactLevelCount())
		+ " roles=" + std::to_string(progression.RoleCount())
		+ " mission_titles=" + std::to_string(mission_titles.Count())
		+ " titled_scripts=" + std::to_string(titledScripts)
		+ " mission_briefs=" + std::to_string(mission_briefs.Count())
		+ " mission_ops=" + std::to_string(mission_ops.Count())
		+ " mission_result_reasons=" + std::to_string(mission_result_reasons.Count())
		+ (contactLevelsOk ? "" : " (contact_levels_missing)"));
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
	progress = CharacterProgress{};
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
bool WorldService::CancelDistrictReservation(const std::string& player_name) {
	const std::string who = player_name.empty() && character ? character->name : player_name;
	return district_router.Exit(who);
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
	if (mission->status == MissionStatus::Completed) {
		threat.ApplyMissionComplete();
		ApplyMissionCompletionReward();
		log.push_back("MISSION_COMPLETE");
	}
	return done;
}
// Opposed-mission race: the opposition team accrues progress on the contested stage, scaled up by
// threat-tier opposition pressure (a stronger opposition closes the gap faster). If the opposition
// wins the stage objective first the mission fails for the owner (opposition_won), mirroring APB's
// symmetric race where either side can secure the objective.
bool WorldService::AdvanceOpposition(double amount) {
	if (!mission) return false;
	double scaled = amount * std::max(0.5, OppositionPressure());
	bool decided = mission->AdvanceOpposition(scaled);
	if (decided && mission->status == MissionStatus::Failed) {
		threat.ApplyMissionFail();
		log.push_back("MISSION_OPPOSITION_WON stage=" + std::to_string(mission->current_index));
	}
	return decided;
}
// Deterministic APB stage countdown: the district GameMode ticks this with its authoritative
// clock; when the current stage's time_limit_sec elapses the mission fails (timed_out).
bool WorldService::TickMission(double now_sec) {
	if (!mission) return false;
	bool timed = mission->CheckTimeout(now_sec);
	if (timed && mission->status == MissionStatus::Failed) {
		threat.ApplyMissionFail();
		log.push_back("MISSION_TIMEOUT stage=" + std::to_string(mission->current_index));
	}
	return timed;
}
// M11 (D10): facade over Matchmaker::FormMatches. Refuses to dispatch a new pairing while
// a mission run is already active (singleton one-pair limit — recorded tech debt for
// group missions). The district GameMode calls this at a ~5s cadence and handles UE-side
// dispatch (spawning opposition / starting the mission for the paired parties).
std::vector<MatchPairing> WorldService::FormMatches(int64_t now_ms) {
	if (mission && mission->status == MissionStatus::Active) {
		return {}; // active run — do not form a new pairing (singleton guard)
	}
	return matchmaker.FormMatches(now_ms);
}
// Contacts reward the player with cash + standing for completing their work. Base payout is a
// tunable recreation default scaled by stage count (no per-mission reward table is parsed from
// the catalog yet), then scaled by the active threat tier's reward multiplier. Role/weapon XP
// is per-action (kills) and handled separately, matching APB. Persists the character on award.
void WorldService::ApplyMissionCompletionReward() {
	if (!mission || !character) return;
	const int32_t stages = std::max(1, mission->StageCount());
	const int64_t base_cash = 250 * stages;
	const int64_t base_standing = 100 * stages;
	const MissionReward reward = ComputeMissionReward(base_cash, base_standing, 0,
		threat.CurrentTier().reward_multiplier);
	character->cash += reward.cash;
	if (!mission->contact_id.empty())
		progress.AddContactStanding(mission->contact_id, reward.standing);
	log.push_back("MISSION_REWARD cash=" + std::to_string(reward.cash)
		+ " standing=" + std::to_string(reward.standing) + " contact=" + mission->contact_id);
	PersistCharacter();
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
bool WorldService::InitSocialPersistence(const std::string& dir) {
	social_store.Init(dir + "/social");
	if (social_store.IsActive()) { social_store.LoadClans(clans); social_store.LoadFriends(friends_svc); social_store.LoadMail(mail); }
	claim_journal.Init(dir + "/social");
	claim_journal.Load();
	return social_store.IsActive();
}
void WorldService::SaveAllNow() {
	PersistAccounts();
	PersistCharacter();
	PersistAuction();
	PersistMail();
	if (social_store.IsActive()) { social_store.SaveClans(clans); social_store.SaveFriends(friends_svc); social_store.SaveMail(mail); }
}
void WorldService::SaveSocialNow() {
	if (social_store.IsActive()) { social_store.SaveClans(clans); social_store.SaveFriends(friends_svc); social_store.SaveMail(mail); }
}
void WorldService::LogoutAccount() {
	PersistCharacter();
	log.push_back("LOGOUT" + (login.session ? (" " + login.session->username) : ""));
	district.reset(); vehicle.reset();
	login.Logout();
	character.reset();
	inventory.slots.clear();
	appearance = CharacterAppearance{};
	progress = CharacterProgress{};
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
	store.SaveProgress(login.session->username, character_slot, progress);
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
	progress = CharacterProgress{};
	store.LoadProgress(acct, character_slot, progress); // tolerate-missing: fresh char keeps empty progress
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
	{
		const ThreatTier tier = threat.CurrentTier();
		s.threat_level = tier.level;
		s.threat_tier_name = tier.name;
		s.threat_tier_description = tier.description;
	}
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
		s.mission_opposition_contesting = mission->opposition_contesting;
		s.mission_opposition_won = mission->opposition_won;
		s.mission_timed_out = mission->timed_out;
		s.mission_stage_deadline_server_sec = mission->current_stage_deadline_sec;
		if (const MissionStageRuntime* st = mission->Current()) {
			const double target = st->def.target_progress > 0 ? st->def.target_progress : 1.0;
			s.mission_stage_progress = st->progress / target;
			s.mission_opp_stage_progress = st->opp_progress / target;
			s.mission_stage_time_limit_sec = st->def.time_limit_sec;
		}
	}
	if (district) {
		s.session_id = district->session_id;
		s.district_id = district->district_id;
		s.district_players = (int32_t)district->players.size();
	}
	// M15 progression: expose per-character standing + role XP, id-sorted for deterministic sync.
	for (const auto& kv : progress.contact_standing)
		if (kv.second > 0) s.contact_standings.push_back({ kv.first, kv.second });
	for (const auto& kv : progress.role_xp)
		if (kv.second > 0) s.role_xp.push_back({ kv.first, kv.second });
	auto byId = [](const SnapshotProgressEntry& a, const SnapshotProgressEntry& b) { return a.id < b.id; };
	std::sort(s.contact_standings.begin(), s.contact_standings.end(), byId);
	std::sort(s.role_xp.begin(), s.role_xp.end(), byId);
	if (mission && !mission->contact_id.empty()) {
		const LevelLadder ladder = progression.LadderForContact(mission->contact_id);
		s.active_contact_id = mission->contact_id;
		s.active_contact_standing = progress.ContactStanding(mission->contact_id);
		s.active_contact_level = progress.ContactLevel(mission->contact_id, ladder);
	}
	return s;
}

bool WorldService::RestoreHandoff(const DomainSnapshot& snapshot) {
	if (!snapshot.has_character || snapshot.character_name.empty() || snapshot.cash < 0 || snapshot.g1c < 0 ||
		snapshot.inventory_slot_count < 0 || snapshot.inventory_total_qty < 0 ||
		snapshot.inventory_slot_count > snapshot.inventory_total_qty || !std::isfinite(snapshot.threat_points)) return false;

	CharacterProfile next_character{snapshot.character_name, snapshot.faction, snapshot.cash, snapshot.g1c};
	ThreatSystem next_threat = threat;
	next_threat.faction = snapshot.faction;
	next_threat.points = snapshot.threat_points;
	Inventory next_inventory;
	if (snapshot.inventory_total_qty > 0) {
		if (snapshot.inventory_slot_count == 0) return false;
		const int32_t base_qty = snapshot.inventory_total_qty / snapshot.inventory_slot_count;
		const int32_t remainder = snapshot.inventory_total_qty % snapshot.inventory_slot_count;
		for (int32_t index = 0; index < snapshot.inventory_slot_count; ++index) {
			next_inventory.slots.push_back({"handoff_inventory_" + std::to_string(index), base_qty + (index < remainder ? 1 : 0)});
		}
	}
	CharacterProgress next_progress;
	for (const SnapshotProgressEntry& entry : snapshot.contact_standings) {
		if (entry.id.empty() || entry.value < 0 || next_progress.contact_standing.count(entry.id)) return false;
		next_progress.contact_standing.emplace(entry.id, entry.value);
	}
	for (const SnapshotProgressEntry& entry : snapshot.role_xp) {
		if (entry.id.empty() || entry.value < 0 || next_progress.role_xp.count(entry.id)) return false;
		next_progress.role_xp.emplace(entry.id, entry.value);
	}

	std::optional<MissionRun> next_mission;
	if (!snapshot.mission_id.empty()) {
		const MissionScriptDef* script = mission_scripts.Find(snapshot.mission_id);
		if (script) next_mission = MissionRun::FromScript(*script, snapshot.faction);
		else next_mission = MissionRun::MakeDefault(snapshot.mission_id, snapshot.mission_title, snapshot.faction);
		next_mission->Start();
		const int32_t required_stages = std::max(1, snapshot.mission_stage_count);
		while (next_mission->StageCount() < required_stages) {
			MissionStageRuntime stage;
			stage.def.index = next_mission->StageCount();
			stage.def.target_progress = 1.0;
			next_mission->stages.push_back(std::move(stage));
		}
		if (next_mission->StageCount() > required_stages) next_mission->stages.resize(required_stages);
		next_mission->title = snapshot.mission_title.empty() ? next_mission->title : snapshot.mission_title;
		next_mission->current_index = std::max(0, std::min(snapshot.mission_stage_index, std::max(0, next_mission->StageCount() - 1)));
		next_mission->status = snapshot.mission_status == "Completed" ? MissionStatus::Completed :
			snapshot.mission_status == "Failed" ? MissionStatus::Failed :
			snapshot.mission_status == "Inactive" ? MissionStatus::Inactive : MissionStatus::Active;
		next_mission->opposition_contesting = snapshot.mission_opposition_contesting;
		next_mission->opposition_won = snapshot.mission_opposition_won;
		next_mission->timed_out = snapshot.mission_timed_out;
		if (MissionStageRuntime* stage = next_mission->Current()) {
			const double target = stage->def.target_progress > 0 ? stage->def.target_progress : 1.0;
			stage->progress = std::max(0.0, std::min(1.0, snapshot.mission_stage_progress)) * target;
			stage->opp_progress = std::max(0.0, std::min(1.0, snapshot.mission_opp_stage_progress)) * target;
		}
	}

	std::optional<DistrictSession> next_district;
	if (!snapshot.session_id.empty()) {
		DistrictSession session;
		session.session_id = snapshot.session_id;
		session.district_id = snapshot.district_id;
		session.players.push_back(snapshot.character_name);
		session.population = std::max(1, snapshot.district_players);
		next_district = std::move(session);
	}

	character = std::move(next_character);
	threat = std::move(next_threat);
	inventory = std::move(next_inventory);
	progress = std::move(next_progress);
	mission = std::move(next_mission);
	district = std::move(next_district);
	phase = district ? SessionPhase::District : SessionPhase::WorldLobby;
	log.push_back("HANDOFF_APPLY " + snapshot.character_name);
	return true;
}

void WorldService::ReconcileMailClaims() {
	if (!character || !claim_journal.IsActive()) return;
	const std::vector<MailClaimReceipt> receipts = claim_journal.CommittedReceiptsFor(character->name);
	if (receipts.empty()) return;
	int64_t reapplied = 0;
	for (const MailClaimReceipt& r : receipts) {
		character->cash += r.cash_delta;
		reapplied += r.cash_delta;
		// Crash between the character receipt and the mail commit leaves the message
		// unclaimed while its cash is already paid; finish the flag so the player
		// cannot claim the same message a second time.
		if (!claim_journal.MailAlreadyCommitted(r.character, r.mail_id) && mail.CommitClaimed(r.mail_id))
			claim_journal.CommitMail(r.character, r.mail_id);
	}
	log.push_back("MAIL_CLAIM_RECONCILE " + character->name
		+ " receipts=" + std::to_string(receipts.size())
		+ " cash=" + std::to_string(reapplied));
}

bool WorldService::ApplyHandoff(const DomainSnapshot& snapshot) {
	if (!RestoreHandoff(snapshot)) return false;
	ReconcileMailClaims();
	PersistCharacter();
	return true;
}

bool WorldService::ApplyHandoffForAccount(const DomainSnapshot& snapshot, const std::string& account) {
	if (account.empty() || !RestoreHandoff(snapshot)) return false;
	ReconcileMailClaims();
	if (store.IsActive() && character) {
		if (!store.SaveCharacter(account, character_slot, *character, appearance, inventory, threat.points)) return false;
		if (!store.SaveProgress(account, character_slot, progress)) return false;
	}
	return true;
}
}
