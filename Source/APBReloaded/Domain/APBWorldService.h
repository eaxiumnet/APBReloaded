#pragma once
#include "APBCatalog.h"
#include "APBInventory.h"
#include "APBArmas.h"
#include "APBAuction.h"
#include "APBThreat.h"
#include "APBMission.h"
#include "APBCombat.h"
#include "APBCustomization.h"
#include "APBModelRegistry.h"
#include "APBSocial.h"
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
	std::string mission_id;
	std::string mission_title;
	int32_t mission_stage_index = 0;
	int32_t mission_stage_count = 0;
	std::string mission_status;
	std::string session_id;
	std::string district_id;
	int32_t district_players = 0;
};
class WorldService {
public:
	Catalog catalog;
	MissionScriptLibrary mission_scripts;
	ModelRegistry models;
	CustomizationService customization;
	LoginService login;
	WorldDirectory world_dir;
	DistrictRouter district_router;
	SocialService social;
	MailService mail;
	ConfigBlobStore config_blobs;
	DistrictStreamPlan stream_plan;
	SessionPhase phase = SessionPhase::Boot;
	std::optional<CharacterProfile> character;
	CharacterAppearance appearance;
	Inventory inventory;
	ArmasStore armas;
	AuctionHouse auction;
	ThreatSystem threat;
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
	bool PersistenceActive() const { return store.IsActive(); }
	/** Force-save all aggregates (accounts/character/auction/mail). */
	void SaveAllNow();
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
	bool JoinDistrict(const std::string& district_id, const std::string& player_name);
	bool JoinDistrictAsPeer(const std::string& session_id, const std::string& player_name);
	bool ExitDistrict();
	ArmasResult ArmasBuy(const std::string& item_id);
	AuctionResult AuctionList(const std::string& item_id, int32_t qty, int64_t price);
	AuctionResult AuctionBuy(int64_t listing_id, CharacterProfile& seller_profile, Inventory& seller_inv);
	bool StartMissionScript(const std::string& mission_id = "");
	void StartMission(const std::string& mission_id = "");
	bool AdvanceMission(double amount = 1.0);
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

private:
	void PersistAccounts();
	void PersistCharacter();
	void PersistAuction();
	void PersistMail();
	void TryLoadPersistedCharacter();
};
}
