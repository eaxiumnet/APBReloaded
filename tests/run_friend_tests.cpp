// run_friend_tests.cpp — M14 (D10) domain suite for apb::FriendsService.
// Compile (see tests\build_and_run.ps1):
//   cl /nologo /EHsc /std:c++17 /I"...\Domain" run_friend_tests.cpp ...\APBFriends.cpp
#include "../Source/APBReloaded/Domain/APBFriends.h"
#include <iostream>
#include <string>
#include <algorithm>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static bool Has(const std::vector<std::string>& v, const std::string& s) {
	return std::find(v.begin(), v.end(), s) != v.end();
}

// --- Test 1: request -> accept -> mutual friendship -------------------------
void TestRequestAccept() {
	FriendsService f;
	CHECK(f.SendRequest("alice", "bob") == FriendResult::Ok, "T1: alice requests bob");
	CHECK(Has(f.IncomingRequests("bob"), "alice"), "T1: bob has incoming");
	CHECK(Has(f.OutgoingRequests("alice"), "bob"), "T1: alice has outgoing");
	CHECK(!f.AreFriends("alice", "bob"), "T1: request alone is not friendship");
	CHECK(f.AcceptRequest("bob", "alice") == FriendResult::Ok, "T1: bob accepts");
	CHECK(f.AreFriends("alice", "bob") && f.AreFriends("bob", "alice"), "T1: mutual friends");
	CHECK(f.IncomingRequests("bob").empty(), "T1: request cleared on accept");
	CHECK(Has(f.FriendsOf("alice"), "bob") && Has(f.FriendsOf("bob"), "alice"), "T1: both lists updated");
}

// --- Test 2: self, duplicate, already-friends -------------------------------
void TestRequestGuards() {
	FriendsService f;
	CHECK(f.SendRequest("alice", "alice") == FriendResult::SelfTarget, "T2: no self-request");
	CHECK(f.SendRequest("", "bob") == FriendResult::InvalidArg, "T2: empty rejected");
	CHECK(f.SendRequest("alice", "bob") == FriendResult::Ok, "T2: first request ok");
	CHECK(f.SendRequest("alice", "bob") == FriendResult::AlreadyInvited, "T2: duplicate rejected");
	f.AcceptRequest("bob", "alice");
	CHECK(f.SendRequest("alice", "bob") == FriendResult::AlreadyFriends, "T2: already friends");
	// Accept/decline with no pending.
	CHECK(f.AcceptRequest("carol", "dave") == FriendResult::NoSuchInvite, "T2: accept no invite");
	CHECK(f.DeclineRequest("carol", "dave") == FriendResult::NoSuchInvite, "T2: decline no invite");
}

// --- Test 3: decline --------------------------------------------------------
void TestDecline() {
	FriendsService f;
	f.SendRequest("alice", "bob");
	CHECK(f.DeclineRequest("bob", "alice") == FriendResult::Ok, "T3: bob declines");
	CHECK(f.IncomingRequests("bob").empty(), "T3: request cleared");
	CHECK(!f.AreFriends("alice", "bob"), "T3: not friends after decline");
}

// --- Test 4: reciprocal request auto-accepts --------------------------------
void TestAutoAccept() {
	FriendsService f;
	CHECK(f.SendRequest("bob", "alice") == FriendResult::Ok, "T4: bob -> alice");
	// alice sends back while bob's request is pending -> instant friendship.
	CHECK(f.SendRequest("alice", "bob") == FriendResult::Ok, "T4: alice -> bob (reciprocal)");
	CHECK(f.AreFriends("alice", "bob"), "T4: auto-accepted into friendship");
	CHECK(f.IncomingRequests("alice").empty() && f.IncomingRequests("bob").empty(),
		"T4: all requests cleared");
}

// --- Test 5: mutual removal -------------------------------------------------
void TestRemove() {
	FriendsService f;
	f.SendRequest("alice", "bob"); f.AcceptRequest("bob", "alice");
	CHECK(f.RemoveFriend("dave", "erin") == FriendResult::NotFriends, "T5: remove non-friend");
	CHECK(f.RemoveFriend("alice", "bob") == FriendResult::Ok, "T5: alice removes bob");
	CHECK(!f.AreFriends("alice", "bob") && !f.AreFriends("bob", "alice"), "T5: mutual removal");
	CHECK(f.FriendsOf("alice").empty() && f.FriendsOf("bob").empty(), "T5: both lists cleared");
}

// --- Test 6: friends-list caps (sender + target) ----------------------------
void TestCaps() {
	FriendsService f;
	f.max_friends = 2;
	// Fill bob to cap (bob friends x, y).
	f.SendRequest("x", "bob"); f.AcceptRequest("bob", "x");
	f.SendRequest("y", "bob"); f.AcceptRequest("bob", "y");
	CHECK(f.FriendCount("bob") == 2, "T6: bob at cap");
	// alice (empty list) requests bob -> target full.
	CHECK(f.SendRequest("alice", "bob") == FriendResult::TargetListFull, "T6: target list full");
	// Fill alice to cap, then a new request from alice -> sender full.
	f.SendRequest("p", "alice"); f.AcceptRequest("alice", "p");
	f.SendRequest("q", "alice"); f.AcceptRequest("alice", "q");
	CHECK(f.SendRequest("alice", "z") == FriendResult::FriendsListFull, "T6: sender list full");
}

// --- Test 7: ignore basics --------------------------------------------------
void TestIgnoreBasics() {
	FriendsService f;
	CHECK(f.Ignore("alice", "alice") == FriendResult::SelfTarget, "T7: no self-ignore");
	CHECK(f.Ignore("alice", "troll") == FriendResult::Ok, "T7: ignore troll");
	CHECK(f.IsIgnoring("alice", "troll"), "T7: is ignoring");
	CHECK(Has(f.IgnoredBy("alice"), "troll"), "T7: listed in ignores");
	CHECK(f.Ignore("alice", "troll") == FriendResult::AlreadyIgnored, "T7: already ignored");
	// Ignore cap.
	f.max_ignores = 1;
	CHECK(f.Ignore("alice", "troll2") == FriendResult::IgnoreListFull, "T7: ignore list full");
}

// --- Test 8: friend/ignore mutual-exclusion invariants ----------------------
void TestExclusion() {
	FriendsService f;
	// (a) cannot friend someone you ignore.
	f.Ignore("alice", "bob");
	CHECK(f.SendRequest("alice", "bob") == FriendResult::YouIgnoreTarget, "T8: can't friend one you ignore");
	// (b) cannot send to someone who ignores you.
	f.Ignore("carol", "dave");
	CHECK(f.SendRequest("dave", "carol") == FriendResult::TargetIgnoresYou, "T8: target ignores you");
	// (c) cannot ignore an existing friend.
	f.SendRequest("erin", "frank"); f.AcceptRequest("frank", "erin");
	CHECK(f.Ignore("erin", "frank") == FriendResult::TargetIsFriend, "T8: can't ignore a friend");
	// Un-ignore then friending works.
	CHECK(f.Unignore("alice", "bob") == FriendResult::Ok, "T8: un-ignore bob");
	CHECK(f.SendRequest("alice", "bob") == FriendResult::Ok, "T8: can friend after un-ignore");
}

// --- Test 9: ignore cancels pending request; unignore guards ----------------
void TestIgnoreCancelsRequest() {
	FriendsService f;
	f.SendRequest("alice", "bob");           // pending alice -> bob
	CHECK(f.Ignore("bob", "alice") == FriendResult::Ok, "T9: bob ignores alice");
	CHECK(f.IncomingRequests("bob").empty(), "T9: pending request dropped by ignore");
	CHECK(f.Unignore("bob", "carol") == FriendResult::NotIgnored, "T9: un-ignore non-ignored");
	CHECK(f.Unignore("bob", "alice") == FriendResult::Ok, "T9: un-ignore ok");
	CHECK(!f.IsIgnoring("bob", "alice"), "T9: no longer ignoring");
}

// --- Test 10: presence ------------------------------------------------------
void TestPresence() {
	FriendsService f;
	f.SendRequest("alice", "bob"); f.AcceptRequest("bob", "alice");
	f.SendRequest("alice", "carol"); f.AcceptRequest("carol", "alice");
	CHECK(!f.IsOnline("bob"), "T10: bob offline by default");
	f.SetOnline("bob", true);
	CHECK(f.IsOnline("bob"), "T10: bob online");
	auto online = f.OnlineFriendsOf("alice");
	CHECK(online.size() == 1 && Has(online, "bob"), "T10: only bob shows online");
	f.SetOnline("bob", false);
	CHECK(f.OnlineFriendsOf("alice").empty(), "T10: bob offline -> none online");
}

// --- Test 11: persistence round-trip ----------------------------------------
void TestPersistence() {
	FriendsService a;
	a.SendRequest("alice", "bob");  a.AcceptRequest("bob", "alice");
	a.SendRequest("alice", "carol"); a.AcceptRequest("carol", "alice");
	a.Ignore("alice", "troll");
	a.Ignore("bob", "spammer");
	a.SendRequest("alice", "dave");  // pending (transient, must NOT persist)
	a.SetOnline("bob", true);        // presence (transient)

	const std::string json = a.SaveJson();
	CHECK(!json.empty(), "T11: SaveJson non-empty");

	FriendsService b;
	CHECK(b.LoadJson(json) == true, "T11: LoadJson ok");
	// Friendships restored symmetrically.
	CHECK(b.AreFriends("alice", "bob") && b.AreFriends("bob", "alice"), "T11: alice-bob restored");
	CHECK(b.AreFriends("alice", "carol") && b.AreFriends("carol", "alice"), "T11: alice-carol restored");
	CHECK(b.FriendCount("alice") == 2, "T11: alice has 2 friends");
	// Ignores restored (directional).
	CHECK(b.IsIgnoring("alice", "troll") && b.IsIgnoring("bob", "spammer"), "T11: ignores restored");
	CHECK(!b.IsIgnoring("troll", "alice"), "T11: ignore is directional");
	// Transient state NOT persisted.
	CHECK(b.IncomingRequests("dave").empty(), "T11: pending request not persisted");
	CHECK(!b.IsOnline("bob"), "T11: presence not persisted");
	// Idempotent.
	FriendsService c; c.LoadJson(json);
	CHECK(c.SaveJson() == a.SaveJson(), "T11: save/load/save idempotent");
	// Empty / no-content input.
	FriendsService d;
	CHECK(d.LoadJson("") == false, "T11: empty rejected");
	CHECK(d.LoadJson("{ \"friends\": [], \"ignores\": [] }") == false, "T11: empty content -> false");
}

int main() {
	std::cout << "=== APBFriendTests (M14 FriendsService) ===\n";
	TestRequestAccept();
	TestRequestGuards();
	TestDecline();
	TestAutoAccept();
	TestRemove();
	TestCaps();
	TestIgnoreBasics();
	TestExclusion();
	TestIgnoreCancelsRequest();
	TestPresence();
	TestPersistence();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
