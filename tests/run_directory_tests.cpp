// run_directory_tests.cpp — M16 (brief #15): world-side district registry + heartbeat
// eviction tests. Links APBDistrictDirectory.cpp + APBRelayProtocol.cpp (the test drives
// the directory through the same relay factories the world recv loop uses).
#include "APBDistrictDirectory.h"
#include "APBDistrictPopulationSnapshot.h"
#include "APBRelayProtocol.h"
#include <cstdio>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

static void TestRegisterAndFind() {
	DistrictDirectory dir;
	CHECK(dir.AliveCount() == 0, "empty directory");
	CHECK(dir.Register("Financial", 1001, 7801, 100), "register returns live");
	const DistrictNode* n = dir.Find(1001);
	CHECK(n != nullptr && n->district == "Financial", "found by numeric_id");
	CHECK(n && n->port == 7801, "port recorded");
	CHECK(dir.IsAlive(1001), "registered node is alive");
	CHECK(dir.Find(9999) == nullptr, "unknown id not found");
	// Re-register updates port but keeps first-seen registered_ms.
	dir.Register("Financial", 1001, 7802, 500);
	CHECK(dir.Find(1001)->port == 7802, "re-register updates port");
	CHECK(dir.Find(1001)->registered_ms == 100, "re-register keeps first-seen time");
	CHECK(dir.AliveCount() == 1, "re-register does not duplicate");
}

static void TestHeartbeatLiveness() {
	DistrictDirectory dir;
	dir.Register("Waterfront", 2001, 7811, 0);
	CHECK(dir.Heartbeat(2001, 5000), "heartbeat known id ok");
	CHECK(dir.Find(2001)->last_heartbeat_ms == 5000, "heartbeat advances liveness clock");
	CHECK(!dir.Heartbeat(3003, 5000), "heartbeat unknown id -> false");
	// ReportLoad also proves liveness and updates occupancy.
	CHECK(dir.ReportLoad(2001, 40, 6000), "report load ok");
	CHECK(dir.Find(2001)->player_count == 40, "occupancy recorded");
	CHECK(dir.Find(2001)->last_heartbeat_ms == 6000, "report load counts as liveness");
	CHECK(dir.ReportLoad(2001, -5, 6100) && dir.Find(2001)->player_count == 0, "negative load clamped to 0");
}

static void TestStaleEviction() {
	// The M16 verify gate: a district silent for > 2 heartbeats is evicted.
	DistrictDirectory dir; // interval 5000ms, multiple 2 -> threshold 10000ms
	CHECK(dir.StaleThresholdMs() == 10000.0, "stale threshold = 2 heartbeats");
	dir.Register("Financial", 1001, 7801, 0);
	dir.Register("Financial", 1002, 7802, 0);

	// Both beat at t=5000; prune at 6000 keeps them.
	dir.Heartbeat(1001, 5000);
	dir.Heartbeat(1002, 5000);
	CHECK(dir.PruneStale(6000) == 0, "fresh nodes not evicted");
	CHECK(dir.AliveCount() == 2, "both alive at 6000");

	// 1002 keeps beating; 1001 goes silent. At t=15001, 1001 has been silent 10001ms (>10000).
	dir.Heartbeat(1002, 15000);
	CHECK(dir.PruneStale(15001) == 1, "one stale node evicted");
	CHECK(!dir.IsAlive(1001), "silent node evicted");
	CHECK(dir.IsAlive(1002), "beating node survives");
	CHECK(dir.Find(1001) == nullptr, "evicted node removed from directory");

	// Exactly at threshold (silent == 10000) is NOT yet stale (strict >).
	DistrictDirectory dir2;
	dir2.Register("Waterfront", 2001, 7811, 0);
	CHECK(dir2.PruneStale(10000) == 0, "at-threshold not evicted");
	CHECK(dir2.PruneStale(10001) == 1, "just-past-threshold evicted");

	// A heartbeat from an already-evicted id does not resurrect it.
	CHECK(!dir2.Heartbeat(2001, 10002), "evicted id cannot heartbeat back");
	CHECK(dir2.AliveCount() == 0, "no resurrection");
}

static void TestLeastLoaded() {
	DistrictDirectory dir;
	dir.Register("Financial", 1001, 7801, 0);
	dir.Register("Financial", 1002, 7802, 0);
	dir.Register("Waterfront", 2001, 7811, 0);
	dir.ReportLoad(1001, 55, 100);
	dir.ReportLoad(1002, 20, 100);
	dir.ReportLoad(2001, 5, 100);
	const DistrictNode* best = dir.LeastLoaded("Financial");
	CHECK(best != nullptr && best->numeric_id == 1002, "least-loaded Financial instance picked");
	CHECK(dir.LeastLoaded("Waterfront")->numeric_id == 2001, "single instance picked");
	CHECK(dir.LeastLoaded("Nowhere") == nullptr, "no instance for unknown district");
	CHECK((int)dir.ListAlive().size() == 3, "list alive returns all three");
}

static void TestApplyRelay() {
	DistrictDirectory dir;
	// Drive the directory through the same relay messages the world recv loop decodes.
	CHECK(dir.Apply(RelayCodec::MakeRegister("Financial", 1001, 7801), 0), "apply register");
	CHECK(dir.IsAlive(1001), "register via Apply");
	CHECK(dir.Apply(RelayCodec::MakeHeartbeat(1001, 1), 5000), "apply heartbeat");
	CHECK(dir.Find(1001)->last_heartbeat_ms == 5000, "heartbeat via Apply advances clock");
	CHECK(dir.Apply(RelayCodec::MakeReportLoad(1001, 12), 6000), "apply report load");
	CHECK(dir.Find(1001)->player_count == 12, "load via Apply");

	CHECK(dir.Apply(RelayCodec::MakePlayerJoined("acct", "char", 1001), 6100), "apply player joined");
	CHECK(dir.Find(1001)->player_count == 13, "player joined increments occupancy");
	CHECK(dir.Apply(RelayCodec::MakePlayerLeft("acct", "char", 1001), 6200), "apply player left");
	CHECK(dir.Find(1001)->player_count == 12, "player left decrements occupancy");

	// A non-directory verb (RegisterAck) is ignored.
	CHECK(!dir.Apply(RelayCodec::MakeRegisterAck(1001, true), 6300), "ack verb ignored by directory");
}

static void TestDeregister() {
	DistrictDirectory dir;
	dir.Register("Financial", 1001, 7801, 0);
	CHECK(dir.Deregister(1001), "graceful deregister removes node");
	CHECK(!dir.IsAlive(1001), "deregistered node gone");
	CHECK(!dir.Deregister(1001), "deregister missing node -> false");
	CHECK(std::string(EvictReasonName(EvictReason::Stale)) == "Stale", "evict reason name");
}

static void TestAggregateByDistrict() {
	// The district-select screen shows one row per district (server-up + pooled pop bar),
	// not one row per instance. Two Financial instances collapse into a single row whose
	// population is the sum; districts come back sorted by name for deterministic UI.
	DistrictDirectory dir;
	CHECK(dir.AggregateByDistrict().empty(), "empty directory -> no district rows");
	dir.Register("Waterfront", 2001, 7811, 0);
	dir.Register("Financial", 1001, 7801, 0);
	dir.Register("Financial", 1002, 7802, 0);
	dir.ReportLoad(1001, 55, 100);
	dir.ReportLoad(1002, 20, 100);
	dir.ReportLoad(2001, 5, 100);
	auto rows = dir.AggregateByDistrict();
	CHECK(rows.size() == 2, "two distinct districts aggregated");
	CHECK(rows[0].district == "Financial", "rows sorted by district name (Financial first)");
	CHECK(rows[0].instances == 2, "Financial pools two instances");
	CHECK(rows[0].total_players == 75, "Financial population summed across instances");
	CHECK(rows[1].district == "Waterfront", "Waterfront row second");
	CHECK(rows[1].instances == 1 && rows[1].total_players == 5, "Waterfront single instance pop");
	// An evicted instance drops out of the aggregate immediately.
	dir.Deregister(1002);
	auto rows2 = dir.AggregateByDistrict();
	CHECK(rows2[0].instances == 1 && rows2[0].total_players == 55, "eviction updates aggregate");
}

static void TestPopulationSnapshotCopy() {
	DistrictDirectory dir;
	dir.Register("Financial", 1001, 7801, 0);
	dir.Register("Financial", 1002, 7802, 0);
	dir.ReportLoad(1001, 55, 100);
	dir.ReportLoad(1002, 20, 100);

	auto aggregates = dir.AggregateByDistrict();
	const auto snapshots = MakeDistrictPopulationSnapshots(aggregates);
	aggregates[0].district = "Changed";
	aggregates[0].instances = 0;
	aggregates[0].total_players = 0;
	CHECK(snapshots.size() == 1, "snapshot copy preserves one aggregate row");
	if (snapshots.size() == 1)
	{
		CHECK(snapshots[0].DistrictId == "Financial", "snapshot copy preserves district id");
		CHECK(snapshots[0].InstanceCount == 2, "snapshot copy preserves instance count");
		CHECK(snapshots[0].Population == 75, "snapshot copy preserves summed population");
	}
}

int main() {
	std::printf("=== APB District Directory Tests (M16 heartbeat eviction) ===\n");
	TestRegisterAndFind();
	TestHeartbeatLiveness();
	TestStaleEviction();
	TestLeastLoaded();
	TestApplyRelay();
	TestDeregister();
	TestAggregateByDistrict();
	TestPopulationSnapshotCopy();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
