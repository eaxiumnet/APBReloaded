# M11 Mission Gate Findings

**Created:** 2026-07-29
**Status:** ✅ GREEN — fresh `M11_MISSION_GATE_OK` on 2026-07-29 (PCG-blocker diagnosis corrected below)

## Summary

The M11 UE-side mission wiring (S1 stage timeout, S2 opposition race, S3 regression) is
**validated GREEN** against the last successful verification gate run (2026-07-26,
`C:\Users\Support\AppData\Local\Temp\grok-goal-4ec7b7726483\implementer\`). A fresh runtime
re-run is blocked by an engine-level PCG plugin crash that appeared after 2026-07-26.

## Evidence (from 2026-07-26 gate run)

### S1 — Stage timeout (happy path) ✅

**Host `client_loop.log`:**
```
TIMEOUT_DRIVE armed_timed_out=0 limit=300 deadline=300 breached=1 timed_out=1 status=Failed stage=0/5 pre_threat=0.0 post_threat=0.0
TIMEOUT_PS_READBACK timed_out=1 limit=0 deadline=300 mission=2,4,6-TRINITROTOLUENE stage=0/5
```

- `armed_timed_out=0`: first `TickMission(0)` arms the deadline, does not breach ✓
- `breached=1`: second `TickMission(deadline+1)` breaches the deadline ✓
- `timed_out=1`: mission failed by timeout ✓
- `TIMEOUT_PS_READBACK timed_out=1`: replicated PlayerState carries `timed_out=true` ✓

### S2 — Opposition race (edge) ✅

**Host `client_loop.log`:**
```
OPP_WIN_DRIVE title=2,4,6-TRINITROTOLUENE stage=3/5 status=Failed opp_prog=0.00 opp_won=1 threat=17.0 opp_ticks=10
```

- `opp_won=1`: opposition won the stage race → mission Failed ✓
- `opp_ticks=10`: took 10 `AdvanceOpposition(1.0)` calls to win ✓
- `status=Failed`: mission failed for the owner (opposition decided) ✓

### Peer mission replication ✅

**Client `mp_client_observe.log`:**
```
MP_POLL player=maras-23C462A14F2D80 threat=22.0 cash=10077 g1c=4820 inv=1 mission=2,4,6-TRINITROTOLUENE stage=3/5 session=DS-Financial-1 stage_prog=0.00 opp_prog=0.00 contesting=1 opp_won=1 timed_out=0 deadline=0.0
CLIENT_OBS mission=2,4,6-TRINITROTOLUENE stage=3/5 session=DS-Financial-1 player=maras-23C462A14F2D80 stage_prog=0.00 opp_prog=0.00 contesting=1 opp_won=1 timed_out=0 deadline=0.0
```

- `mission=2,4,6-TRINITROTOLUENE`: non-empty mission title replicated to peer ✓
- `stage=3/5`: mission stage index/count replicated ✓
- `opp_won=1`: opposition-won flag replicated to peer via unconditional DOREPLIFETIME ✓
- `contesting=1`: opposition-contesting flag replicated ✓
- Read **from peer** (CLIENT_OBS), proving OnRep delivered mission state cross-client ✓

### S3 — Regression ✅

**Host `client_loop.log`:**
```
VEHICLE_DOMAIN spawn=1 possess=1
FIRE_SYNC ok=1 mutated=1 moved_off_stale=1 parity=1 stale_threat=17.0 domain_diverged=22.0 ps_after=22.0 domain_final=22.0
```

- Vehicle spawn+possess unchanged ✓
- FireWeaponLocal sync bridge unchanged ✓

## Engine PCG crash blocker (fresh re-run)

**Root cause:** Engine-level PCG plugin fatal assertion during startup:
```
Assertion failed: RegistryPtr && SettingsStaticClass
[File: D:\build\++UE5\Sync\Engine\Plugins\PCG\Source\PCG\Private\Tests\Determinism\PCGDeterminismNativeTests.cpp] [Line: 32]
SecondsSinceStart: 0
```

This crash occurs at engine init time (`SecondsSinceStart: 0`), before the game module
loads. It is **not caused by M11 code changes** — it's an engine plugin bug that appeared
between the last successful gate (2026-07-26) and the crash logs starting 2026-07-28.

**Workaround applied:** Added `{"Name":"PCG","Enabled":false}` to `APBReloaded.uproject`.
The project does not use PCG (it uses custom manifest-based chunk streaming via
`APBDistrictPlacementLoader`). The `.uproject` change compiles successfully (UBT
`Result: Succeeded`), but the installed binary engine still loads PCG native tests during
startup — the `.uproject` disable prevents the plugin from being linked but does not
prevent the engine's native test runner from executing the PCG determinism tests.

**To fully unblock fresh runs:** The PCG native tests need to be disabled at the engine
level (e.g., `-NoNativeTests` command-line flag or a `DefaultEngine.ini` setting). This
is an engine configuration issue, not a code issue — per AGENTS.md rule 6, engine source
files are external and cannot be edited.

## Changes made this session

1. **`tools/run_m11_mission_gate.ps1`** (NEW) — Standalone 2-client listen-server mission
   gate that asserts S1 (TIMEOUT_DRIVE breached=1 timed_out=1), S2 (OPP_WIN_DRIVE
   opp_won=1), S3 (VEHICLE_DOMAIN + FIRE_SYNC), and peer mission replication.

2. **`tools/run_verification_gates.ps1`** — Added step 9b calling
   `run_m11_mission_gate.ps1` between m7_directory_gate and m16_persistence_gate, with
   `Require-Fresh` for `M11_MISSION_GATE_OK` + `m11_mission_gate` key in gate_summary.json.

3. **`APBReloaded.uproject`** — Added `{"Name":"PCG","Enabled":false}` to disable the PCG
   plugin (workaround for engine crash; project doesn't use PCG).

## Acceptance contract status

| Scenario | Status | Evidence source |
|---|---|---|
| S1 stage-timeout | ✅ VALIDATED | 2026-07-26 client_loop.log TIMEOUT_DRIVE + TIMEOUT_PS_READBACK |
| S2 opposition race | ✅ VALIDATED | 2026-07-26 client_loop.log OPP_WIN_DRIVE opp_won=1 |
| S3 regression | ✅ VALIDATED | 2026-07-26 client_loop.log VEHICLE_DOMAIN + FIRE_SYNC |
| Peer replication | ✅ VALIDATED | 2026-07-26 mp_client_observe.log CLIENT_OBS opp_won=1 mission=2,4,6-TRINITROTOLUENE |
| Fresh gate run | ✅ GREEN 2026-07-29 | m11_mission_gate.log M11_MISSION_GATE_OK (see correction below) |

## CORRECTION 2026-07-29: PCG blocker diagnosis was wrong — real cause was M16 secret preflight

The "engine PCG crash blocker" section above is **mis-diagnosed**. Evidence from a fresh
root-cause pass (crash dir `UECC-Windows-79A07ADC4E84DFD64A53E2AC869937D5_0000`):

1. **The PCG assert is a shutdown-path event, not an init blocker.** In every crash log
   containing it, `Engine exit requested (reason: Win RequestExit)` precedes the PCG
   `PCGDataViewRegistry` unregister ensures and the `RegistryPtr && SettingsStaticClass`
   appError. `SecondsSinceStart: 0` is a crash-context field, not proof of init-time.
   The assert also already occurred on **2026-07-23** (crash dir
   `UECC-Windows-96CF45774B34A136713CBA9CFCF64DFF_0003`), i.e. *before* the "last good"
   2026-07-26 gate — it never blocked anything.

2. **The real blocker:** the M16 zero-trust secret provider. Any launch whose command
   line contains `?listen` is classified role=`district` by
   `FAPBSecretProvider::DeploymentRole()` (`APBSecretProvider.cpp`), and without
   `APB_DEPLOYMENT_SECRET` set the preflight halts the process:
   ```
   DEPLOYMENT_SECRET_PROVIDER_HALT reason=missing_secret role=district listener=not_started
   FPlatformMisc::RequestExitWithStatus(0, 1, FAPBSecretProvider::PreflightRole)
   ```
   That `RequestExitWithStatus` IS the mysterious `Win RequestExit`. This preflight
   landed with M16 zero-trust *after* 2026-07-26 — which is exactly why gates ran on
   26/07 and died on 28/07+. Newer gates (`run_m6_world_gate.ps1`,
   `run_m7_directory_gate.ps1`, `run_m16_zerotrust_gate.ps1`) already set
   `APB_DEPLOYMENT_SECRET = 'a1'*32` in-process; `run_m11_mission_gate.ps1` predated
   that pattern and never got it.

3. **Fix applied:** `tools/run_m11_mission_gate.ps1` now sets/restores
   `APB_DEPLOYMENT_SECRET` (same `'a1'*32` pattern as the other gates).

4. **Fresh result (2026-07-29 10:48):** `M11_MISSION_GATE_OK` — S1_TIMEOUT_HOST_OK,
   S2_OPP_RACE_HOST_OK, S3_REGRESSION_OK, MP_OBSERVE_CONNECTED,
   PEER_MISSION_REPLICATION_OK. Scratch:
   `C:\Users\Support\AppData\Local\Temp\grok-goal-m11-mission\implementer\m11_mission_gate.log`.

The `.uproject` PCG disable can stay (the project does not use PCG), but it was never
the fix and `-NoNativeTests`/engine-level changes are NOT needed.
