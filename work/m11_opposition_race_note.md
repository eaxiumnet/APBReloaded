# M11 — Opposed-mission score race (Domain)

**Owner:** Qoder · **Status:** Domain layer landed + tested (17/17 suites FAILS=0). UE-side wiring pending.

## Why
APB opposed missions are a **symmetric race**: both teams contest the same stage objective and
*either* side can secure it first. The previous WorldService mission loop modeled the opposition
only cosmetically — `MissionRun::opposition_contesting` was a display flag, and the only way the
player could lose was `RegisterOppositionTakeout()` hitting `takeout_fail_at`. There was no path
for the opposition to *win the objective* and fail the mission. This closes that 1:1 gameplay gap.

## What changed (additive, merge-friendly — no signature/behavior changes to existing paths)
- **`Domain/APBMission.h`**
  - `MissionStageRuntime` gained `double opp_progress = 0;` (opposition's accrual on this stage).
  - `MissionRun` gained `bool opposition_won = false;` and `bool AdvanceOpposition(double amount = 1.0);`.
- **`Domain/APBMission.cpp`** — `MissionRun::AdvanceOpposition(amount)`:
  - No-op unless `status==Active`, `opposition_contesting`, and `amount>0`.
  - Accrues/clamps `opp_progress` on `Current()` stage (mirrors the `Progress()` accrual pattern).
  - When it reaches `target_progress` → `opposition_won=true`, `status=Failed`, returns `true`.
- **`Domain/APBWorldService.{h,cpp}`** — `WorldService::AdvanceOpposition(amount)`:
  - Scales input by `max(0.5, OppositionPressure())` (stronger threat tier ⇒ opposition closes faster).
  - On a decided loss applies `threat.ApplyMissionFail()` and logs `MISSION_OPPOSITION_WON stage=<i>`.
- **`DomainSnapshot`** now exposes (for HUD race bar / PlayerState sync):
  `mission_opposition_contesting`, `mission_opposition_won`, `mission_stage_progress`,
  `mission_opp_stage_progress` (owner/opposition current-stage fractions in [0..1]).

## Tests
`tests/run_domain_tests.cpp` → `TestOppositionRace()` (registered in `main`, run via
`tests\build_and_run.ps1`). Covers: contested flag exposed; opposition accrues to the objective and
fails the mission with `opposition_won`; **owner can still win the race if faster**; a completed
mission cannot be flipped by late opposition progress. All 17 domain suites report `FAILS=0`.

## Remaining (UE-side, for whoever picks up the district GameMode)
- Drive `WorldService::AdvanceOpposition(dt-scaled)` from the district opposed-mission ticker
  (server-authoritative), symmetric with how the owning team calls `AdvanceMission`.
- Bind the four new snapshot fields into the mission HUD as a two-sided race bar.
- `AdvanceOpposition` is deterministic (caller supplies the amount) — keep the tick source in the
  GameMode, not a wall-clock read inside Domain.
