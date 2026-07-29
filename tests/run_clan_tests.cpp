// run_clan_tests.cpp — M14 (D10) domain suite for apb::ClanService.
// Compile (see tests\build_and_run.ps1):
//   cl /nologo /EHsc /std:c++17 /I"...\Domain" run_clan_tests.cpp ...\APBClan.cpp
#include "../Source/APBReloaded/Domain/APBClan.h"
#include <iostream>
#include <string>
#include <algorithm>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if(!(cond)){ std::cerr<<"FAIL: "<<msg<<"\n"; ++fails; } else { std::cout<<"PASS: "<<msg<<"\n"; } } while(0)

static bool Has(const std::vector<std::string>& v, const std::string& s) {
	return std::find(v.begin(), v.end(), s) != v.end();
}

// --- Test 1: create + duplicate name/tag rejection --------------------------
void TestCreate() {
	ClanService cs;
	CHECK(cs.CreateClan("C1", "The Praetorians", "PRAE", Faction::Enforcer, "alice") == ClanResult::Ok,
		"T1: create ok");
	CHECK(cs.InClan("alice") && cs.ClanOf("alice") == "C1", "T1: leader is member");
	CHECK(cs.IsLeader("alice"), "T1: alice is leader");
	CHECK(cs.Size("C1") == 1, "T1: size 1");
	CHECK(cs.LeaderOf("C1") == "alice", "T1: leader query");
	CHECK(cs.RankOf("alice") == "Officer", "T1: leader seeded at rank 0 (Officer)");

	// Empty args rejected.
	CHECK(cs.CreateClan("", "x", "x", Faction::Enforcer, "z") == ClanResult::InvalidArg, "T1: empty id");
	CHECK(cs.CreateClan("C2", "", "x", Faction::Enforcer, "z") == ClanResult::InvalidArg, "T1: empty name");

	// Duplicate id.
	CHECK(cs.CreateClan("C1", "Other", "OTH", Faction::Enforcer, "bob") == ClanResult::NameTaken,
		"T1: duplicate id rejected");
	// Duplicate name (case-insensitive).
	CHECK(cs.CreateClan("C3", "the praetorians", "XYZ", Faction::Enforcer, "bob") == ClanResult::NameTaken,
		"T1: duplicate name (ci) rejected");
	// Duplicate tag (case-insensitive).
	CHECK(cs.CreateClan("C4", "Different Name", "prae", Faction::Enforcer, "bob") == ClanResult::NameTaken,
		"T1: duplicate tag (ci) rejected");
	// Leader already in a clan.
	CHECK(cs.CreateClan("C5", "Alices Second", "AL2", Faction::Enforcer, "alice") == ClanResult::PlayerInClan,
		"T1: leader already clanned");
}

// --- Test 2: invite gate, wrong faction, self, full, already ----------------
void TestInvite() {
	ClanService cs;
	cs.CreateClan("C1", "Redhill", "RED", Faction::Criminal, "leader");

	// Self-invite rejected.
	CHECK(cs.Invite("leader", "leader", Faction::Criminal) == ClanResult::SelfTarget, "T2: no self-invite");
	// Wrong faction rejected.
	CHECK(cs.Invite("leader", "cop", Faction::Enforcer) == ClanResult::WrongFaction, "T2: wrong faction");
	// Non-member cannot invite.
	CHECK(cs.Invite("outsider", "x", Faction::Criminal) == ClanResult::NotInClan, "T2: outsider can't invite");

	// Leader (perms=All) invites correctly.
	CHECK(cs.Invite("leader", "bob", Faction::Criminal) == ClanResult::Ok, "T2: leader invites bob");
	CHECK(Has(cs.PendingInvitesFor("bob"), "C1"), "T2: bob has pending invite");
	// Duplicate invite rejected.
	CHECK(cs.Invite("leader", "bob", Faction::Criminal) == ClanResult::AlreadyInvited, "T2: no double invite");

	// bob joins, becomes plain Member with no Invite perm -> cannot invite.
	CHECK(cs.AcceptInvite("bob") == ClanResult::Ok, "T2: bob accepts");
	CHECK(cs.RankOf("bob") == "Member", "T2: bob at lowest rank");
	CHECK(cs.Invite("bob", "carol", Faction::Criminal) == ClanResult::NoPermission,
		"T2: plain member lacks invite perm");

	// Target already in a clan.
	cs.CreateClan("C2", "Other", "OTH", Faction::Criminal, "carol");
	CHECK(cs.Invite("leader", "carol", Faction::Criminal) == ClanResult::TargetInClan, "T2: target already clanned");
}

// --- Test 3: invite full clan -----------------------------------------------
void TestClanFull() {
	ClanService cs;
	cs.max_members = 2;
	cs.CreateClan("C1", "Tiny", "TNY", Faction::Criminal, "leader");
	CHECK(cs.Invite("leader", "bob", Faction::Criminal) == ClanResult::Ok, "T3: invite bob ok");
	CHECK(cs.AcceptInvite("bob") == ClanResult::Ok, "T3: bob accepts (size 2)");
	CHECK(cs.Size("C1") == 2, "T3: at cap");
	// Now full -> further invites rejected.
	CHECK(cs.Invite("leader", "carol", Faction::Criminal) == ClanResult::ClanFull, "T3: clan full on invite");
}

// --- Test 4: accept/decline invite behavior ---------------------------------
void TestAcceptDecline() {
	ClanService cs;
	cs.CreateClan("C1", "Alpha", "ALP", Faction::Enforcer, "la");
	cs.CreateClan("C2", "Beta", "BET", Faction::Enforcer, "lb");
	// bob invited by both; accepting one clears the other.
	CHECK(cs.Invite("la", "bob", Faction::Enforcer) == ClanResult::Ok, "T4: A invites bob");
	CHECK(cs.Invite("lb", "bob", Faction::Enforcer) == ClanResult::Ok, "T4: B invites bob");
	CHECK(cs.PendingInvitesFor("bob").size() == 2, "T4: two pending");
	// Accept -> takes most-recent (C2), clears all.
	CHECK(cs.AcceptInvite("bob") == ClanResult::Ok, "T4: bob accepts");
	CHECK(cs.ClanOf("bob") == "C2", "T4: joined most-recent (C2)");
	CHECK(cs.PendingInvitesFor("bob").empty(), "T4: all invites cleared on join");

	// Decline path.
	cs.CreateClan("C3", "Gamma", "GAM", Faction::Enforcer, "lc");
	CHECK(cs.Invite("lc", "dan", Faction::Enforcer) == ClanResult::Ok, "T4: C invites dan");
	CHECK(cs.DeclineInvite("dan") == ClanResult::Ok, "T4: dan declines");
	CHECK(cs.PendingInvitesFor("dan").empty(), "T4: decline clears");
	CHECK(cs.DeclineInvite("dan") == ClanResult::NoSuchInvite, "T4: nothing to decline");
	CHECK(cs.AcceptInvite("dan") == ClanResult::NoSuchInvite, "T4: nothing to accept");
}

// --- Test 5: leader cannot leave; member can -------------------------------
void TestLeave() {
	ClanService cs;
	cs.CreateClan("C1", "Wardens", "WRD", Faction::Enforcer, "leader");
	cs.Invite("leader", "bob", Faction::Enforcer);
	cs.AcceptInvite("bob");

	// Retail rule: leader cannot leave outright.
	CHECK(cs.Leave("leader") == ClanResult::LeaderMustTransferOrDisband, "T5: leader can't leave");
	CHECK(cs.InClan("leader"), "T5: leader still in clan");
	// Non-member leave.
	CHECK(cs.Leave("stranger") == ClanResult::NotInClan, "T5: stranger not in clan");
	// Member leaves cleanly.
	CHECK(cs.Leave("bob") == ClanResult::Ok, "T5: bob leaves");
	CHECK(!cs.InClan("bob") && cs.Size("C1") == 1, "T5: bob gone, size 1");
}

// --- Test 6: kick — permission + rank hierarchy + protections ---------------
void TestKick() {
	ClanService cs;
	cs.CreateClan("C1", "Enforcers", "ENF", Faction::Enforcer, "leader");
	// officer (rank 0, has Kick), member (rank 1), member2 (rank 1)
	cs.Invite("leader", "officer", Faction::Enforcer); cs.AcceptInvite("officer");
	cs.SetMemberRank("leader", "officer", 0); // promote to Officer (rank 0)
	cs.Invite("leader", "member", Faction::Enforcer); cs.AcceptInvite("member");
	cs.Invite("leader", "member2", Faction::Enforcer); cs.AcceptInvite("member2");

	CHECK(cs.RankOf("officer") == "Officer", "T6: officer promoted");
	// Can't kick self.
	CHECK(cs.Kick("officer", "officer") == ClanResult::SelfTarget, "T6: no self-kick");
	// Can't kick the leader.
	CHECK(cs.Kick("officer", "leader") == ClanResult::NoPermission, "T6: leader untouchable");
	// Plain member lacks Kick perm.
	CHECK(cs.Kick("member", "member2") == ClanResult::NoPermission, "T6: member lacks kick perm");
	// Officer cannot kick same-rank officer... make a second officer to test.
	cs.Invite("leader", "officer2", Faction::Enforcer); cs.AcceptInvite("officer2");
	cs.SetMemberRank("leader", "officer2", 0);
	CHECK(cs.Kick("officer", "officer2") == ClanResult::NoPermission, "T6: can't kick equal rank");
	// Officer kicks lower-rank member.
	CHECK(cs.Kick("officer", "member") == ClanResult::Ok, "T6: officer kicks member");
	CHECK(!cs.InClan("member"), "T6: member removed");
	// Leader can kick anyone (an officer).
	CHECK(cs.Kick("leader", "officer2") == ClanResult::Ok, "T6: leader kicks officer2");
	CHECK(!cs.InClan("officer2"), "T6: officer2 removed");
	// Kick a non-member.
	CHECK(cs.Kick("leader", "ghost") == ClanResult::NoSuchMember, "T6: no such member");
}

// --- Test 7: disband clears membership + invites ----------------------------
void TestDisband() {
	ClanService cs;
	cs.CreateClan("C1", "Doomed", "DOM", Faction::Criminal, "leader");
	cs.Invite("leader", "bob", Faction::Criminal); cs.AcceptInvite("bob");
	cs.Invite("leader", "pending", Faction::Criminal); // pending invite, not accepted

	// Non-leader cannot disband.
	CHECK(cs.Disband("bob") == ClanResult::NotLeader, "T7: non-leader can't disband");
	CHECK(cs.Disband("leader") == ClanResult::Ok, "T7: leader disbands");
	CHECK(!cs.InClan("leader") && !cs.InClan("bob"), "T7: memberships cleared");
	CHECK(cs.Find("C1") == nullptr, "T7: clan gone");
	CHECK(cs.PendingInvitesFor("pending").empty(), "T7: dangling invite purged");
	CHECK(cs.Disband("nobody") == ClanResult::NotInClan, "T7: disband when clanless");
}

// --- Test 8: transfer leadership --------------------------------------------
void TestTransfer() {
	ClanService cs;
	cs.CreateClan("C1", "Regents", "REG", Faction::Enforcer, "old");
	cs.Invite("old", "new", Faction::Enforcer); cs.AcceptInvite("new");

	CHECK(cs.TransferLeader("old", "old") == ClanResult::SelfTarget, "T8: no self-transfer");
	CHECK(cs.TransferLeader("new", "old") == ClanResult::NotLeader, "T8: only leader transfers");
	CHECK(cs.TransferLeader("old", "ghost") == ClanResult::NoSuchMember, "T8: target must be member");
	CHECK(cs.TransferLeader("old", "new") == ClanResult::Ok, "T8: transfer ok");
	CHECK(cs.LeaderOf("C1") == "new" && cs.IsLeader("new"), "T8: new is leader");
	CHECK(!cs.IsLeader("old") && cs.InClan("old"), "T8: old stays as member");
	CHECK(cs.RankOf("old") == "Officer" && cs.RankOf("new") == "Officer", "T8: both at rank 0");
	// After transfer, old leader CAN leave (no longer leader).
	CHECK(cs.Leave("old") == ClanResult::Ok, "T8: former leader can now leave");
}

// --- Test 9: MOTD -----------------------------------------------------------
void TestMotd() {
	ClanService cs;
	cs.CreateClan("C1", "Heralds", "HER", Faction::Criminal, "leader");
	cs.Invite("leader", "member", Faction::Criminal); cs.AcceptInvite("member");

	CHECK(cs.SetMotd("member", "hi") == ClanResult::NoPermission, "T9: member lacks EditMotd");
	CHECK(cs.SetMotd("leader", "Raid at 8pm") == ClanResult::Ok, "T9: leader sets motd");
	CHECK(cs.MotdOf("C1") == "Raid at 8pm", "T9: motd stored");
	CHECK(cs.SetMotd("stranger", "x") == ClanResult::NotInClan, "T9: outsider can't set");
}

// --- Test 10: ranks — add, assign, permission escalation --------------------
void TestRanks() {
	ClanService cs;
	cs.CreateClan("C1", "Architects", "ARC", Faction::Enforcer, "leader");
	cs.Invite("leader", "bob", Faction::Enforcer); cs.AcceptInvite("bob");
	// bob is plain Member -> no ManageRanks.
	CHECK(cs.AddRank("bob", "Recruit", ClanPerm::None) == ClanResult::NoPermission, "T10: member can't add ranks");
	// Leader adds a new rank (index 2) with Invite perm.
	CHECK(cs.AddRank("leader", "Recruiter", ClanPerm::Invite) == ClanResult::Ok, "T10: leader adds rank");
	// Assign bob to that rank.
	CHECK(cs.SetMemberRank("leader", "bob", 2) == ClanResult::Ok, "T10: assign bob to Recruiter");
	CHECK(cs.RankOf("bob") == "Recruiter", "T10: bob is Recruiter");
	CHECK(cs.HasPermission("bob", ClanPerm::Invite), "T10: bob now has Invite perm");
	CHECK(!cs.HasPermission("bob", ClanPerm::Kick), "T10: bob lacks Kick perm");
	// bob can now invite.
	CHECK(cs.Invite("bob", "carol", Faction::Enforcer) == ClanResult::Ok, "T10: bob (Recruiter) invites");
	// Out-of-range rank.
	CHECK(cs.SetMemberRank("leader", "bob", 99) == ClanResult::NoSuchRank, "T10: bad rank index");
	// Cannot change the leader's rank.
	CHECK(cs.SetMemberRank("leader", "leader", 1) == ClanResult::NoPermission, "T10: leader rank fixed");
	// Leader always has All perms.
	CHECK(cs.HasPermission("leader", ClanPerm::All), "T10: leader has All");
}

// --- Test 11: query surface -------------------------------------------------
void TestQueries() {
	ClanService cs;
	CHECK(cs.ClanOf("nobody").empty(), "T11: clanless -> empty");
	CHECK(!cs.InClan("nobody"), "T11: not in clan");
	CHECK(cs.LeaderOf("NONE").empty(), "T11: no such clan leader");
	CHECK(cs.Find("NONE") == nullptr, "T11: no such clan");
	CHECK(cs.Size("NONE") == 0, "T11: size of missing clan");
	CHECK(cs.MembersOf("NONE").empty(), "T11: members of missing clan");

	cs.CreateClan("C1", "Query Test", "QRY", Faction::Criminal, "leader");
	cs.Invite("leader", "bob", Faction::Criminal); cs.AcceptInvite("bob");
	auto members = cs.MembersOf("C1");
	CHECK(members.size() == 2 && Has(members, "leader") && Has(members, "bob"), "T11: members listed");
}

// --- Test 12: persistence round-trip (SaveJson -> LoadJson) -----------------
void TestPersistence() {
	ClanService a;
	a.CreateClan("C1", "The \"Wired\" Crew", "WIR", Faction::Enforcer, "boss"); // quote in name
	a.Invite("boss", "vet", Faction::Enforcer);   a.AcceptInvite("vet");
	a.SetMemberRank("boss", "vet", 0);             // promote vet to Officer
	a.Invite("boss", "grunt", Faction::Enforcer);  a.AcceptInvite("grunt");
	a.AddRank("boss", "Recruiter", ClanPerm::Invite);
	a.SetMotd("boss", "Meet at the docks");
	a.CreateClan("C2", "Bloodroses", "ROSE", Faction::Criminal, "queen");

	const std::string json = a.SaveJson();
	CHECK(!json.empty(), "T12: SaveJson non-empty");

	ClanService b;
	CHECK(b.LoadJson(json) == true, "T12: LoadJson ok");

	// Clan/member topology preserved.
	CHECK(b.Find("C1") != nullptr && b.Find("C2") != nullptr, "T12: both clans restored");
	CHECK(b.LeaderOf("C1") == "boss" && b.LeaderOf("C2") == "queen", "T12: leaders restored");
	CHECK(b.Find("C1")->faction == Faction::Enforcer, "T12: faction restored");
	CHECK(b.Find("C1")->name == "The \"Wired\" Crew", "T12: escaped name round-trips");
	CHECK(b.Find("C1")->tag == "WIR", "T12: tag restored");
	CHECK(b.MotdOf("C1") == "Meet at the docks", "T12: motd restored");
	CHECK(b.Size("C1") == 3, "T12: member count restored");

	// player->clan index rebuilt.
	CHECK(b.ClanOf("vet") == "C1" && b.ClanOf("grunt") == "C1", "T12: membership index rebuilt");
	CHECK(b.ClanOf("queen") == "C2", "T12: cross-clan index rebuilt");

	// Ranks + permissions preserved (vet promoted, grunt at Member, custom rank present).
	CHECK(b.RankOf("vet") == "Officer", "T12: promoted rank restored");
	CHECK(b.RankOf("grunt") == "Member", "T12: default rank restored");
	CHECK(b.HasPermission("vet", ClanPerm::Kick), "T12: officer perms restored");
	CHECK(!b.HasPermission("grunt", ClanPerm::Invite), "T12: member perms restored");
	// Custom rank survived: assign grunt to it and confirm the perm.
	CHECK(b.SetMemberRank("boss", "grunt", 2) == ClanResult::Ok, "T12: custom rank index valid");
	CHECK(b.RankOf("grunt") == "Recruiter" && b.HasPermission("grunt", ClanPerm::Invite),
		"T12: custom rank + perm restored");

	// next_join_seq preserved -> a newly-joined member gets a strictly-higher seq than
	// any restored member, so join ordering stays monotonic across a reload.
	b.Invite("boss", "fresh", Faction::Enforcer); b.AcceptInvite("fresh");
	const ClanMember* fm = b.Find("C1")->FindMember("fresh");
	const ClanMember* gm = b.Find("C1")->FindMember("grunt");
	CHECK(fm && gm && fm->joined_seq > gm->joined_seq, "T12: join seq monotonic post-reload");

	// Empty/garbage input rejected.
	ClanService c;
	CHECK(c.LoadJson("") == false, "T12: empty input rejected");
	CHECK(c.LoadJson("{ \"clans\": [] }") == false, "T12: no-clans input rejected");

	// Round-trip is stable: re-saving the loaded state reproduces the document.
	CHECK(b.SaveJson() != json, "T12: re-save differs after adding member (sanity)");
	ClanService d; d.LoadJson(json);
	CHECK(d.SaveJson() == a.SaveJson(), "T12: save/load/save is idempotent");
}

int main() {
	std::cout << "=== APBClanTests (M14 ClanService) ===\n";
	TestCreate();
	TestInvite();
	TestClanFull();
	TestAcceptDecline();
	TestLeave();
	TestKick();
	TestDisband();
	TestTransfer();
	TestMotd();
	TestRanks();
	TestQueries();
	TestPersistence();
	std::cout << "FAILS=" << fails << "\n";
	return fails ? 1 : 0;
}
