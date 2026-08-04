#pragma once
// APBDistrictDirectory.h — M16 (brief #15): the WORLD server's live registry of district
// server instances, with heartbeat-driven liveness + stale eviction. Pure C++17, no UE /
// platform headers — unit-testable in isolation like the rest of the Domain.
//
// This is the server-side consumer of the M7 relay control channel (APBRelayProtocol.h):
//   district process --Register/Heartbeat/ReportLoad/PlayerJoined/PlayerLeft--> world
// The world's relay recv loop DecodeStream()s those messages and feeds each into Apply().
// PruneStale() (called on the world tick) implements the M16 verify gate: a district that
// stops heart-beating (e.g. kill -9) is evicted from the directory within ~2 heartbeats.
//
// Deterministic: every liveness decision takes a caller-supplied monotonic clock (now_ms),
// never a wall-clock read, so it replays identically in tests and on the server.
#include "APBRelayProtocol.h" // RelayMessage / RelayVerb (Apply convenience)
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace apb {

// A single registered district-server instance as the world sees it.
struct DistrictNode {
	std::string district;        // district id/name, e.g. "Financial"
	int32_t     numeric_id = 0;  // stable apbdb numeric_id (also the map key)
	int32_t     port = 0;        // district NetDriver port (world hands this to clients)
	std::string target_district_epoch; // district boot epoch
	int32_t     player_count = 0;// last ReportLoad occupancy
	int64_t     registered_ms = 0;
	int64_t     last_heartbeat_ms = 0;
	bool        alive = false;   // false once evicted (kept for one query, then removed)
};

// Why a node left the directory (for logging / relay ack to any survivors).
enum class EvictReason { None, Stale, Graceful };
const char* EvictReasonName(EvictReason r);

// Per-district aggregate the world's district-select screen renders: is the district
// available at all (>=1 live instance) and what is its combined population (the "pop bar").
// APB pools multiple instances of the same district; the player picks the district, not the
// instance — LeastLoaded() then routes them to a specific node.
struct DistrictPopulation {
	std::string district;          // district id/name
	int32_t     instances = 0;     // number of live instances serving it
	int32_t     total_players = 0; // summed occupancy across those instances
};

class DistrictDirectory {
public:
	// Expected heartbeat cadence and how many missed beats before eviction.
	// Default: 5s cadence, evict after >2 missed (i.e. >10s silence) per the M16 gate.
	double heartbeat_interval_ms = 5000.0;
	double eviction_multiple = 2.0;

	// --- relay-message ingestion (world recv loop calls one of these per message) ---

	// Register or refresh a district instance. Returns true if the instance is now live.
	// A re-Register of an existing numeric_id updates its port and re-arms liveness.
	bool Register(const std::string& district, int32_t numeric_id, int32_t port, const std::string& target_district_epoch, int64_t now_ms);

	// Record a liveness beat. Returns false if numeric_id is unknown (world should ask it
	// to Register). Beats from an evicted/unknown id do NOT resurrect it.
	bool Heartbeat(int32_t numeric_id, int64_t now_ms);

	// Update occupancy from a ReportLoad. Returns false if unknown. Also counts as liveness.
	bool ReportLoad(int32_t numeric_id, int32_t player_count, int64_t now_ms);

	// Graceful removal (district announced shutdown). Returns true if it was present.
	bool Deregister(int32_t numeric_id, EvictReason reason = EvictReason::Graceful);

	// Convenience: dispatch a decoded relay message to the right handler above.
	// Register/Heartbeat/ReportLoad/PlayerJoined/PlayerLeft are handled; others ignored.
	// (PlayerJoined/PlayerLeft also count as liveness and adjust player_count.)
	bool Apply(const RelayMessage& m, int64_t now_ms);

	// --- world tick ---

	// Evict every instance silent for longer than heartbeat_interval_ms*eviction_multiple.
	// Returns the number evicted. Call once per world tick with the current clock.
	int32_t PruneStale(int64_t now_ms);

	// Milliseconds of silence that trigger eviction.
	double StaleThresholdMs() const { return heartbeat_interval_ms * eviction_multiple; }

	// --- queries ---
	const DistrictNode* Find(int32_t numeric_id) const;
	bool IsAlive(int32_t numeric_id) const;
	int32_t AliveCount() const;
	std::vector<DistrictNode> ListAlive() const;

	// Per-district population summary across all live instances, sorted by district name
	// for deterministic output. This is what GetDistrictListJson exposes to the client's
	// district-select screen (server-up + pop bar), rather than the raw catalog list.
	std::vector<DistrictPopulation> AggregateByDistrict() const;

	// Least-loaded live instance serving `district` (by name), or nullptr if none.
	// This is the "which instance do I send the joining player to" directory query.
	const DistrictNode* LeastLoaded(const std::string& district) const;

private:
	std::unordered_map<int32_t, DistrictNode> nodes_;
};

} // namespace apb
