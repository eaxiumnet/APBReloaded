# M11 — Domain Matchmaking Brain (handoff)

**Status:** Domain half DONE + tested. All 12 domain suites FAILS=0 (`$exe12 = APBMatchmakingTests`).
**Author:** Qoder. **Scope:** pure-C++17 `apb::Matchmaker` — the authoritative pairing brain
for APB's adversarial cross-faction opposition missions. No UE/platform deps; unit-testable in
isolation like WorldService/ChatService.

Grounded on `_active.md` **D10** ("Matchmaking = threat-tier opposition pairing + group queue in
Domain (APB-authentic), not a separate process") and **D14** ("opposition dispatch via Domain
matchmaking (threat-tier pairing)").

## Files
- `Source/APBReloaded/Domain/APBMatchmaking.h` — API (`MatchTicket`, `MatchPairing`, `Matchmaker`).
- `Source/APBReloaded/Domain/APBMatchmaking.cpp` — implementation.
- `tests/run_matchmaking_tests.cpp` — 8 test groups; wired as `$exe12` in `tests/build_and_run.ps1`.

## Model
A **party** (solo player = party of 1, or a group = one atomic ticket) enqueues a `MatchTicket`:
```
party_id     unique key (group id, or solo player name)
faction      Enforcer | Criminal
threat_tier  0..N (higher = more skilled)
party_size   members (a group queues + matches as a UNIT — never split)
enqueued_ms  caller-supplied clock at enqueue; drives widening tolerance
```
`FormMatches(now_ms)` returns `MatchPairing`s (one Enforcer party vs one Criminal party; vectors
leave room for future N-vs-N). `tier` = tougher of the two sides; `tolerance_used` = tier gap the
search had to accept (0 = exact-tier).

## Algorithm (deterministic, oldest-first, fair)
1. Snapshot queue, `stable_sort` ascending by `enqueued_ms` (oldest served first).
2. Greedy: for each unmatched party `a`, scan younger unmatched `b`:
   - **opposing faction only** (Enforcer↔Criminal; same-faction never pairs, at any slack).
   - effective tolerance `eff = max(tolA, tolB)` — the party that waited LONGER grants the wider
     search (fairness).
   - legal iff `|tierA - tierB| <= eff`; among legal candidates prefer the **smallest tier gap**,
     tie-broken to the **oldest** candidate.
3. Both parties leave the queue; survivors keep oldest-first order.

**Tolerance policy** (`ToleranceForWait`): starts at 0 (exact tier), +1 per `widen_interval_ms`
(default 15000ms), capped at `max_tolerance` (default 4). Clock skew (now < enqueued) → 0.
`widen_interval_ms <= 0` → always max slack (degenerate config guard).

## Determinism / integration notes
- **Caller supplies `now_ms`** — no wall-clock dependency, so district-side callers control the
  tick and results are reproducible in tests.
- Queue is **transient** (no persistence) — matches retail; matchmaking state dies with the process.
- Re-`Enqueue` of an existing `party_id` **replaces** the ticket and **resets** the wait clock.
- Integrates with `GroupService` (APBGroup): a group's id + member count feed one ticket
  (`party_id` = group id, `party_size` = member count). Threat tier comes from `APBThreat`.

## NOT done here (UE district-side N-work, open for another agent)
- Opposition **dispatch/geometry**: spawning the paired opposition party into the district,
  mission **stage triggers** on interactables/zones (D14), mission HUD.
- The tick loop that calls `FormMatches(now)` on the district/world server + routing the
  resulting pairing to both clients.
- INT-table mission template import (`MissionTemplates.INT` / `TaskObjectives.INT` → `Content\Data\`).

## Verify
`powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1`
→ all 12 suites `FAILS=0` (matchmaking suite header: `=== APB Matchmaking Tests (M11 threat-tier opposition) ===`).

Ready for: `feat(M11): Domain threat-tier opposition matchmaker + tests`.
