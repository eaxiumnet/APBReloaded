// run_group_tests.cpp — M14 domain suite for apb::GroupService.
// Compile (see tests\build_and_run.ps1):
//   cl /nologo /EHsc /std:c++17 /I"...\Domain" run_group_tests.cpp ...\APBGroup.cpp
#include "../Source/APBReloaded/Domain/APBGroup.h"
#include <iostream>
#include <string>
#include <algorithm>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static bool Has(const std::vector<std::string>& v, const std::string& s) {
	return std::find(v.begin(), v.end(), s) != v.end();
}

// --- Test 1: create + auto-create-on-invite + accept -----------------------
void TestCreateInviteAccept() {
	GroupService g;
	std::string gid;
	CHECK(g.CreateGroup("alice", gid) == GroupResult::Ok, "T1: create ok");
	CHECK(!gid.empty() && g.GroupOf("alice") == gid, "T1: alice in her group");
	CHECK(g.IsLeader("alice"), "T1: alice is leader");
	CHECK(g.Size(gid) == 1, "T1: size 1");

	// Duplicate create for grouped player fails.
	std::string dummy;
	CHECK(g.CreateGroup("alice", dummy) == GroupResult::PlayerInGroup, "T1: no double group");

	CHECK(g.Invite("alice", "bob") == GroupResult::Ok, "T1: leader invites bob");
	CHECK(Has(g.PendingInvitesFor("bob"), gid), "T1: bob has pending invite");
	CHECK(!g.InGroup("bob"), "T1: invite alone does not join");
	CHECK(g.AcceptInvite("bob") == GroupResult::Ok, "T1: bob accepts");
	CHECK(g.GroupOf("bob") == gid && g.Size(gid) == 2, "T1: bob joined, size 2");
	CHECK(g.PendingInvitesFor("bob").empty(), "T1: accept clears invite");
}

// --- Test 2: invite auto-creates a group for a solo inviter ----------------
void TestInviteAutoCreates() {
	GroupService g;
	CHECK(g.Invite("carol", "dave") == GroupResult::Ok, "T2: solo invite auto-creates");
	std::string gid = g.GroupOf("carol");
	CHECK(!gid.empty() && g.IsLeader("carol"), "T2: carol became leader");
	CHECK(g.AcceptInvite("dave") == GroupResult::Ok && g.GroupOf("dave") == gid, "T2: dave joins carol's group");
}

// --- Test 3: capacity cap (default 4) --------------------------------------
void TestCapacity() {
	GroupService g;
	std::string gid;
	g.CreateGroup("l", gid);
	g.Invite("l", "m2"); g.AcceptInvite("m2");
	g.Invite("l", "m3"); g.AcceptInvite("m3");
	g.Invite("l", "m4"); g.AcceptInvite("m4");
	CHECK(g.Size(gid) == 4, "T3: group full at 4");
	CHECK(g.Invite("l", "m5") == GroupResult::GroupFull, "T3: invite rejected when full");
}

// --- Test 4: invite guards --------------------------------------------------
void TestInviteGuards() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	CHECK(g.Invite("alice", "alice") == GroupResult::SelfTarget, "T4: cannot invite self");
	CHECK(g.Invite("alice", "bob") == GroupResult::Ok, "T4: first invite ok");
	CHECK(g.Invite("alice", "bob") == GroupResult::AlreadyInvited, "T4: duplicate invite rejected");
	g.AcceptInvite("bob");
	// bob already grouped -> cannot be invited elsewhere
	std::string gid2; g.CreateGroup("carol", gid2);
	CHECK(g.Invite("carol", "bob") == GroupResult::TargetInGroup, "T4: cannot invite grouped player");
	// non-leader cannot invite
	CHECK(g.Invite("bob", "erin") == GroupResult::NotLeader, "T4: non-leader cannot invite");
}

// --- Test 5: decline + accept with no invite -------------------------------
void TestDeclineAndNoInvite() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob");
	CHECK(g.DeclineInvite("bob") == GroupResult::Ok, "T5: decline ok");
	CHECK(g.PendingInvitesFor("bob").empty(), "T5: invite cleared after decline");
	CHECK(g.AcceptInvite("bob") == GroupResult::NoSuchInvite, "T5: accept with no invite fails");
	CHECK(g.DeclineInvite("zoe") == GroupResult::NoSuchInvite, "T5: decline with no invite fails");
}

// --- Test 6: leave with leadership succession ------------------------------
void TestLeaveSuccession() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob");   g.AcceptInvite("bob");
	g.Invite("alice", "carol"); g.AcceptInvite("carol");
	CHECK(g.LeaderOf(gid) == "alice", "T6: alice leads");
	CHECK(g.Leave("alice") == GroupResult::Ok, "T6: leader leaves");
	CHECK(!g.InGroup("alice"), "T6: alice removed");
	CHECK(g.LeaderOf(gid) == "bob", "T6: leadership -> earliest joined (bob)");
	CHECK(g.Size(gid) == 2, "T6: size 2 after leave");
	// non-member leave
	CHECK(g.Leave("nobody") == GroupResult::PlayerNotInGroup, "T6: non-member leave fails");
}

// --- Test 7: last member leaving disbands the group ------------------------
void TestLeaveDisbands() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	CHECK(g.Leave("alice") == GroupResult::Ok, "T7: solo leader leaves");
	CHECK(g.Find(gid) == nullptr, "T7: emptied group disbanded");
	CHECK(!g.InGroup("alice"), "T7: alice no longer grouped");
}

// --- Test 8: kick guards ----------------------------------------------------
void TestKick() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob"); g.AcceptInvite("bob");
	CHECK(g.Kick("bob", "alice") == GroupResult::NotLeader, "T8: non-leader cannot kick");
	CHECK(g.Kick("alice", "alice") == GroupResult::SelfTarget, "T8: leader cannot kick self");
	CHECK(g.Kick("alice", "ghost") == GroupResult::NotMember, "T8: cannot kick non-member");
	CHECK(g.Kick("alice", "bob") == GroupResult::Ok, "T8: leader kicks bob");
	CHECK(!g.InGroup("bob") && g.Size(gid) == 1, "T8: bob removed");
}

// --- Test 9: disband + transfer leader -------------------------------------
void TestDisbandTransfer() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob"); g.AcceptInvite("bob");
	CHECK(g.TransferLeader("bob", "alice") == GroupResult::NotLeader, "T9: only leader transfers");
	CHECK(g.TransferLeader("alice", "ghost") == GroupResult::NotMember, "T9: transfer to non-member fails");
	CHECK(g.TransferLeader("alice", "bob") == GroupResult::Ok, "T9: transfer ok");
	CHECK(g.LeaderOf(gid) == "bob" && g.IsLeader("bob") && !g.IsLeader("alice"), "T9: bob now leads");
	CHECK(g.Disband("alice") == GroupResult::NotLeader, "T9: ex-leader cannot disband");
	CHECK(g.Disband("bob") == GroupResult::Ok, "T9: leader disbands");
	CHECK(g.Find(gid) == nullptr && !g.InGroup("alice") && !g.InGroup("bob"), "T9: all cleared");
}

// --- Test 10: ready state + AllReady + mission assign ----------------------
void TestReadyAndMission() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob"); g.AcceptInvite("bob");
	CHECK(!g.AllReady(gid), "T10: not all ready initially");
	CHECK(g.SetReady("alice", true) == GroupResult::Ok, "T10: alice ready");
	CHECK(!g.AllReady(gid), "T10: still not all ready");
	CHECK(g.SetReady("bob", true) == GroupResult::Ok, "T10: bob ready");
	CHECK(g.AllReady(gid), "T10: all ready now");
	CHECK(g.SetReady("ghost", true) == GroupResult::PlayerNotInGroup, "T10: non-member cannot ready");

	CHECK(g.AssignMission("bob", "MIS-1") == GroupResult::NotLeader, "T10: only leader assigns mission");
	CHECK(g.AssignMission("alice", "MIS-1") == GroupResult::Ok, "T10: leader assigns mission");
	CHECK(g.Find(gid)->mission_id == "MIS-1", "T10: mission recorded");
	CHECK(g.ClearMission("alice") == GroupResult::Ok, "T10: clear mission");
	CHECK(g.Find(gid)->mission_id.empty(), "T10: mission cleared");
}

// --- Test 11: disband/leave clears others' pending invites ------------------
void TestInviteCleanupOnDisband() {
	GroupService g;
	std::string gid; g.CreateGroup("alice", gid);
	g.Invite("alice", "bob"); // bob has a pending invite to alice's group
	CHECK(Has(g.PendingInvitesFor("bob"), gid), "T11: bob pending pre-disband");
	CHECK(g.Disband("alice") == GroupResult::Ok, "T11: disband");
	CHECK(g.PendingInvitesFor("bob").empty(), "T11: pending invite purged on disband");
	CHECK(g.AcceptInvite("bob") == GroupResult::NoSuchInvite, "T11: bob cannot accept vanished group");
}

int main() {
	std::cout << "APB Group tests (M14)\n";
	TestCreateInviteAccept();
	TestInviteAutoCreates();
	TestCapacity();
	TestInviteGuards();
	TestDeclineAndNoInvite();
	TestLeaveSuccession();
	TestLeaveDisbands();
	TestKick();
	TestDisbandTransfer();
	TestReadyAndMission();
	TestInviteCleanupOnDisband();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
