// run_matchmaking_tests.cpp — M11 (D10/D14): Matchmaker pairing tests.
// Links only APBMatchmaking.cpp. Pattern mirrors the other run_*_tests.cpp.
#include "APBMatchmaking.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

static MatchTicket Party(const std::string& id, Faction f, int32_t tier, int32_t size, int64_t at) {
	MatchTicket t; t.party_id = id; t.faction = f; t.threat_tier = tier; t.party_size = size; t.enqueued_ms = at;
	return t;
}

static void TestEnqueueCancel() {
	Matchmaker mm;
	CHECK(!mm.Enqueue(Party("", Faction::Criminal, 1, 1, 0)), "empty party_id rejected");
	CHECK(mm.Enqueue(Party("crim1", Faction::Criminal, 2, 3, 0)), "enqueue accepted");
	CHECK(mm.QueueSize() == 1 && mm.IsQueued("crim1"), "queued once");
	// Re-queue with the same id replaces (no duplicate).
	CHECK(mm.Enqueue(Party("crim1", Faction::Criminal, 3, 3, 500)), "re-queue accepted");
	CHECK(mm.QueueSize() == 1, "re-queue does not duplicate");
	CHECK(mm.Snapshot()[0].threat_tier == 3 && mm.Snapshot()[0].enqueued_ms == 500, "re-queue replaced ticket + reset wait");
	CHECK(mm.PlayersWaiting() == 3, "PlayersWaiting sums party_size");
	CHECK(!mm.Cancel("nope"), "cancel of absent party returns false");
	CHECK(mm.Cancel("crim1") && mm.QueueSize() == 0, "cancel removes the party");
}

static void TestExactTierMatch() {
	Matchmaker mm;
	mm.Enqueue(Party("enf", Faction::Enforcer, 2, 1, 0));
	mm.Enqueue(Party("cri", Faction::Criminal, 2, 1, 0));
	auto matches = mm.FormMatches(0);
	CHECK(matches.size() == 1, "exact-tier opposing parties pair immediately");
	CHECK(!matches.empty() && matches[0].tier == 2 && matches[0].tolerance_used == 0, "pairing reports tier + zero slack");
	CHECK(!matches.empty() && matches[0].enforcers.size() == 1 && matches[0].criminals.size() == 1,
	      "one party on each side");
	CHECK(!matches.empty() && matches[0].enforcers[0].party_id == "enf" && matches[0].criminals[0].party_id == "cri",
	      "sides sorted by faction");
	CHECK(mm.QueueSize() == 0, "matched parties leave the queue");
}

static void TestNoSameFactionMatch() {
	Matchmaker mm;
	mm.Enqueue(Party("c1", Faction::Criminal, 2, 1, 0));
	mm.Enqueue(Party("c2", Faction::Criminal, 2, 1, 0));
	auto matches = mm.FormMatches(1000000); // even with max slack, same faction never pairs
	CHECK(matches.empty(), "same-faction parties never pair (opposition is cross-faction)");
	CHECK(mm.QueueSize() == 2, "both remain queued");
}

static void TestToleranceForWait() {
	Matchmaker mm; // defaults: widen_interval_ms=15000, max_tolerance=4
	CHECK(mm.ToleranceForWait(0, 0) == 0, "no wait -> exact tier (tolerance 0)");
	CHECK(mm.ToleranceForWait(0, 14999) == 0, "under one interval -> still 0");
	CHECK(mm.ToleranceForWait(0, 15000) == 1, "one interval -> tolerance 1");
	CHECK(mm.ToleranceForWait(0, 45000) == 3, "three intervals -> tolerance 3");
	CHECK(mm.ToleranceForWait(0, 60000) == 4, "four intervals -> tolerance 4");
	CHECK(mm.ToleranceForWait(0, 9999999) == 4, "tolerance caps at max_tolerance");
	CHECK(mm.ToleranceForWait(1000, 500) == 0, "clock skew (now<enqueued) -> 0");
}

static void TestTierGapNeedsWidening() {
	Matchmaker mm;
	mm.Enqueue(Party("enf", Faction::Enforcer, 0, 1, 0));
	mm.Enqueue(Party("cri", Faction::Criminal, 3, 1, 0));
	CHECK(mm.FormMatches(0).empty(), "tier gap 3 with no wait does not pair");
	CHECK(mm.FormMatches(30000).empty(), "tier gap 3 at tolerance 2 still does not pair");
	CHECK(mm.QueueSize() == 2, "unmatched parties stay queued");
	auto matches = mm.FormMatches(45000); // tolerance 3 -> gap 3 acceptable
	CHECK(matches.size() == 1 && matches[0].tolerance_used == 3, "widened tolerance pairs the tier-3 gap");
	CHECK(matches.size() == 1 && matches[0].tier == 3, "match tier is the tougher side");
}

static void TestBestGapPreferred() {
	Matchmaker mm;
	// One criminal, two enforcers within a widened window; the closer tier must win.
	mm.Enqueue(Party("cri", Faction::Criminal, 2, 1, 0));
	mm.Enqueue(Party("enf_far", Faction::Enforcer, 4, 1, 1));
	mm.Enqueue(Party("enf_near", Faction::Enforcer, 2, 1, 2));
	auto matches = mm.FormMatches(100000); // ample tolerance for both
	CHECK(matches.size() == 1, "one criminal pairs with exactly one enforcer");
	CHECK(!matches.empty() && matches[0].enforcers[0].party_id == "enf_near", "closest-tier opponent chosen over farther one");
	CHECK(mm.IsQueued("enf_far"), "the farther-tier enforcer stays queued");
}

static void TestGroupAsUnit() {
	Matchmaker mm;
	mm.Enqueue(Party("squad", Faction::Enforcer, 1, 4, 0)); // a 4-player group queues as one ticket
	mm.Enqueue(Party("solo", Faction::Criminal, 1, 1, 0));
	CHECK(mm.PlayersWaitingFor(Faction::Enforcer) == 4, "group counts all members in players-waiting");
	CHECK(mm.PlayersWaitingFor(Faction::Criminal) == 1, "solo counts one");
	auto matches = mm.FormMatches(0);
	CHECK(matches.size() == 1 && matches[0].enforcers[0].party_size == 4, "group stays intact through matching");
}

static void TestMultiplePairingsOldestFirst() {
	Matchmaker mm;
	mm.Enqueue(Party("E0", Faction::Enforcer, 1, 1, 0));
	mm.Enqueue(Party("E1", Faction::Enforcer, 1, 1, 1));
	mm.Enqueue(Party("C0", Faction::Criminal, 1, 1, 2));
	mm.Enqueue(Party("C1", Faction::Criminal, 1, 1, 3));
	auto matches = mm.FormMatches(10);
	CHECK(matches.size() == 2, "two opposing pairs form in one pass");
	// Oldest enforcer (E0) pairs the oldest available criminal (C0); E1 pairs C1.
	CHECK(matches.size() == 2 && matches[0].enforcers[0].party_id == "E0" && matches[0].criminals[0].party_id == "C0",
	      "oldest-first fairness: E0<->C0");
	CHECK(matches.size() == 2 && matches[1].enforcers[0].party_id == "E1" && matches[1].criminals[0].party_id == "C1",
	      "second pair E1<->C1");
	CHECK(mm.QueueSize() == 0, "queue drained");
}

int main() {
	std::printf("=== APB Matchmaking Tests (M11 threat-tier opposition) ===\n");
	TestEnqueueCancel();
	TestExactTierMatch();
	TestNoSameFactionMatch();
	TestToleranceForWait();
	TestTierGapNeedsWidening();
	TestBestGapPreferred();
	TestGroupAsUnit();
	TestMultiplePairingsOldestFirst();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
