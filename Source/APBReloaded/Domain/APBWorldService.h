#pragma once
#include "APBCatalog.h"
#include "APBInventory.h"
#include "APBArmas.h"
#include "APBAuction.h"
#include "APBThreat.h"
#include "APBFactionInfo.h"
#include "APBOrganisations.h"
#include "APBMedals.h"
#include "APBStreetNames.h"
#include "APBAmmoCategories.h"
#include "APBScoreboardDescriptions.h"
#include "APBHUDCombatMessages.h"
#include "APBHUDMessages.h"
#include "APBModifierEffects.h"
#include "APBModifierItemTypes.h"
#include "APBRoleMilestones.h"
#include "APBRewardPackages.h"
#include "APBWeightedRewards.h"
#include "APBRedeemableRewards.h"
#include "APBRewardItemTypes.h"
#include "APBInventoryItemTypes.h"
#include "APBUnlockItemTypes.h"
#include "APBInventoryInfraCategories.h"
#include "APBWeaponItemTypes.h"
#include "APBVehicleItemTypes.h"
#include "APBClothingItemTypes.h"
#include "APBContactsCatalog.h"
#include "APBTutorials.h"
#include "APBChatMessageCategories.h"
#include "APBEmoteCommands.h"
#include "APBCeremonyMsgs.h"
#include "APBTaskTargetTypes.h"
#include "APBGameplayObjects.h"
#include "APBEquipmentTypes.h"
#include "APBCapacityItems.h"
#include "APBRewardPackageItemTypes.h"
#include "APBDailyActivityContacts.h"
#include "APBTaskOperationUIProfiles.h"
#include "APBTooltips.h"
#include "APBLoadingTips.h"
#include "APBSubtitles.h"
#include "APBDisplayPoints.h"
#include "APBPopupDialogs.h"
#include "APBHUDMarkerText.h"
#include "APBMission.h"
#include "APBMatchmaking.h"
#include "APBCombat.h"
#include "APBCustomization.h"
#include "APBModelRegistry.h"
#include "APBSocial.h"
#include "APBGroup.h"
#include "APBClan.h"
#include "APBFriends.h"
#include "APBSocialStore.h"
#include "APBMailClaimJournal.h"
#include "APBPersistence.h"
namespace apb {
enum class SessionPhase { Boot, LoggedIn, WorldLobby, District };
struct DistrictSession {
	std::string session_id, district_id, map_name;
	std::vector<std::string> players;
	int32_t population = 0;
};
struct VehicleInstance {
	std::string vehicle_id;
	std::string package_family;
	std::string driver;
	bool possessed = false;
};
/** One progression track entry (contact standing or role XP), id-sorted for deterministic sync. */
struct SnapshotProgressEntry {
	std::string id;
	int64_t value = 0;
};
/** Authoritative Domain→UI snapshot (single bridge for UE PlayerState sync). */
struct DomainSnapshot {
	bool has_character = false;
	std::string character_name;
	Faction faction = Faction::Criminal;
	int64_t cash = 0;
	int64_t g1c = 0;
	int32_t inventory_slot_count = 0;
	int32_t inventory_total_qty = 0;
	double threat_points = 0;
	int32_t threat_bots = 0;
	// Current notoriety/prestige tier for the HUD heat display (apbdb /heat = HeatLevels.INT).
	int32_t threat_level = 0;
	std::string threat_tier_name;        // apbdb id, e.g. "NotorietyLevel3" / "PrestigeLevel2"
	std::string threat_tier_description; // player-facing blurb shown at this heat level
	std::string mission_id;
	std::string mission_title;
	int32_t mission_stage_index = 0;
	int32_t mission_stage_count = 0;
	std::string mission_status;
	// Opposed-mission race: contested state + owner/opposition current-stage progress fractions [0..1].
	bool mission_opposition_contesting = false;
	bool mission_opposition_won = false;
	double mission_stage_progress = 0;
	double mission_opp_stage_progress = 0;
	bool mission_timed_out = false;
	double mission_stage_time_limit_sec = 0; // current stage's countdown length (0 = no timer)
	double mission_stage_deadline_server_sec = 0; // absolute server-clock deadline for current stage (0 = no timer)
	std::string session_id;
	std::string district_id;
	int32_t district_players = 0;
	// ---- M15 progression (per-character; mirrors CharacterProgress, id-sorted) ----
	std::vector<SnapshotProgressEntry> contact_standings; // all contacts with standing > 0
	std::vector<SnapshotProgressEntry> role_xp;           // all roles with xp > 0
	// Convenience: the current mission's contact, for HUD "working for X" display.
	std::string active_contact_id;
	int64_t active_contact_standing = 0;
	int32_t active_contact_level = 0;
};
class WorldService {
public:
	Catalog catalog;
	MissionScriptLibrary mission_scripts;
	MissionTitleCatalog mission_titles;
	MissionBriefCatalog mission_briefs;
	MissionOperationCatalog mission_ops;
	MissionResultReasonCatalog mission_result_reasons;
	ModelRegistry models;
	CustomizationService customization;
	LoginService login;
	WorldDirectory world_dir;
	DistrictRouter district_router;
	SocialService social;
	MailService mail;
	ClanService clans;
	FriendsService friends_svc;
	GroupService groups;
	SocialStore social_store;
	MailClaimJournal claim_journal;
	Matchmaker matchmaker; // M11 (D10): threat-tier opposition pairing, world-scoped lifetime
	ConfigBlobStore config_blobs;
	DistrictStreamPlan stream_plan;
	SessionPhase phase = SessionPhase::Boot;
	std::optional<CharacterProfile> character;
	CharacterAppearance appearance;
	CharacterProgress progress; // M15 per-character contact standing + role XP (persisted sidecar)
	ProgressionCatalog progression; // M15 contacts/roles + real retail per-contact level counts
	Inventory inventory;
	ArmasStore armas;
	AuctionHouse auction;
	ThreatSystem threat;
	ThreatRatingCatalog threat_ratings; // matchmaking rating tiers + district-join gating (ThreatLevels.INT)
	FactionInfoCatalog faction_info; // faction-selection screen display names + lore (Factions.INT)
	OrganisationCatalog organisations; // contact orgs + weapon vendors + store fronts (Organisations.INT)
	MedalCatalog medals; // post-mission + profile achievements and demerit dishonours (Medals.INT)
	StreetNameCatalog street_names; // world-map / minimap location + intersection labels (StreetName.INT)
	AmmoCategoryCatalog ammo_categories; // weapon ammo pools + HUD ammo-counter text (AmmoCategories.INT)
	ScoreboardDescriptionCatalog scoreboard_descriptions; // scoreboard column tooltips (ScoreboardDescriptions.INT)
	HUDCombatMessageCatalog hud_combat_messages; // on-screen combat score-feed messages (HUDCombatMessages.INT)
	HUDMessageCatalog hud_messages; // broad on-screen HUD notifications/prompts/error banners (HUDMessages.INT)
	ModifierEffectCatalog modifier_effects; // character/vehicle/weapon/consumable mod tooltips (ModifierEffects.INT)
	ModifierItemTypeCatalog modifier_item_types; // purchasable/equippable mod items -> effect binding (ModifierItemTypes.INT)
	RoleMilestoneCatalog role_milestones; // per-rank role progression titles + reward mail (RoleMilestones.INT)
	RewardPackageCatalog reward_packages; // reward-bundle display descriptions (RewardPackages.INT)
	WeightedRewardCatalog weighted_rewards; // reward-mail subject/body for granted rewards (WeightedRewards.INT)
	RedeemableRewardCatalog redeemable_rewards; // player-choice reward confirmation mails (RedeemableRewards.INT)
	RewardItemTypeCatalog reward_item_types; // per-component reward descriptions + mails (RewardPackageItemTypes.INT)
	InventoryItemTypeCatalog inventory_item_types; // master id -> display-name dictionary (InventoryItemTypes.INT)
	UnlockItemTypeCatalog unlock_item_types; // unlock-item id -> description (UnlockItemTypes.INT)
	InventoryInfraCategoryCatalog inventory_infra_categories; // item category taxonomy (InventoryItemInfraCategories.INT)
	WeaponItemTypeCatalog weapon_item_types; // weapon id -> rich description (WeaponItemTypes.INT)
	VehicleItemTypeCatalog vehicle_item_types; // vehicle id -> rich description (VehicleItemTypes.INT)
		ClothingItemTypeCatalog clothing_item_types; // clothing id -> rich description (ClothingItemTypes.INT)
			ContactCatalog contacts_catalog; // authoritative retail contact name + untruncated bio (Contacts.INT)
			TutorialCatalog tutorials; // in-game City Guide / tutorial onboarding text (Tutorials.INT)
		ChatMessageCategoryCatalog chat_message_categories; // chat channel slash commands + help (ChatMessageCategories.INT)
		EmoteCommandCatalog emote_commands; // emote slash commands + display names (EmoteCommands.INT)
		CeremonyMsgCatalog ceremony_msgs; // big on-screen celebration popup titles (HUDCeremonyMsg.INT)
		TaskTargetTypeCatalog task_target_types; // mission objective display names (TaskTargetTypes.INT)
		GameplayObjectCatalog gameplay_objects; // context-sensitive HUD interaction labels (GameplayObjects.INT)
		EquipmentTypeCatalog equipment_types; // mission toolkit item descriptions (EquipmentTypes.INT)
		CapacityItemCatalog capacity_items; // inventory capacity expansion descriptions (CapacityItemTypes.INT)
		RewardPackageItemTypeCatalog reward_package_item_types; // reward-package description/mail text (RewardPackageItemTypes.INT)
		DailyActivityContactCatalog daily_activity_contacts; // daily-activity objective text + flavour variants (DailyActivityContacts.INT)
		TaskOperationUIProfileCatalog task_operation_ui_profiles; // per-tracked-value HUD labels per mission-operation (TaskOperationUIProfile.INT)
		TooltipCatalog tooltips; // frontend/menu UI hover tooltips keyed by (scene, widget) (Tooltips.INT)
			LoadingTipCatalog loading_tips; // loading-screen gameplay hints (LoadingMovieTips.INT)
		SubtitleCatalog subtitles; // voice-line captions for NPCs/contacts/missions (Subtitles_MASC.int)
	DisplayPointCatalog display_points; // collectible/achievement/progression display entries (DisplayPoint.INT)
	PopupDialogCatalog popup_dialogs; // in-game advisory/help popups (PopupDialogs.INT)
	HUDMarkerTextCatalog hud_marker_text; // role-dependent HUD marker labels (HUDMarkerVisualText.INT)
	std::optional<MissionRun> mission;
	std::optional<DistrictSession> district;
	std::optional<VehicleInstance> vehicle;
	std::string active_world_id;
	std::vector<std::string> log;

	// ---- persistence (M2, opt-in) ----
	JsonDomainStore store;
	int32_t character_slot = 0;

	bool InitFromDataDir(const std::string& dir);
	/** Opt-in file persistence. When never called (or it fails), behavior is
	 *  exactly the previous in-memory mode. Loads accounts/auction/mail. */
	bool InitPersistence(const std::string& dir);
	bool InitSocialPersistence(const std::string& dir);
	bool PersistenceActive() const { return store.IsActive(); }
	/** Force-save all aggregates (accounts/character/auction/mail). */
	void SaveAllNow();
	void SaveSocialNow();
	/** Saves the current character, clears session character state, logs out. */
	void LogoutAccount();
	/** Inventory grant with persistence hook. */
	bool GrantItem(const std::string& item_id, int32_t qty);
	/** Mail from the current character with persistence hook. */
	bool SendMail(const std::string& to, const std::string& subject, const std::string& body, int64_t cash = 0);
	bool MarkMailRead(int64_t id);
	std::vector<const MailMessage*> MailInbox() const;
	bool RegisterAccount(const std::string& user, const std::string& pass);
	bool LoginAccount(const std::string& user, const std::string& pass);
	bool EnterWorld(const std::string& world_id = "W1");
	bool CreateCharacter(const std::string& name, Faction faction);
	bool ApplyAppearance(const CharacterAppearance& app);
	bool EquipClothing(const std::string& slot, const std::string& item_id, int32_t c0 = 0, int32_t c1 = 0, const std::string& decal = "");
	std::string SaveAppearanceBlob() const;
	bool LoadAppearanceBlob(const std::string& blob);
	bool SaveCharacterConfig();
	bool LoadCharacterConfig();
	std::vector<DistrictInfo> ListDistricts() const;
	DistrictReservation ReserveDistrict(const std::string& district_id, const std::string& player_name = "");
	bool CancelDistrictReservation(const std::string& player_name);
	bool JoinDistrict(const std::string& district_id, const std::string& player_name);
	bool JoinDistrictAsPeer(const std::string& session_id, const std::string& player_name);
	bool ExitDistrict();
	ArmasResult ArmasBuy(const std::string& item_id);
	AuctionResult AuctionList(const std::string& item_id, int32_t qty, int64_t price);
	AuctionResult AuctionBuy(int64_t listing_id, CharacterProfile& seller_profile, Inventory& seller_inv);
	bool StartMissionScript(const std::string& mission_id = "");
	void StartMission(const std::string& mission_id = "");
	bool AdvanceMission(double amount = 1.0);
	/** Opposed-mission race: opposition team accrues progress on the contested stage.
	 *  Returns true when the opposition decides the mission (wins the stage → Failed). */
	bool AdvanceOpposition(double amount = 1.0);
	/** Deterministic mission-stage countdown tick (APB stage timers). Fails the mission when the
	 *  current stage's time_limit_sec elapses; applies threat fail + logs. now_sec is caller time. */
	bool TickMission(double now_sec);
	/** M11 (D10): facade over Matchmaker::FormMatches. Refuses to form a new pairing while a
	 *  mission run is already active (singleton one-pair limit — recorded tech debt for group
	 *  missions). Returns formed opposition pairings for UE-side district dispatch. */
	std::vector<MatchPairing> FormMatches(int64_t now_ms);
	void OppositionTakeout();
	ShotResult FireWeapon(const std::string& weapon_id, CombatantState& shooter, CombatantState& target, double aim_x, double aim_y);
	void OnHostileKill();
	std::vector<std::string> ListMissionScriptIds() const;
	bool SpawnVehicle(const std::string& vehicle_id);
	bool PossessVehicle(const std::string& player_name);
	bool ExitVehicle();
	std::vector<std::string> StreamChunksNear(double x, double y) const;
	double NotorietyOrPrestige() const { return threat.points; }
	double OppositionPressure() const { return threat.CurrentTier().opposition_multiplier; }
	/** Pure snapshot of shipped Domain fields — used by UE SyncPlayerStateFromDomain and Domain tests. */
	DomainSnapshot CaptureSnapshot() const;
	bool ApplyHandoff(const DomainSnapshot& snapshot);
	bool ApplyHandoffForAccount(const DomainSnapshot& snapshot, const std::string& account);

private:
	bool RestoreHandoff(const DomainSnapshot& snapshot);
	/** Defect 6: a district snapshot carries the cash value the district knew, so a
	 *  stale return silently reverts a claim the world already committed. Reapplies
	 *  every committed receipt for the restored character exactly once, and finishes
	 *  any mail flag left uncommitted by a crash. Must run after RestoreHandoff and
	 *  before persistence. Idempotent: RestoreHandoff resets cash from the snapshot
	 *  each time, so reapplying the same receipts converges on the same total. */
	void ReconcileMailClaims();
	void PersistAccounts();
	void PersistCharacter();
	void PersistAuction();
	void PersistMail();
	void TryLoadPersistedCharacter();
	/** M15: on mission completion, award cash + contact standing (threat-scaled) and persist. */
	void ApplyMissionCompletionReward();
};
}
