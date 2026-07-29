// APBMatchmaking.cpp — M11 (D10/D14): threat-tier opposition matcher implementation.
// Pure C++17, no UE/platform deps. Deterministic greedy oldest-first pairing.
#include "APBMatchmaking.h"
#include <algorithm>

namespace apb {

// --- queue management --------------------------------------------------------

bool Matchmaker::Enqueue(const MatchTicket& ticket) {
	if (ticket.party_id.empty()) return false;
	// Re-queue replaces the existing ticket (and resets the wait clock to the new one).
	for (auto& t : queue_) {
		if (t.party_id == ticket.party_id) { t = ticket; return true; }
	}
	queue_.push_back(ticket);
	return true;
}

bool Matchmaker::Cancel(const std::string& party_id) {
	for (auto it = queue_.begin(); it != queue_.end(); ++it) {
		if (it->party_id == party_id) { queue_.erase(it); return true; }
	}
	return false;
}

bool Matchmaker::IsQueued(const std::string& party_id) const {
	for (const auto& t : queue_) if (t.party_id == party_id) return true;
	return false;
}

int32_t Matchmaker::QueueSize() const { return (int32_t)queue_.size(); }

int32_t Matchmaker::QueueSizeFor(Faction f) const {
	int32_t n = 0;
	for (const auto& t : queue_) if (t.faction == f) ++n;
	return n;
}

int32_t Matchmaker::PlayersWaiting() const {
	int32_t n = 0;
	for (const auto& t : queue_) n += t.party_size > 0 ? t.party_size : 1;
	return n;
}

int32_t Matchmaker::PlayersWaitingFor(Faction f) const {
	int32_t n = 0;
	for (const auto& t : queue_) if (t.faction == f) n += t.party_size > 0 ? t.party_size : 1;
	return n;
}

// --- tolerance policy --------------------------------------------------------

int32_t Matchmaker::ToleranceForWait(int64_t enqueued_ms, int64_t now_ms) const {
	if (widen_interval_ms <= 0) return max_tolerance; // degenerate config -> always max slack
	int64_t waited = now_ms - enqueued_ms;
	if (waited <= 0) return 0;                          // just enqueued (or clock skew): exact tier
	int64_t steps = waited / widen_interval_ms;
	if (steps < 0) steps = 0;
	if (steps > max_tolerance) steps = max_tolerance;
	return (int32_t)steps;
}

// --- pairing -----------------------------------------------------------------

std::vector<MatchPairing> Matchmaker::FormMatches(int64_t now_ms) {
	std::vector<MatchPairing> out;

	// Work on an oldest-first copy so results are deterministic and fair.
	std::vector<MatchTicket> q = queue_;
	std::stable_sort(q.begin(), q.end(),
		[](const MatchTicket& a, const MatchTicket& b) { return a.enqueued_ms < b.enqueued_ms; });

	std::vector<char> matched(q.size(), 0);

	for (size_t i = 0; i < q.size(); ++i) {
		if (matched[i]) continue;
		const MatchTicket& a = q[i];
		const int32_t tolA = ToleranceForWait(a.enqueued_ms, now_ms);

		// Find the best opposing-faction partner among the younger, unmatched tickets.
		int best = -1, bestGap = 0x7fffffff;
		int64_t bestEnq = 0;
		for (size_t j = i + 1; j < q.size(); ++j) {
			if (matched[j]) continue;
			const MatchTicket& b = q[j];
			if (b.faction == a.faction) continue;           // opposition = opposing faction only
			const int32_t tolB = ToleranceForWait(b.enqueued_ms, now_ms);
			const int32_t eff  = tolA > tolB ? tolA : tolB;  // longer wait grants the wider search
			const int32_t gap  = std::abs(a.threat_tier - b.threat_tier);
			if (gap > eff) continue;                          // tier gap too wide for current slack
			// Prefer the closest tier; tie-break to the oldest candidate (smallest enqueued_ms).
			if (gap < bestGap || (gap == bestGap && b.enqueued_ms < bestEnq)) {
				best = (int)j; bestGap = gap; bestEnq = b.enqueued_ms;
			}
		}

		if (best < 0) continue;
		const MatchTicket& b = q[(size_t)best];

		MatchPairing p;
		p.tier = a.threat_tier > b.threat_tier ? a.threat_tier : b.threat_tier;
		p.tolerance_used = bestGap;
		p.formed_ms = now_ms;
		const MatchTicket& enf = a.faction == Faction::Enforcer ? a : b;
		const MatchTicket& cri = a.faction == Faction::Criminal ? a : b;
		p.enforcers.push_back(enf);
		p.criminals.push_back(cri);
		out.push_back(std::move(p));

		matched[i] = 1;
		matched[(size_t)best] = 1;
	}

	// Rebuild the queue from the survivors (preserving oldest-first order).
	std::vector<MatchTicket> remaining;
	remaining.reserve(q.size());
	for (size_t i = 0; i < q.size(); ++i) if (!matched[i]) remaining.push_back(q[i]);
	queue_.swap(remaining);

	return out;
}

std::vector<MatchTicket> Matchmaker::Snapshot() const {
	std::vector<MatchTicket> q = queue_;
	std::stable_sort(q.begin(), q.end(),
		[](const MatchTicket& a, const MatchTicket& b) { return a.enqueued_ms < b.enqueued_ms; });
	return q;
}

} // namespace apb
