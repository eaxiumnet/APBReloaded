# M16 — Server-Authoritative Anti-Cheat Heuristics (Domain) — Handoff Note

**Author:** Qoder  **Date:** 2026-07-20  **Milestone:** M16 (brief #15)
**Status:** Anti-cheat heuristic Domain layer COMPLETE + proven. Server-hardening ops
(launch scripts, heartbeats, password hashing) remain — see "NOT done here".

## What landed

Pure-C++17 Domain service (no UE/platform headers), unit-tested via `tests/build_and_run.ps1`.

- `Source/APBReloaded/Domain/APBAntiCheat.h` / `.cpp`
- `tests/run_anticheat_tests.cpp` → wired as `$exe16` (`APBAntiCheatTests`).
- All 16 domain suites green (`FAILS=0`), including the new
  `=== APB Anti-Cheat Tests (M16 server heuristics) ===`.

## Design posture (ARCHITECTURE.md §9)

This is NOT a kernel-level AC product (retail GFAC/EAC/BattlEye are out of scope for a
private port). It is the **server-side sanity layer** that runs on the district authority
on top of the already server-authoritative Domain (validated RPCs, server-side
`ResolveShot`, no client-trusted economy).

**All checks are deterministic and take caller-supplied clocks/samples — no wall-clock
reads.** This matches the rest of the Domain so heuristics replay identically in tests and
on the server (feed them the server tick time / RPC timestamps).

## API surface (`namespace apb`)

### `MovementValidator` — speed / teleport heuristics
- `MoveSample{x, y, t_seconds}` — planar sample, matching the `APBCombat.h` x/y model.
- `Check(prev, cur, max_speed)` → `MoveVerdict{Ok, SpeedViolation, Teleport}`.
- Single-step jump `>= teleport_distance` (default **5000** units) → `Teleport` regardless
  of dt. Non-positive dt (same/rewound timestamp) with movement → `Teleport`.
- Speed `> max_speed * speed_tolerance` (default **1.25**, absorbs latency/physics spikes)
  → `SpeedViolation`. Zero `max_speed` + any movement → `SpeedViolation`.

### `FireRateValidator` — per-weapon minimum shot interval
- `MinShotIntervalMs(rpm)` → `60000/rpm` (rpm<=0 → 0 = no limit).
- `CheckShot(now_ms, rpm)` → `Ok` or `FireRateViolation`. Accepts shots at
  `interval_tolerance` (default **0.85**) of nominal to allow jitter. **A rejected shot does
  NOT advance `last_shot_ms`**, so a sustained burst keeps failing until it legitimately
  slows. `Reset()` clears the timer (weapon swap / respawn).

### `ShotAnomalyCheck` — reported damage / range vs catalog
- `Check(ItemDef weapon, reported_damage, distance)` → `ShotVerdict`.
- `reported_damage > weapon.damage * damage_tolerance` (default **1.10**) → `DamageAnomaly`
  (**checked first**). `distance > weapon.max_range * range_tolerance` (default **1.10**) →
  `OutOfRange`. Zero catalog values disable the respective check.

### `AnomalyLog` — weighted violation accumulator → escalating sanction
- `Record(weight)` (weight<=0 ignored) accumulates `violations`; `Current()` maps to
  `Sanction{None, Warn, Kick, Ban}` via thresholds `warn_at`=3, `kick_at`=6, `ban_at`=12.
- Per-player; heavier heuristics can pass a larger `weight` to escalate faster (a single
  weight-20 record → `Ban`). `Reset()` on session end / appeal.

### Tolerance defaults are tunable operational values
`teleport_distance`/`speed_tolerance`/`interval_tolerance`/`damage_tolerance`/
`range_tolerance` and the `AnomalyLog` thresholds are recreation defaults, not retail-sourced
constants. Retune once real movement/weapon telemetry is captured; they are plain struct
fields so callers can override per-district or per-weapon.

## Integration points (for the district-server / UE-side agents)
- **Movement:** on each authoritative movement update/RPC, feed the previous and current
  server-side `MoveSample` (+ the entity's `max_speed`) into a per-player `MovementValidator`;
  on a non-`Ok` verdict, `AnomalyLog.Record` (teleport heavier than speed) and act on the
  returned `Sanction`.
- **Firing:** on each server-side shot (alongside `ResolveShot`), call the per-player
  `FireRateValidator.CheckShot(tick_ms, weapon.rpm)` and `ShotAnomalyCheck.Check(weapon,
  server_computed_damage, engagement_distance)`; funnel non-`Ok` verdicts into `AnomalyLog`.
- **Sanctions:** map `Warn`→server message, `Kick`→disconnect, `Ban`→persist + directory
  eviction. Wire through the district GameMode, not client polling.

## NOT done here (deliberately out of Domain scope)
- **Password hashing** — already owned by the auth path (`Domain/APBCrypto.h` PBKDF2 +
  `Domain/APBTicket`); M16's ARCHITECTURE.md line is satisfied there, not re-implemented in
  anti-cheat. TLS-on-localhost (see §R3) also remains deferred ops work.
- **Launch / crash-recovery ops:** `tools\scripts\start_world.ps1` / `start_district.ps1`,
  heartbeat → world-directory eviction on `kill -9`, fresh-server bootstrap from clean
  `Saved\`. These are process/ops deliverables of the M16 brief, separate from the pure
  heuristic logic that landed here.
- **UE bridge:** calling these validators from the district GameMode/subsystem on the real
  movement/shot RPC path, sanction enforcement, and admin/telemetry surfacing.

## Build/verify
```
powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1
```
`$exe16 = APBAntiCheatTests` compiles `APBAntiCheat.cpp + run_anticheat_tests.cpp`
(no `APBCatalog.cpp` — `ItemDef` is header-only in `APBTypes.h`). All 16 suites `FAILS=0`.
