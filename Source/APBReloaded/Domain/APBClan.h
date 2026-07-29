#pragma once
// APBClan.h — M14 (D10): pure-C++17 clan domain (ranks, permissions, invites,
// membership, leadership, MOTD). No platform/UE headers — unit-testable in
// isolation like GroupService/ChatService.
//
// Grounded 1:1 on retail APB strings (APBUserInterface.int / UIChat.int):
//   - single leader; "You_are_now_the_clan_leader", transfer/disband on leave
//   - "ClanLeaveLeaderFail": leader CANNOT leave — must disband or transfer first
//   - permission-gated invites ("CannotTeamInvite_NoPermission") -> ranks carry perms
//   - single-faction clans ("...cannot send clan invites to players of the opposite
//     Faction") -> Enforcer and Criminal never share a clan
//   - one clan per character ("<character>_is_already_in_a_clan"), "Your_clan_is_full"
//   - "Clan_message_of_the_day" -> per-clan MOTD; "ClanRank" -> named member ranks
//
// Integration seam (decoupled): ClanService.ClanOf(player) yields the clan id the UE
// layer sets on ChatService via SetClan(player, clan_id) so the Clan chat channel
// routes to clan members. ClanService never depends on ChatService.
#include "APBTypes.h"   // apb::Faction
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace apb {

// Clan rank permission bitmask (plain uint32_t constants — no operator boilerplate).
// The leader implicitly holds All regardless of rank.
namespace ClanPerm {
	enum : uint32_t {
		None        = 0u,
		Invite      = 1u << 0, // send clan invites
		Kick        = 1u << 1, // remove members of lower rank
		EditMotd    = 1u << 2, // set Message of the Day
		ManageRanks = 1u << 3, // add ranks + assign member ranks
		All         = 0xFFFFFFFFu
	};
}

enum class ClanResult {
	Ok,
	InvalidArg,        // empty id/name/tag/player
	SelfTarget,        // cannot invite/kick self
	NameTaken,         // clan name or tag already used (case-insensitive)
	PlayerInClan,      // actor already belongs to a clan
	TargetInClan,      // invitee already belongs to a clan
	NotInClan,         // actor must be in a clan for this action
	NoSuchClan,        // referenced clan id does not exist
	NotLeader,         // action requires leadership (transfer/disband)
	NoPermission,      // actor's rank lacks the required permission
	ClanFull,          // at max_members
	WrongFaction,      // invitee faction != clan faction
	NoSuchInvite,      // accept/decline with no matching pending invite
	AlreadyInvited,    // invitee already has a pending invite to this clan
	NoSuchMember,      // target is not a member of this clan
	NoSuchRank,        // rank index out of range
	LeaderMustTransferOrDisband // leader tried to Leave()
};

struct ClanRank {
	std::string name;
	uint32_t permissions = ClanPerm::None;
};

struct ClanMember {
	std::string player;
	int32_t rank_index = 0;  // index into Clan::ranks
	int64_t joined_seq = 0;  // monotonic join order
};

struct Clan {
	std::string id, name, tag;
	Faction faction = Faction::Criminal;
	std::string leader;
	std::string motd;
	std::vector<ClanRank> ranks;     // rank[0] = highest (default "Officer")
	std::vector<ClanMember> members; // includes the leader

	const ClanMember* FindMember(const std::string& player) const;
	ClanMember* FindMember(const std::string& player);
	bool IsFull(int32_t cap) const { return (int32_t)members.size() >= cap; }
};

class ClanService {
public:
	int32_t max_members = 100; // APB clans are large; configurable cap

	// --- lifecycle & membership ----------------------------------------------
	// Creates a clan led by `leader` (single-faction). Fails if id/name/tag empty,
	// name/tag already taken, or leader already in a clan. Seeds default ranks.
	ClanResult CreateClan(const std::string& id, const std::string& name, const std::string& tag,
		Faction faction, const std::string& leader);
	// Invite (requires ClanPerm::Invite). `invitee_faction` must equal the clan's faction.
	ClanResult Invite(const std::string& inviter, const std::string& invitee, Faction invitee_faction);
	ClanResult AcceptInvite(const std::string& invitee);
	ClanResult DeclineInvite(const std::string& invitee);
	// Leave: a non-leader departs. The LEADER cannot leave — must Transfer or Disband first.
	ClanResult Leave(const std::string& player);
	// Kick (requires ClanPerm::Kick). Cannot target self or the leader.
	ClanResult Kick(const std::string& actor, const std::string& target);
	ClanResult Disband(const std::string& leader);
	ClanResult TransferLeader(const std::string& leader, const std::string& target);

	// --- ranks & MOTD ---------------------------------------------------------
	ClanResult SetMotd(const std::string& actor, const std::string& text);       // ClanPerm::EditMotd
	ClanResult AddRank(const std::string& actor, const std::string& name, uint32_t permissions); // ManageRanks
	ClanResult SetMemberRank(const std::string& actor, const std::string& target, int32_t rank_index); // ManageRanks

	// --- persistence (pure string round-trip) ---------------------------------
	// Serializes all DURABLE clan state (clans, ranks, members, next join seq) to a
	// JSON document. Pending invites are transient session state and are NOT persisted.
	// File IO (clans.json read/write) lives in the UE/store layer, keeping Domain pure.
	std::string SaveJson() const;
	// Replaces ALL state from a SaveJson() document. Rebuilds the player->clan index
	// from member lists and restores next_join_seq_. Returns false on empty/garbage input.
	bool LoadJson(const std::string& text);

	// --- queries (const) ------------------------------------------------------
	bool InClan(const std::string& player) const;
	std::string ClanOf(const std::string& player) const;          // "" if none
	std::string LeaderOf(const std::string& clan_id) const;
	bool IsLeader(const std::string& player) const;
	const Clan* Find(const std::string& clan_id) const;
	int32_t Size(const std::string& clan_id) const;
	std::string RankOf(const std::string& player) const;          // rank name, "" if none
	bool HasPermission(const std::string& player, uint32_t perm) const;
	std::string MotdOf(const std::string& clan_id) const;
	std::vector<std::string> MembersOf(const std::string& clan_id) const;
	std::vector<std::string> PendingInvitesFor(const std::string& invitee) const; // clan ids

private:
	std::unordered_map<std::string, Clan> clans_;                 // id -> clan
	std::unordered_map<std::string, std::string> player_clan_;    // player -> clan id
	std::unordered_map<std::string, std::vector<std::string>> invites_; // invitee -> clan ids
	int64_t next_join_seq_ = 1;

	Clan* FindMut(const std::string& clan_id);
	void ClearInvite(const std::string& invitee, const std::string& clan_id);
	bool NameOrTagTaken(const std::string& name, const std::string& tag) const;
	void AddMemberAtLowestRank(Clan& c, const std::string& player);
	void DisbandInternal(Clan& c);
	static uint32_t MemberPerms(const Clan& c, const std::string& player);
};

} // namespace apb
