# M11 — Mission stage countdown timers (Domain)

**Owner:** Qoder · **Status:** Domain layer landed + tested (17/17 suites FAILS=0). UE-side wiring pending.

## Why
APB mission stages carry a countdown clock — if you fail to complete a timed stage before it runs
out, the mission fails. `MissionStageDef.time_limit_sec` was already **parsed** from mission JSON
(`APBMission.cpp` stage loader) but was **dead data**: nothing ever enforced it. This closes that
1:1 gameplay gap without touching the existing progress path.

## What changed (additive, merge-friendly — no signature changes to Start/Progress/AdvanceMission)
- **`Domain/APBMission.h`** — `MissionRun` gained:
  - `double current_stage_deadline_sec = 0;` — absolute deadline of the armed stage (0 = unarmed).
  - `int32_t timed_stage_index = -1;` — which stage the deadline belongs to (re-arms on advance).
  - `bool timed_out = false;` — set when a stage clock expires.
  - `bool CheckTimeout(double now_sec);`
- **`Domain/APBMission.cpp`** — `MissionRun::CheckTimeout(now_sec)`:
  - No-op unless `status==Active` with a `Current()` stage.
  - Stage with `time_limit_sec <= 0` → disarmed, never times out.
  - First tick on a new current stage **arms** the deadline (`now_sec + time_limit_sec`) and returns
    false (so arming and expiry never happen on the same tick).
  - Once armed, `now_sec > deadline` → `timed_out=true`, `status=Failed`, returns `true`.
  - **Lazy arming from the tick clock is what avoids changing `Start`/`Progress` signatures** — the
    timer re-arms automatically when `current_index` advances to a new stage.
- **`Domain/APBWorldService.{h,cpp}`** — `WorldService::TickMission(now_sec)`:
  - Calls `mission->CheckTimeout(now_sec)`; on a decided timeout applies `threat.ApplyMissionFail()`
    and logs `MISSION_TIMEOUT stage=<i>`.
- **`DomainSnapshot`** now exposes `mission_timed_out` and `mission_stage_time_limit_sec`
  (current stage's countdown length; 0 = no timer) for the HUD countdown.

## Tests
`tests/run_domain_tests.cpp` → `TestMissionStageTimeout()` (registered in `main`, run via
`tests\build_and_run.ps1`). Covers: arm tick doesn't fail; snapshot exposes the limit; within-window
tick keeps the mission Active; a tick past the deadline fails the mission with `timed_out`; a
no-limit stage never times out even at `now=1e9`. All 17 domain suites report `FAILS=0`.

## Interplay with the opposed-mission race (`m11_opposition_race_note.md`)
`TickMission` (stage clock) and `AdvanceOpposition` (opposition score race) are independent
server-authoritative deciders on the same `MissionRun` — either can move it to `Failed` first.
Both are deterministic (caller supplies the clock/amount).

## Remaining (UE-side, for whoever picks up the district GameMode)
- Feed `WorldService::TickMission(now_sec)` from the district GameMode's authoritative clock each
  tick (alongside `AdvanceMission`/`AdvanceOpposition`), not a wall-clock read inside Domain.
- Bind `mission_stage_time_limit_sec` + a client-side countdown to the mission HUD; surface
  `mission_timed_out` as the failure reason.
- Populate real per-stage `time_limit_sec` values when importing retail `MissionTemplates.INT`
  (the loader already reads the field; timed stages will "just work" once the data carries limits).
