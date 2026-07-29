#pragma once
// APBMatchmaking.h — M11 (D10/D14): pure-C++17 threat-tier opposition matchmaker.
// No platform/UE headers — unit-testable in isolation like WorldService/ChatService.
//
// APB's mission model is adversarial CROSS-FACTION opposition: a party on one faction is
// paired against an opposing-faction party of comparable skill (threat tier). This service
// is the AUTHORITATIVE pairing brain: parties enqueue (a group queues as ONE atomic ticket,
// so groups are never split), and FormMatches() pairs Enforcer<->Criminal parties whose
// threat tiers are within a tolerance that WIDENS the longer a party waits (retail relaxes
// its search over time so nobody sits in queue forever). Dispatch/geometry (spawning the
// opposition, mission stages) is a UE district concern (M11 N-side) built on top of this.
//
// Grounded on _active.md D10 ("Matchmaking = threat-tier opposition pairing + group queue
// in Domain (APB-authentic), not a separate process") and D14 ("opposition dispatch via
// Domain matchmaking (threat-tier pairing)"). Reuses apb::Faction from APBTypes.h.
#include "APBTypes.h"
#include <string>
#include <vector>
#include <cstdint>

namespace apb {

// A queued party. A solo player is a party of size 1 (party_id == player name); a group
// queues as one ticket (party_id == group id) so the matcher keeps the group together.
struct MatchTicket {
	std::string party_id;      // group id, or solo player name — unique key in the queue
	Faction     faction = Faction::Criminal;
	int32_t     threat_tier = 0; // 0..N (higher = more skilled); pairing prefers equal tiers
	int32_t     party_size = 1;  // members in the party (a group queues as a unit)
	int64_t     enqueued_ms = 0; // wall clock at enqueue; drives the widening tolerance
};

// A formed opposition match: one Enforcer party vs one Criminal party (vectors leave room
// for future N-vs-N balancing without an API break). `tier` is the higher of the two tiers
// (the mission scales to the tougher side); `tolerance_used` records how far the search had
// to widen to pair them (0 = exact-tier match).
struct MatchPairing {
	std::vector<MatchTicket> enforcers;
	std::vector<MatchTicket> criminals;
	int32_t tier = 0;
	int32_t tolerance_used = 0;
	int64_t formed_ms = 0;
};

class Matchmaker {
public:
	// Tier tolerance policy. At enqueue time a party demands an exact-tier opponent
	// (tolerance 0); every `widen_interval_ms` it waits, the acceptable tier gap grows by 1,
	// capped at `max_tolerance`. The pair's effective tolerance is driven by the party that
	// has waited LONGER (fairness: the one suffering the queue gets the relaxed search).
	int32_t max_tolerance   = 4;
	int64_t widen_interval_ms = 15000; // 15s per +1 tier of slack (retail-style relaxation)

	// --- queue management ---
	// Enqueue a party. If party_id is already queued, its ticket is REPLACED (re-queue),
	// preserving the original enqueued_ms is NOT done — a re-queue resets the wait. Returns
	// false for an empty party_id.
	bool Enqueue(const MatchTicket& ticket);
	// Remove a queued party (player left / cancelled). Returns false if it was not queued.
	bool Cancel(const std::string& party_id);
	bool IsQueued(const std::string& party_id) const;

	int32_t QueueSize() const;
	int32_t QueueSizeFor(Faction f) const;
	// Total players waiting (sum of party_size), optionally for one faction.
	int32_t PlayersWaiting() const;
	int32_t PlayersWaitingFor(Faction f) const;

	// Tolerance a party is entitled to after waiting (now_ms - enqueued_ms).
	int32_t ToleranceForWait(int64_t enqueued_ms, int64_t now_ms) const;

	// Form as many opposition pairings as possible at time now_ms. Oldest-waiting parties
	// are served first; each match removes both parties from the queue. A pairing is legal
	// only if the two parties are opposing factions AND |tierA - tierB| <= the effective
	// (widened) tolerance. Deterministic: given the same queue + now_ms, always the same
	// pairings.
	std::vector<MatchPairing> FormMatches(int64_t now_ms);

	// Read-only queue snapshot (for probes/UI), oldest-first.
	std::vector<MatchTicket> Snapshot() const;

private:
	std::vector<MatchTicket> queue_; // insertion order is normalised to oldest-first on match
};

} // namespace apb
