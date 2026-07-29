#pragma once
// APBGroup.h — M14 (D10): pure-C++17 mission-group / party domain.
// No platform/UE headers — unit-testable in isolation like ChatService/TicketService.
//
// Models APB's real-time mission group ("team"): up to 4 players (per
// MissionScriptDef.group_max), one leader, leader-driven invite/accept/decline,
// leave/kick/disband, leadership transfer, per-member ready state, and a single
// assigned mission id (the shared-mission-queue hook M11/M14 expects).
//
// Integration seam (kept decoupled — services stay independent Domain units):
//   GroupService.GroupOf(player) -> the group_id string that the UE layer sets on
//   ChatService via SetGroup(player, group_id), so the Group chat channel routes
//   to exactly this group's members. GroupService never depends on ChatService.
//
// A player may belong to at most one group. Group ids are deterministic
// ("GRP-<n>") for reproducible tests; join order is tracked so leadership passes
// to the earliest-joined remaining member when a leader departs.
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace apb {

// Outcome of every mutating GroupService call. Ok = applied; everything else is
// a precondition failure and leaves state unchanged.
enum class GroupResult {
	Ok,
	InvalidArg,       // empty player/mission name
	SelfTarget,       // cannot invite/kick/transfer to self
	PlayerInGroup,    // actor is already in a group where that is disallowed
	PlayerNotInGroup, // actor must be grouped for this action
	TargetInGroup,    // invitee already belongs to another group
	NotLeader,        // action requires group leadership
	NotMember,        // target is not a member of the actor's group
	GroupFull,        // group already at max_size
	NoSuchGroup,      // referenced group id does not exist
	NoSuchInvite,     // accept/decline with no matching pending invite
	AlreadyInvited    // invitee already has a pending invite to this group
};

struct GroupMember {
	std::string player;
	bool ready = false;   // ready-up for the shared mission queue
	int64_t joined_seq = 0; // monotonic join order (leadership succession)
};

struct Group {
	std::string id;
	std::string leader;
	std::vector<GroupMember> members; // includes the leader
	int32_t max_size = 4;
	std::string mission_id; // assigned mission ("" = none)

	const GroupMember* FindMember(const std::string& player) const;
	GroupMember* FindMember(const std::string& player);
	bool IsFull() const { return (int32_t)members.size() >= max_size; }
};

class GroupService {
public:
	// Group size cap for newly created groups (APB standard = 4).
	int32_t default_max_size = 4;

	// --- creation & membership ------------------------------------------------
	// Creates a solo group led by `leader`. Fails if leader is empty or already grouped.
	GroupResult CreateGroup(const std::string& leader, std::string& out_group_id);
	// Leader invites a player. If the inviter has no group yet, one is auto-created
	// with the inviter as leader (APB: inviting forms a group). Only a leader may invite.
	GroupResult Invite(const std::string& inviter, const std::string& invitee);
	// Invitee joins the group they were invited to (must still exist + have room).
	// Clears all of the invitee's other pending invites on success.
	GroupResult AcceptInvite(const std::string& invitee);
	// Invitee declines their (most recent) pending invite.
	GroupResult DeclineInvite(const std::string& invitee);
	// Player leaves their group. If the leader leaves and members remain, leadership
	// passes to the earliest-joined remaining member; an emptied group is disbanded.
	GroupResult Leave(const std::string& player);
	// Leader removes another member (not self — use Leave/Disband/Transfer for that).
	GroupResult Kick(const std::string& leader, const std::string& target);
	// Leader dissolves the whole group; all memberships and the group's invites clear.
	GroupResult Disband(const std::string& leader);
	// Leader hands leadership to an existing member.
	GroupResult TransferLeader(const std::string& leader, const std::string& target);

	// --- shared mission queue hooks -------------------------------------------
	GroupResult SetReady(const std::string& player, bool ready);
	GroupResult AssignMission(const std::string& leader, const std::string& mission_id);
	GroupResult ClearMission(const std::string& leader);

	// --- queries (const, never mutate) ----------------------------------------
	bool InGroup(const std::string& player) const;
	std::string GroupOf(const std::string& player) const;        // "" if solo
	std::string LeaderOf(const std::string& group_id) const;     // "" if none
	bool IsLeader(const std::string& player) const;
	const Group* Find(const std::string& group_id) const;
	int32_t Size(const std::string& group_id) const;             // 0 if none
	bool AllReady(const std::string& group_id) const;            // false for empty/unknown
	std::vector<std::string> MembersOf(const std::string& group_id) const;
	std::vector<std::string> PendingInvitesFor(const std::string& invitee) const; // group ids

private:
	std::unordered_map<std::string, Group> groups_;              // id -> group
	std::unordered_map<std::string, std::string> player_group_;  // player -> group id
	std::unordered_map<std::string, std::vector<std::string>> invites_; // invitee -> group ids
	int64_t next_group_seq_ = 1;
	int64_t next_join_seq_ = 1;

	Group* FindMut(const std::string& group_id);
	void ClearInvite(const std::string& invitee, const std::string& group_id);
	void AddMember(Group& g, const std::string& player);
	void DisbandInternal(Group& g);
};

} // namespace apb
