// APBDistrictDirectory.cpp — M16 (brief #15): implementation of the world-side district
// registry declared in APBDistrictDirectory.h. Pure logic; no I/O, no wall-clock reads.
#include "APBDistrictDirectory.h"
#include <algorithm>

namespace apb {

const char* EvictReasonName(EvictReason r) {
	switch (r) {
		case EvictReason::None:     return "None";
		case EvictReason::Stale:    return "Stale";
		case EvictReason::Graceful: return "Graceful";
		default:                    return "Unknown";
	}
}

bool DistrictDirectory::Register(const std::string& district, int32_t numeric_id,
	int32_t port, int64_t now_ms) {
	const bool is_new = nodes_.find(numeric_id) == nodes_.end();
	DistrictNode& n = nodes_[numeric_id];
	n.district = district;
	n.numeric_id = numeric_id;
	n.port = port;
	if (is_new) n.registered_ms = now_ms; // first-seen; re-register keeps the original
	n.last_heartbeat_ms = now_ms;
	n.alive = true;
	return true;
}

bool DistrictDirectory::Heartbeat(int32_t numeric_id, int64_t now_ms) {
	auto it = nodes_.find(numeric_id);
	if (it == nodes_.end() || !it->second.alive) return false; // unknown/evicted: no resurrect
	it->second.last_heartbeat_ms = now_ms;
	return true;
}

bool DistrictDirectory::ReportLoad(int32_t numeric_id, int32_t player_count, int64_t now_ms) {
	auto it = nodes_.find(numeric_id);
	if (it == nodes_.end() || !it->second.alive) return false;
	it->second.player_count = player_count < 0 ? 0 : player_count;
	it->second.last_heartbeat_ms = now_ms; // load reports also prove liveness
	return true;
}

bool DistrictDirectory::Deregister(int32_t numeric_id, EvictReason /*reason*/) {
	return nodes_.erase(numeric_id) > 0;
}

bool DistrictDirectory::Apply(const RelayMessage& m, int64_t now_ms) {
	switch (m.verb) {
		case RelayVerb::Register:
			return Register(m.district, m.numeric_id, m.port, now_ms);
		case RelayVerb::Heartbeat:
			return Heartbeat(m.numeric_id, now_ms);
		case RelayVerb::ReportLoad:
			return ReportLoad(m.numeric_id, m.player_count, now_ms);
		case RelayVerb::PlayerJoined: {
			auto it = nodes_.find(m.numeric_id);
			if (it == nodes_.end() || !it->second.alive) return false;
			it->second.player_count += 1;
			it->second.last_heartbeat_ms = now_ms;
			return true;
		}
		case RelayVerb::PlayerLeft: {
			auto it = nodes_.find(m.numeric_id);
			if (it == nodes_.end() || !it->second.alive) return false;
			if (it->second.player_count > 0) it->second.player_count -= 1;
			it->second.last_heartbeat_ms = now_ms;
			return true;
		}
		default:
			return false; // RegisterAck/ExpectTicket/etc. are not directory events
	}
}

int32_t DistrictDirectory::PruneStale(int64_t now_ms) {
	const double threshold = StaleThresholdMs();
	int32_t evicted = 0;
	for (auto it = nodes_.begin(); it != nodes_.end(); ) {
		const double silent = (double)(now_ms - it->second.last_heartbeat_ms);
		if (it->second.alive && silent > threshold) {
			it = nodes_.erase(it);
			++evicted;
		} else {
			++it;
		}
	}
	return evicted;
}

const DistrictNode* DistrictDirectory::Find(int32_t numeric_id) const {
	auto it = nodes_.find(numeric_id);
	return it == nodes_.end() ? nullptr : &it->second;
}

bool DistrictDirectory::IsAlive(int32_t numeric_id) const {
	auto it = nodes_.find(numeric_id);
	return it != nodes_.end() && it->second.alive;
}

int32_t DistrictDirectory::AliveCount() const {
	int32_t n = 0;
	for (const auto& kv : nodes_) if (kv.second.alive) ++n;
	return n;
}

std::vector<DistrictNode> DistrictDirectory::ListAlive() const {
	std::vector<DistrictNode> out;
	for (const auto& kv : nodes_) if (kv.second.alive) out.push_back(kv.second);
	std::sort(out.begin(), out.end(),
		[](const DistrictNode& a, const DistrictNode& b) { return a.numeric_id < b.numeric_id; });
	return out;
}

std::vector<DistrictPopulation> DistrictDirectory::AggregateByDistrict() const {
	std::vector<DistrictPopulation> out;
	for (const auto& kv : nodes_) {
		const DistrictNode& n = kv.second;
		if (!n.alive) continue;
		auto it = std::find_if(out.begin(), out.end(),
			[&](const DistrictPopulation& p) { return p.district == n.district; });
		if (it == out.end()) {
			DistrictPopulation p;
			p.district = n.district;
			p.instances = 1;
			p.total_players = n.player_count;
			out.push_back(p);
		} else {
			it->instances += 1;
			it->total_players += n.player_count;
		}
	}
	std::sort(out.begin(), out.end(),
		[](const DistrictPopulation& a, const DistrictPopulation& b) { return a.district < b.district; });
	return out;
}

const DistrictNode* DistrictDirectory::LeastLoaded(const std::string& district) const {
	const DistrictNode* best = nullptr;
	for (const auto& kv : nodes_) {
		const DistrictNode& n = kv.second;
		if (!n.alive || n.district != district) continue;
		if (!best || n.player_count < best->player_count ||
			(n.player_count == best->player_count && n.numeric_id < best->numeric_id)) {
			best = &n;
		}
	}
	return best;
}

} // namespace apb
