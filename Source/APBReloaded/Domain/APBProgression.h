#pragma once
// APBProgression.h — M15 (brief #14): pure-C++17 economy & progression Domain service.
// No UE/platform headers — unit-testable in isolation like the other Domain services.
//
// Scope handled here (the parts NOT already owned by APBThreat.h):
//   1. ProgressionCatalog — contacts (mission-givers) + activity/weapon roles parsed from
//      Content/Data/contacts_lore.json + roles.json. Only fields present in the apbdb-seeded
//      data are modelled; nothing is invented.
//   2. LevelLadder — cumulative-standing -> contact level mapping. APB contacts unlock items
//      at discrete levels; the seeded data does not publish per-level standing tables, so the
//      thresholds are tunable recreation defaults (flagged as such), not invented facts.
//   3. CharacterProgress — the server-authoritative per-character state: contact standing +
//      role XP (mirrors the replicated AAPBPlayerState progression fields).
//   4. Reward + unlock rules — mission reward scaled by the threat reward multiplier (ties to
//      APBThreat.h's ThreatTier.reward_multiplier), and contact-level-gated item unlocks.
//
// Threat tiers/opposition/reward multipliers themselves live in APBThreat.h (ThreatSystem);
// this service consumes that multiplier rather than re-deriving it.
#include "APBTypes.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace apb {

// A contact NPC (mission-giver). Standing earned with a contact unlocks items at levels.
struct ContactDef {
	std::string id;        // e.g. "Financial_C1"
	std::string title;     // e.g. "Double-B"
	std::string district;  // derived from the id prefix: "Financial" / "Waterfront" / ...
};

// An activity/weapon role progression track. Seeded from apbdb (roles.json) and merged with
// the full retail roster + canonical display names/descriptions from PlayerRoles.INT
// (player_roles.json).
struct RoleDef {
	std::string id;
	std::string name;
	std::string description; // retail flavor/reward text (empty when not published)
};

// Cumulative-standing -> contact level ladder (defined below); forward-declared so
// ProgressionCatalog can return one by value.
class LevelLadder;

// Contacts + roles reference data, parsed from the shipped JSON.
class ProgressionCatalog {
public:
	std::unordered_map<std::string, ContactDef> contacts;
	std::unordered_map<std::string, RoleDef> roles;
	// Real retail per-contact level counts, keyed by NORMALIZED contact id (see
	// NormalizeContactId) so the zero-padded ContactLevels ids ("Financial_C01")
	// resolve against the lore ids ("Financial_C1"). Parsed from contact_levels.json,
	// which is extracted from the retail ContactLevels.INT by
	// tools/scripts/extract_contact_levels.ps1.
	std::unordered_map<std::string, int32_t> contact_max_level;

	// Additive parsers (merge by id). Return true if at least one entry was parsed.
	bool LoadContactsFromText(const std::string& json);
	bool LoadRolesFromText(const std::string& json);
	bool LoadContactLevelsFromText(const std::string& json);
	bool LoadContactsFromFile(const std::string& path);
	bool LoadRolesFromFile(const std::string& path);
	bool LoadContactLevelsFromFile(const std::string& path);

	const ContactDef* FindContact(const std::string& id) const {
		auto it = contacts.find(id);
		return it == contacts.end() ? nullptr : &it->second;
	}
	const RoleDef* FindRole(const std::string& id) const {
		auto it = roles.find(id);
		return it == roles.end() ? nullptr : &it->second;
	}
	std::vector<const ContactDef*> ContactsInDistrict(const std::string& district) const;
	int32_t ContactCount() const { return (int32_t)contacts.size(); }
	int32_t RoleCount() const { return (int32_t)roles.size(); }
	int32_t ContactLevelCount() const { return (int32_t)contact_max_level.size(); }

	// Real retail max level for a contact (highest Level<NN> in ContactLevels.INT), or 0
	// when the contact has no entry. Accepts either the padded or lore id form.
	int32_t ContactMaxLevel(const std::string& contact_id) const;

	// Standing ladder sized to the contact's real retail level count when known, else the
	// tunable DefaultContactLadder. Thresholds stay recreation defaults (no retail table).
	LevelLadder LadderForContact(const std::string& contact_id) const;
};

// Canonicalize a contact id by stripping leading zeros from a trailing digit run so the
// zero-padded ContactLevels ids ("Financial_C01") match the lore ids ("Financial_C1").
std::string NormalizeContactId(const std::string& id);


// Derive the district token from a contact id ("Financial_C1" -> "Financial").
std::string DistrictFromContactId(const std::string& id);

// Cumulative-standing -> level ladder. thresholds[i] is the standing required to reach
// level i (thresholds[0] == 0). Monotonically increasing.
class LevelLadder {
public:
	std::vector<int64_t> thresholds;

	// Recreation-default contact ladder (levels 0..15). Tunable — retune when a reference
	// per-level standing table is recovered from the retail build.
	static LevelLadder DefaultContactLadder();

	// Ladder sized to an explicit max level (levels 0..max_level), same growth curve as the
	// default. Used to honor each contact's real retail level count from ContactLevels.INT.
	static LevelLadder ContactLadderWithMaxLevel(int32_t max_level);

	int32_t MaxLevel() const { return thresholds.empty() ? 0 : (int32_t)thresholds.size() - 1; }
	int32_t LevelFor(int64_t standing) const;      // highest level whose threshold <= standing
	int64_t StandingForLevel(int32_t level) const; // clamped to [0, MaxLevel]
};

// Server-authoritative per-character progression state.
struct CharacterProgress {
	std::unordered_map<std::string, int64_t> contact_standing; // contact_id -> cumulative standing
	std::unordered_map<std::string, int64_t> role_xp;          // role_id    -> cumulative xp

	int64_t ContactStanding(const std::string& contact_id) const;
	int64_t RoleXp(const std::string& role_id) const;

	// Add positive standing/xp (negatives ignored); returns the new cumulative total.
	int64_t AddContactStanding(const std::string& contact_id, int64_t amount);
	int64_t AddRoleXp(const std::string& role_id, int64_t amount);

	int32_t ContactLevel(const std::string& contact_id, const LevelLadder& ladder) const;
};

// Reward for a completed mission, after threat scaling.
struct MissionReward {
	int64_t cash = 0;
	int64_t standing = 0;
	int64_t role_xp = 0;
};

// Scale base cash + standing by the active threat tier's reward multiplier. Role XP is
// activity-based and NOT threat-scaled (matches APB: role progression is flat per action).
// Rounds to nearest; never negative.
MissionReward ComputeMissionReward(int64_t base_cash, int64_t base_standing,
	int64_t base_role_xp, double threat_reward_multiplier);

// An item purchasable only once a contact reaches the required level.
struct ContactUnlock {
	std::string contact_id;
	int32_t required_level = 0;
	std::string item_id;
};

// True when the character's standing with the unlock's contact reaches the required level.
bool IsUnlocked(const CharacterProgress& progress, const LevelLadder& ladder,
	const ContactUnlock& unlock);

// Every item unlocked for the character given a list of contact-gated unlocks.
std::vector<std::string> UnlockedItems(const CharacterProgress& progress,
	const LevelLadder& ladder, const std::vector<ContactUnlock>& unlocks);

// Cash sink: attempt to spend from a balance. Returns true and debits on success; leaves the
// balance untouched and returns false when funds are insufficient or the cost is negative.
bool TrySpend(int64_t& balance, int64_t cost);

} // namespace apb
