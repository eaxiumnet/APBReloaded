// run_anticheat_tests.cpp — M16 (brief #15): server-authoritative anti-cheat heuristic tests.
// Links only APBAntiCheat.cpp (ItemDef is header-only in APBTypes.h).
// Pattern mirrors the other run_*_tests.cpp.
#include "APBAntiCheat.h"
#include <cstdio>
#include <string>

using namespace apb;

static int fails = 0;
#define CHECK(cond, msg) do { if (!(cond)) { ++fails; std::printf("FAIL: %s\n", msg); } \
    else { std::printf("PASS: %s\n", msg); } } while (0)

static MoveSample S(double x, double y, double t) { MoveSample s; s.x = x; s.y = y; s.t_seconds = t; return s; }

static void TestMovement() {
	MovementValidator mv; // teleport 5000, tolerance 1.25
	const double maxs = 1000.0; // units/sec -> allowed 1250

	CHECK(mv.Check(S(0, 0, 0), S(1000, 0, 1), maxs) == MoveVerdict::Ok, "at max speed -> Ok");
	CHECK(mv.Check(S(0, 0, 0), S(1200, 0, 1), maxs) == MoveVerdict::Ok, "within tolerance -> Ok");
	CHECK(mv.Check(S(0, 0, 0), S(1300, 0, 1), maxs) == MoveVerdict::SpeedViolation, "over tolerance -> SpeedViolation");
	CHECK(mv.Check(S(0, 0, 0), S(6000, 0, 1), maxs) == MoveVerdict::Teleport, "big jump -> Teleport (even with dt)");
	CHECK(mv.Check(S(0, 0, 5), S(100, 0, 5), maxs) == MoveVerdict::Teleport, "movement with zero dt -> Teleport");
	CHECK(mv.Check(S(3, 3, 5), S(3, 3, 5), maxs) == MoveVerdict::Ok, "no movement, zero dt -> Ok");
	CHECK(mv.Check(S(0, 0, 5), S(0, 0, 4), maxs) == MoveVerdict::Ok, "rewound clock, no movement -> Ok");
	CHECK(mv.Check(S(0, 0, 0), S(10, 0, 1), 0.0) == MoveVerdict::SpeedViolation, "zero max_speed + movement -> SpeedViolation");
	CHECK(mv.Check(S(0, 0, 0), S(0, 0, 1), 0.0) == MoveVerdict::Ok, "zero max_speed, no movement -> Ok");
	// Diagonal distance uses hypot.
	CHECK(mv.Check(S(0, 0, 0), S(900, 900, 1), maxs) == MoveVerdict::SpeedViolation, "diagonal ~1273 u/s over tolerance");
}

static void TestFireRate() {
	CHECK(MinShotIntervalMs(600) == 100.0, "600 rpm -> 100ms interval");
	CHECK(MinShotIntervalMs(0) == 0.0, "0 rpm -> no limit");

	FireRateValidator fr; // tolerance 0.85 -> allowed 85ms for 600 rpm
	CHECK(fr.CheckShot(0, 600) == ShotVerdict::Ok, "first shot Ok");
	CHECK(fr.CheckShot(50, 600) == ShotVerdict::FireRateViolation, "50ms after -> too fast");
	CHECK(fr.last_shot_ms == 0, "rejected shot does not advance timer");
	CHECK(fr.CheckShot(90, 600) == ShotVerdict::Ok, "90ms after first -> Ok");
	CHECK(fr.last_shot_ms == 90, "accepted shot advances timer");
	CHECK(fr.CheckShot(200, 600) == ShotVerdict::Ok, "well-spaced shot Ok");

	// No rate limit weapon never violates.
	FireRateValidator fr2;
	CHECK(fr2.CheckShot(0, 0) == ShotVerdict::Ok, "rpm 0 first shot Ok");
	CHECK(fr2.CheckShot(1, 0) == ShotVerdict::Ok, "rpm 0 second shot Ok (no limit)");
}

static void TestShotAnomaly() {
	ItemDef w; w.id = "test_gun"; w.damage = 200.0; w.max_range = 5000.0; w.rpm = 600.0;
	ShotAnomalyCheck ac; // damage tol 1.10 -> 220, range tol 1.10 -> 5500

	CHECK(ac.Check(w, 200.0, 100.0) == ShotVerdict::Ok, "nominal damage/range Ok");
	CHECK(ac.Check(w, 220.0, 100.0) == ShotVerdict::Ok, "damage at tolerance edge Ok");
	CHECK(ac.Check(w, 250.0, 100.0) == ShotVerdict::DamageAnomaly, "excess damage flagged");
	CHECK(ac.Check(w, 200.0, 5500.0) == ShotVerdict::Ok, "range at tolerance edge Ok");
	CHECK(ac.Check(w, 200.0, 6000.0) == ShotVerdict::OutOfRange, "excess range flagged");
	// Damage checked before range.
	CHECK(ac.Check(w, 999.0, 99999.0) == ShotVerdict::DamageAnomaly, "damage anomaly takes precedence");
}

static void TestSanctionEscalation() {
	AnomalyLog log; // warn 3, kick 6, ban 12
	CHECK(log.Current() == Sanction::None, "clean player -> None");
	CHECK(log.Record(1) == Sanction::None, "1 violation -> None");
	CHECK(log.Record(1) == Sanction::None, "2 violations -> None");
	CHECK(log.Record(1) == Sanction::Warn, "3 violations -> Warn");
	log.Record(2); // 5
	CHECK(log.Current() == Sanction::Warn, "5 violations -> still Warn");
	CHECK(log.Record(1) == Sanction::Kick, "6 violations -> Kick");
	log.Record(5); // 11
	CHECK(log.Current() == Sanction::Kick, "11 violations -> still Kick");
	CHECK(log.Record(1) == Sanction::Ban, "12 violations -> Ban");

	// Negative/zero weight ignored; reset clears.
	int32_t before = log.violations;
	log.Record(0);
	log.Record(-5);
	CHECK(log.violations == before, "non-positive weight ignored");
	log.Reset();
	CHECK(log.Current() == Sanction::None, "reset clears violations");

	// A single heavy violation can escalate straight to Ban.
	AnomalyLog log2;
	CHECK(log2.Record(20) == Sanction::Ban, "heavy single violation -> Ban");
}

int main() {
	std::printf("=== APB Anti-Cheat Tests (M16 server heuristics) ===\n");
	TestMovement();
	TestFireRate();
	TestShotAnomaly();
	TestSanctionEscalation();
	std::printf("FAILS=%d\n", fails);
	return fails ? 1 : 0;
}
