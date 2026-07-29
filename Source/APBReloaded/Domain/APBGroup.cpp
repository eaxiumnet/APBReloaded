// APBGroup.cpp — M14 (D10) mission-group / party domain implementation.
// Pure C++17, no platform/UE deps (mirrors APBChat.cpp).
#include "APBGroup.h"
#include <algorithm>

namespace apb {

// --- Group helpers -----------------------------------------------------------

const GroupMember* Group::FindMember(const std::string& player) const {
	for (const auto& m : members) if (m.player == player) return &m;
	return nullptr;
}
GroupMember* Group::FindMember(const std::string& player) {
	for (auto& m : members) if (m.player == player) return &m;
	return nullptr;
}

// --- private helpers ---------------------------------------------------------

Group* GroupService::FindMut(const std::string& group_id) {
	auto it = groups_.find(group_id);
	return it == groups_.end() ? nullptr : &it->second;
}

void GroupService::ClearInvite(const std::string& invitee, const std::string& group_id) {
	auto it = invites_.find(invitee);
	if (it == invites_.end()) return;
	auto& v = it->second;
	v.erase(std::remove(v.begin(), v.end(), group_id), v.end());
	if (v.empty()) invites_.erase(it);
}

void GroupService::AddMember(Group& g, const std::string& player) {
	GroupMember m;
	m.player = player;
	m.ready = false;
	m.joined_seq = next_join_seq_++;
	g.members.push_back(m);
	player_group_[player] = g.id;
}

// Removes the group from all indices. `g` must be a live entry in groups_.
void GroupService::DisbandInternal(Group& g) {
	const std::string id = g.id;
	for (const auto& m : g.members) player_group_.erase(m.player);
	// Drop any pending invites that point at this group.
	for (auto it = invites_.begin(); it != invites_.end();) {
		auto& v = it->second;
		v.erase(std::remove(v.begin(), v.end(), id), v.end());
		if (v.empty()) it = invites_.erase(it); else ++it;
	}
	groups_.erase(id);
}

// --- creation & membership ---------------------------------------------------

GroupResult GroupService::CreateGroup(const std::string& leader, std::string& out_group_id) {
	if (leader.empty()) return GroupResult::InvalidArg;
	if (player_group_.count(leader)) return GroupResult::PlayerInGroup;
	Group g;
	g.id = "GRP-" + std::to_string(next_group_seq_++);
	g.leader = leader;
	g.max_size = default_max_size;
	groups_[g.id] = g;
	AddMember(groups_[g.id], leader);
	out_group_id = g.id;
	return GroupResult::Ok;
}

GroupResult GroupService::Invite(const std::string& inviter, const std::string& invitee) {
	if (inviter.empty() || invitee.empty()) return GroupResult::InvalidArg;
	if (inviter == invitee) return GroupResult::SelfTarget;
	if (player_group_.count(invitee)) return GroupResult::TargetInGroup;

	// Auto-create a group for a solo inviter (APB: inviting forms a group).
	std::string gid = GroupOf(inviter);
	if (gid.empty()) {
		std::string created;
		GroupResult r = CreateGroup(inviter, created);
		if (r != GroupResult::Ok) return r;
		gid = created;
	}
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != inviter) return GroupResult::NotLeader;
	if (g->IsFull()) return GroupResult::GroupFull;

	auto& pend = invites_[invitee];
	if (std::find(pend.begin(), pend.end(), gid) != pend.end())
		return GroupResult::AlreadyInvited;
	pend.push_back(gid);
	return GroupResult::Ok;
}

GroupResult GroupService::AcceptInvite(const std::string& invitee) {
	if (invitee.empty()) return GroupResult::InvalidArg;
	if (player_group_.count(invitee)) return GroupResult::PlayerInGroup;
	auto it = invites_.find(invitee);
	if (it == invites_.end() || it->second.empty()) return GroupResult::NoSuchInvite;

	// Honour the most recent still-valid invite; skip groups that vanished.
	std::string gid;
	Group* g = nullptr;
	while (!it->second.empty()) {
		gid = it->second.back();
		g = FindMut(gid);
		if (g) break;
		it->second.pop_back();
	}
	if (!g) { invites_.erase(it); return GroupResult::NoSuchGroup; }
	if (g->IsFull()) return GroupResult::GroupFull;

	AddMember(*g, invitee);
	invites_.erase(invitee); // joining clears all other pending invites
	return GroupResult::Ok;
}

GroupResult GroupService::DeclineInvite(const std::string& invitee) {
	if (invitee.empty()) return GroupResult::InvalidArg;
	auto it = invites_.find(invitee);
	if (it == invites_.end() || it->second.empty()) return GroupResult::NoSuchInvite;
	it->second.pop_back();
	if (it->second.empty()) invites_.erase(it);
	return GroupResult::Ok;
}

GroupResult GroupService::Leave(const std::string& player) {
	if (player.empty()) return GroupResult::InvalidArg;
	std::string gid = GroupOf(player);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;

	const bool was_leader = (g->leader == player);
	auto& mv = g->members;
	mv.erase(std::remove_if(mv.begin(), mv.end(),
		[&](const GroupMember& m) { return m.player == player; }), mv.end());
	player_group_.erase(player);

	if (mv.empty()) { DisbandInternal(*g); return GroupResult::Ok; }
	if (was_leader) {
		// Succession: earliest-joined remaining member.
		auto oldest = std::min_element(mv.begin(), mv.end(),
			[](const GroupMember& a, const GroupMember& b) { return a.joined_seq < b.joined_seq; });
		g->leader = oldest->player;
	}
	return GroupResult::Ok;
}

GroupResult GroupService::Kick(const std::string& leader, const std::string& target) {
	if (leader.empty() || target.empty()) return GroupResult::InvalidArg;
	if (leader == target) return GroupResult::SelfTarget;
	std::string gid = GroupOf(leader);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != leader) return GroupResult::NotLeader;
	if (!g->FindMember(target)) return GroupResult::NotMember;

	auto& mv = g->members;
	mv.erase(std::remove_if(mv.begin(), mv.end(),
		[&](const GroupMember& m) { return m.player == target; }), mv.end());
	player_group_.erase(target);
	// Leader cannot kick self, so the group is never emptied here.
	return GroupResult::Ok;
}

GroupResult GroupService::Disband(const std::string& leader) {
	if (leader.empty()) return GroupResult::InvalidArg;
	std::string gid = GroupOf(leader);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != leader) return GroupResult::NotLeader;
	DisbandInternal(*g);
	return GroupResult::Ok;
}

GroupResult GroupService::TransferLeader(const std::string& leader, const std::string& target) {
	if (leader.empty() || target.empty()) return GroupResult::InvalidArg;
	if (leader == target) return GroupResult::SelfTarget;
	std::string gid = GroupOf(leader);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != leader) return GroupResult::NotLeader;
	if (!g->FindMember(target)) return GroupResult::NotMember;
	g->leader = target;
	return GroupResult::Ok;
}

// --- shared mission queue hooks ----------------------------------------------

GroupResult GroupService::SetReady(const std::string& player, bool ready) {
	if (player.empty()) return GroupResult::InvalidArg;
	std::string gid = GroupOf(player);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	GroupMember* m = g->FindMember(player);
	if (!m) return GroupResult::NotMember;
	m->ready = ready;
	return GroupResult::Ok;
}

GroupResult GroupService::AssignMission(const std::string& leader, const std::string& mission_id) {
	if (leader.empty() || mission_id.empty()) return GroupResult::InvalidArg;
	std::string gid = GroupOf(leader);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != leader) return GroupResult::NotLeader;
	g->mission_id = mission_id;
	return GroupResult::Ok;
}

GroupResult GroupService::ClearMission(const std::string& leader) {
	if (leader.empty()) return GroupResult::InvalidArg;
	std::string gid = GroupOf(leader);
	if (gid.empty()) return GroupResult::PlayerNotInGroup;
	Group* g = FindMut(gid);
	if (!g) return GroupResult::NoSuchGroup;
	if (g->leader != leader) return GroupResult::NotLeader;
	g->mission_id.clear();
	return GroupResult::Ok;
}

// --- queries -----------------------------------------------------------------

bool GroupService::InGroup(const std::string& player) const {
	return player_group_.count(player) != 0;
}

std::string GroupService::GroupOf(const std::string& player) const {
	auto it = player_group_.find(player);
	return it == player_group_.end() ? std::string() : it->second;
}

std::string GroupService::LeaderOf(const std::string& group_id) const {
	const Group* g = Find(group_id);
	return g ? g->leader : std::string();
}

bool GroupService::IsLeader(const std::string& player) const {
	std::string gid = GroupOf(player);
	if (gid.empty()) return false;
	const Group* g = Find(gid);
	return g && g->leader == player;
}

const Group* GroupService::Find(const std::string& group_id) const {
	auto it = groups_.find(group_id);
	return it == groups_.end() ? nullptr : &it->second;
}

int32_t GroupService::Size(const std::string& group_id) const {
	const Group* g = Find(group_id);
	return g ? (int32_t)g->members.size() : 0;
}

bool GroupService::AllReady(const std::string& group_id) const {
	const Group* g = Find(group_id);
	if (!g || g->members.empty()) return false;
	for (const auto& m : g->members) if (!m.ready) return false;
	return true;
}

std::vector<std::string> GroupService::MembersOf(const std::string& group_id) const {
	std::vector<std::string> out;
	const Group* g = Find(group_id);
	if (g) for (const auto& m : g->members) out.push_back(m.player);
	return out;
}

std::vector<std::string> GroupService::PendingInvitesFor(const std::string& invitee) const {
	auto it = invites_.find(invitee);
	return it == invites_.end() ? std::vector<std::string>() : it->second;
}

} // namespace apb
