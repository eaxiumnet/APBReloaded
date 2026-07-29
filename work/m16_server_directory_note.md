# M16 — World-Side District Directory + Heartbeat Eviction (Domain) — Handoff Note

**Author:** Qoder  **Date:** 2026-07-20  **Milestone:** M16 (brief #15)
**Status:** The *logic* behind the M16 "heartbeat → directory eviction" gate landed as a
tested Domain service. The M16 ops scripts (`bootstrap_server.ps1`, `start_world.ps1`,
`start_district.ps1`) now exist and are DryRun/lifecycle-VERIFIED (see "Ops scripts"). The UE
process wiring (relay socket loop + world tick calling it) and the end-to-end kill -9
integration gate remain — see "NOT done here".

## What landed

Pure-C++17 Domain service (no UE/platform headers), unit-tested via `tests/build_and_run.ps1`.

- `Source/APBReloaded/Domain/APBDistrictDirectory.h` / `.cpp`
- `tests/run_directory_tests.cpp` → wired as `$exe17` (`APBDistrictDirectoryTests`).
- All 17 domain suites green (`FAILS=0`), including the new
  `=== APB District Directory Tests (M16 heartbeat eviction) ===`.

## Why this exists / relationship to existing work
The M16 verify gate is *"kill -9 district → world directory reflects exit ≤ 2 heartbeats"*.
The **message format** for the world↔district control channel already exists
(`APBRelayProtocol.h`, M7 N4: `Register/Heartbeat/ReportLoad/PlayerJoined/PlayerLeft`), but
nothing on the world side **consumed** those messages to track district-server liveness and
evict a silent instance. `DistrictDirectory` is that consumer.

Distinct from the two existing directory-ish types (do NOT merge):
- `APBSocial.h::WorldDirectory` — list of *world instances* (W1/W2) for the pre-district
  world list. Not district-server processes.
- `APBSocial.h::DistrictRouter` — *player* population / queue / reservation. Not liveness.

`DistrictDirectory` tracks the actual **district-server processes** the world has to route
joining players to, keyed by apbdb `numeric_id`.

## API surface (`namespace apb`)
- `DistrictNode{district, numeric_id, port, player_count, registered_ms, last_heartbeat_ms, alive}`.
- Ingestion (world relay recv loop calls one per decoded message):
  - `Register(district, numeric_id, port, now_ms)` — insert/refresh; re-register updates
    port + re-arms liveness but keeps first-seen `registered_ms`.
  - `Heartbeat(numeric_id, now_ms)` — advance liveness; returns false for unknown/evicted
    (no resurrection — the world asks it to Register again).
  - `ReportLoad(numeric_id, player_count, now_ms)` — occupancy (clamped ≥0) + liveness.
  - `Deregister(numeric_id)` — graceful shutdown removal.
  - `Apply(const RelayMessage&, now_ms)` — dispatches the above by verb; `PlayerJoined`/
    `PlayerLeft` also nudge `player_count`; non-directory verbs return false.
- World tick: `PruneStale(now_ms)` — evicts every node silent longer than
  `StaleThresholdMs()` = `heartbeat_interval_ms * eviction_multiple` (defaults **5000ms × 2 =
  10000ms**, i.e. >2 missed beats); returns eviction count. Strict `>` (at-threshold survives).
- Queries: `Find`, `IsAlive`, `AliveCount`, `ListAlive` (sorted by numeric_id),
  `LeastLoaded(district)` — the "which instance do I send this player to" pick (lowest
  `player_count`, ties broken by numeric_id).

## Design notes
- **Deterministic caller-supplied clock** (`now_ms`) throughout — no wall-clock reads, so it
  replays identically in tests and on the server (feed the world tick's monotonic ms).
- **Cadence/eviction are tunable struct fields** (`heartbeat_interval_ms`, `eviction_multiple`),
  not baked constants — match them to the real district heartbeat cadence once wired.
- Additive/merge-friendly: new files only + 2 wiring lines in `build_and_run.ps1`.

## Integration points (for the district-server / UE-side agents)
- World relay recv loop: `RelayCodec::DecodeStream(buf)` → for each msg
  `directory.Apply(msg, tick_ms)`. This is exactly the shape `Apply` was built for.
- World tick (once per interval): `directory.PruneStale(tick_ms)`; on each eviction, tear
  down routing to that instance and (optionally) relay the survivors. This is what makes a
  `kill -9`'d district disappear from the join options within ~2 heartbeats.
- District join routing: replace/augment ad-hoc instance selection with
  `directory.LeastLoaded(district_name)` to get the live, least-loaded instance's `port`.

## Ops scripts (Qoder, 2026-07-21) — landed + DryRun-VERIFIED
The M16 process/ops deliverables now exist under `tools\scripts\` and were exercised this pass:
- `bootstrap_server.ps1` — fresh-server bootstrap from a clean `Saved\`. Verified end-to-end:
  fresh provision → `CLEAN` + exit 0 + dirs created; seeded state (incl. an M15
  `characters\<acct>_<slot>_progress.json` sidecar) → detected `CARRIES 2 state file(s)`;
  `-Clean` (no `-Force`) is report-only (files preserved); `-Clean -Force` deletes them
  (progress sidecar included, since the `characters\*.json` glob covers it). Missing required
  catalog → exit 1.
- `start_world.ps1 -DryRun` → exit 0; emits the world CMD (`-WorldServer -Port=17778
  -RelayPort=17800`), 1:1 with `APBPorts.h` (World=17778, Relay=17800).
- `start_district.ps1 -District <id|numeric_id> -DryRun` → resolves from `districts.json`
  (authoritative): Financial→numeric_id 1→port 17811 (= `DistrictBase 17810 + numeric_id`),
  map `Lvl_APB_Financial_Freeroam?listen`; both name and numeric_id lookups agree; an unknown
  district exits 1 with the known-district list. 1:1 with `APBPorts.h::DistrictPort`.

## NOT done here (out of pure-Domain scope)
- **UE relay transport + role wiring** (the FSocket loop feeding `Apply`, the world-tick
  `PruneStale` call, sanction/eviction side effects) — lives in `Systems/Server/`, needs the
  compiled `APBReloadedServer` target (can't be exercised in the pure-`cl` harness).
- **The kill -9 integration gate itself** — an end-to-end process test, distinct from this
  unit-tested logic core.
- Anti-cheat heuristics (movement/fire-rate/damage/sanction) landed separately — see
  `work/m16_anticheat_note.md`. Password hashing lives in `APBCrypto`/`APBTicket`.

## Build/verify
```
powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1
```
`$exe17 = APBDistrictDirectoryTests` compiles
`APBDistrictDirectory.cpp + APBRelayProtocol.cpp + run_directory_tests.cpp`. All 17 suites `FAILS=0`.
