// APBAntiCheat.cpp — M16 (brief #15): implementation of the server-authoritative anti-cheat
// heuristics declared in APBAntiCheat.h. Pure logic; no I/O, no wall-clock reads.
#include "APBAntiCheat.h"
#include <cmath>

namespace apb {

const char* MoveVerdictName(MoveVerdict v) {
	switch (v) {
		case MoveVerdict::Ok:             return "Ok";
		case MoveVerdict::SpeedViolation: return "SpeedViolation";
		case MoveVerdict::Teleport:       return "Teleport";
		default:                          return "Unknown";
	}
}

const char* ShotVerdictName(ShotVerdict v) {
	switch (v) {
		case ShotVerdict::Ok:                return "Ok";
		case ShotVerdict::FireRateViolation: return "FireRateViolation";
		case ShotVerdict::DamageAnomaly:     return "DamageAnomaly";
		case ShotVerdict::OutOfRange:        return "OutOfRange";
		default:                             return "Unknown";
	}
}

const char* SanctionName(Sanction s) {
	switch (s) {
		case Sanction::None: return "None";
		case Sanction::Warn: return "Warn";
		case Sanction::Kick: return "Kick";
		case Sanction::Ban:  return "Ban";
		default:             return "Unknown";
	}
}

MoveVerdict MovementValidator::Check(const MoveSample& prev, const MoveSample& cur,
	double max_speed) const {
	const double dx = cur.x - prev.x;
	const double dy = cur.y - prev.y;
	const double dist = std::sqrt(dx * dx + dy * dy);

	// A large single-step jump is a teleport regardless of the reported dt.
	if (dist >= teleport_distance) return MoveVerdict::Teleport;

	const double dt = cur.t_seconds - prev.t_seconds;
	if (dt <= 0.0) {
		// No time elapsed (or a rewound clock) but the entity moved -> teleport.
		return dist > 0.0 ? MoveVerdict::Teleport : MoveVerdict::Ok;
	}

	if (max_speed <= 0.0) {
		// No speed budget: any movement is a violation.
		return dist > 0.0 ? MoveVerdict::SpeedViolation : MoveVerdict::Ok;
	}

	const double speed = dist / dt;
	if (speed > max_speed * speed_tolerance) return MoveVerdict::SpeedViolation;
	return MoveVerdict::Ok;
}

double MinShotIntervalMs(double rpm) {
	if (rpm <= 0.0) return 0.0;
	return 60000.0 / rpm;
}

ShotVerdict FireRateValidator::CheckShot(int64_t now_ms, double rpm) {
	const double nominal = MinShotIntervalMs(rpm);
	if (nominal <= 0.0 || last_shot_ms < 0) {
		// No rate limit, or the first observed shot: accept and arm the timer.
		last_shot_ms = now_ms;
		return ShotVerdict::Ok;
	}
	const int64_t elapsed = now_ms - last_shot_ms;
	const double allowed = nominal * interval_tolerance;
	if ((double)elapsed + 1e-9 < allowed) {
		// Too fast — reject and do NOT advance the timer (so a burst keeps failing).
		return ShotVerdict::FireRateViolation;
	}
	last_shot_ms = now_ms;
	return ShotVerdict::Ok;
}

ShotVerdict ShotAnomalyCheck::Check(const ItemDef& weapon, double reported_damage,
	double distance) const {
	if (weapon.damage > 0.0 && reported_damage > weapon.damage * damage_tolerance) {
		return ShotVerdict::DamageAnomaly;
	}
	if (weapon.max_range > 0.0 && distance > weapon.max_range * range_tolerance) {
		return ShotVerdict::OutOfRange;
	}
	return ShotVerdict::Ok;
}

Sanction AnomalyLog::Current() const {
	if (violations >= ban_at)  return Sanction::Ban;
	if (violations >= kick_at) return Sanction::Kick;
	if (violations >= warn_at) return Sanction::Warn;
	return Sanction::None;
}

Sanction AnomalyLog::Record(int32_t weight) {
	if (weight > 0) violations += weight;
	return Current();
}

} // namespace apb
