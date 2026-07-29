#pragma once
// APBAntiCheat.h — M16 (brief #15): pure-C++17 server-authoritative anti-cheat heuristics.
// No UE/platform headers — unit-testable in isolation like the other Domain services.
//
// Design posture per ARCHITECTURE.md §9: this is NOT a kernel-level AC product. It is the
// server-side sanity layer that runs on the district authority on top of the already
// server-authoritative Domain (validated RPCs, server-side ResolveShot, no client economy):
//   1. MovementValidator — speed / teleport heuristics from position samples + max speed.
//   2. FireRateValidator — per-weapon minimum shot interval derived from catalog RPM.
//   3. ShotAnomalyCheck — reported damage / engagement range vs the weapon's catalog limits.
//   4. AnomalyLog       — weighted violation accumulator -> escalating sanction (warn/kick/ban).
//
// All checks are deterministic and take caller-supplied clocks/samples (no wall-clock reads),
// matching the rest of the Domain so they replay identically in tests and on the server.
#include "APBTypes.h" // ItemDef, Faction
#include <cstdint>

namespace apb {

// ---- Movement -------------------------------------------------------------------------

enum class MoveVerdict { Ok, SpeedViolation, Teleport };
const char* MoveVerdictName(MoveVerdict v);

// A time-stamped 2D position sample (the Domain combat model is planar: x/y).
struct MoveSample {
	double x = 0;
	double y = 0;
	double t_seconds = 0;
};

struct MovementValidator {
	// A single-step jump farther than this (regardless of dt) is treated as a teleport.
	double teleport_distance = 5000.0;
	// Allow this multiple of max_speed to absorb latency / physics spikes before flagging.
	double speed_tolerance = 1.25;

	// Validate motion between two samples given the entity's max ground speed (units/sec).
	// Non-positive dt (same/rewound timestamp) with any movement is a teleport.
	MoveVerdict Check(const MoveSample& prev, const MoveSample& cur, double max_speed) const;
};

// ---- Fire rate ------------------------------------------------------------------------

enum class ShotVerdict { Ok, FireRateViolation, DamageAnomaly, OutOfRange };
const char* ShotVerdictName(ShotVerdict v);

// Nominal minimum ms between shots for a given rounds-per-minute. rpm<=0 -> 0 (no limit).
double MinShotIntervalMs(double rpm);

struct FireRateValidator {
	// Accept shots arriving at this fraction of the nominal interval (latency jitter).
	double interval_tolerance = 0.85;
	int64_t last_shot_ms = -1; // -1 = no prior shot

	// Register a shot at now_ms for a weapon with the given rpm. Returns Ok or
	// FireRateViolation; only advances last_shot_ms when the shot is accepted.
	ShotVerdict CheckShot(int64_t now_ms, double rpm);
	void Reset() { last_shot_ms = -1; }
};

// ---- Shot damage / range anomaly ------------------------------------------------------

struct ShotAnomalyCheck {
	double damage_tolerance = 1.10; // reported damage may exceed catalog max by up to 10%
	double range_tolerance = 1.10;  // engagement distance may exceed max_range by up to 10%

	// Validate a server-reported shot against the weapon's catalog limits.
	ShotVerdict Check(const ItemDef& weapon, double reported_damage, double distance) const;
};

// ---- Sanction escalation --------------------------------------------------------------

enum class Sanction { None, Warn, Kick, Ban };
const char* SanctionName(Sanction s);

// Per-player weighted violation accumulator. Thresholds are tunable operational defaults.
struct AnomalyLog {
	int32_t violations = 0;
	int32_t warn_at = 3;
	int32_t kick_at = 6;
	int32_t ban_at = 12;

	// Add weighted violations (weight<=0 ignored); returns the current recommended sanction.
	Sanction Record(int32_t weight = 1);
	Sanction Current() const;
	void Reset() { violations = 0; }
};

} // namespace apb
