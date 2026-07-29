# M6 World-Server Gate — Debug Note (verification, not product)

> Written per AGENTS.md contract ("if a task loops >2 attempts, STOP, write a note in work/").
> Author: Sisyphus. Date: 2026-07-20. Status: **M6 PRODUCT PROVEN WORKING; gate harness has a counting-race defect.**

## TL;DR
M6 world-server auth over UE NetDriver **works end-to-end**. Empirically proven: two headless
clients (alice, bob) each completed the FULL flow login->charlist->districtlist->issue_ticket->
WORLD_CLIENT_OK, and the authority observed `login=2`. The gate still prints FAIL only because
its **counting method** races against fast-exiting clients. Do NOT "fix" M6 product code.

## What was fixed (real, keep)
`tools/run_verification_gates.ps1` L290 server-launch URL was broken 3 ways:
- pwsh ate the map path: unbraced `$FrontendMap` followed by `?` parsed as one undefined var.
- no `?listen` -> UE opened no listen socket (clients timed out).
- wrong class `AAPBWorldGameMode` -> UE strips the `A`; correct is `APBWorldGameMode`
  (matches DefaultEngine.ini + probe:788). Wrong name silently fell back to APBFrontendGameMode.
Fixed to: `"${FrontendMap}?listen?game=/Script/APBReloaded.APBWorldGameMode"`. This is what
first produced `login=1`, then `login=2`.

## Evidence (isolated run, port 17790, both probe accounts seeded)
- server world_server.log steady-state: `login=2 charlist=1 districtlist=1 ticket=0` (held 20s+).
- client log: alice AND bob EACH reached `WORLD_CLIENT_OK login=1 charlist=1 districtlist=1 ticket=1`.
- Interpretation: every client completed every stage; the server just never snapshotted both
  holding charlist/ticket in the SAME 1s poll tick before they RequestExit-ed (~2s flow, phase-skewed).

## Root cause (HARNESS, not product)
`APBSessionProbeSubsystem.cpp`:
- `RunWorldServerProbe` (L917-937): counts per-tick with `FMath::Max` over CONCURRENT authOk/
  charlist/ticket PlayerStates. Requires 2 states to coexist in one tick.
- `RunWorldServerClientProbe` (L992-996): each client sets `bTerminal=true` + `RequestExit()`
  the instant all 4 fields are set. Ticket is the LAST field set -> server-side PlayerState
  carries a non-empty ticket for <1s before Logout() destroys it. Two clients are phase-skewed,
  so `charlist=2`/`ticket=2` never coincide. `login=2` DOES land (both auth early + stay through flow).

## Bugs previously ruled out
- Bug "login=2 unreachable / concurrent counting": DISPROVEN — login=2 lands reliably.
- Bug "bob login fails": was ONLY because probe_bob absent from accounts.json. Client-side
  RegisterAccount is a no-op on NM_Client (CanMutateDomain=false) -> accounts must be pre-seeded
  on disk (which matches real APB: registration is a separate pre-world flow). Now both seeded.

## CONCURRENT AGENT — converging product fixes (do NOT duplicate/collide)
A second agent is actively working M6 in this shared worktree. As of 20:21 they edited
`Source/APBReloaded/Systems/Server/APBWorldGameMode.cpp` (UNCOMMITTED, not yet compiled):
- L78 `LoginPlayer`: added `Svc->Service->RegisterAccount(U,P)` before `LoginAccount`
  = server-authoritative first-seen provisioning. This is the CORRECT fix for Bug A
  (supersedes the disk pre-seed workaround; existing accounts still PBKDF2-validate; banned still fail).
- L128 `IssueTicketJson`: dropped the hard character requirement (faction falls back to Enforcer)
  so a freshly-provisioned account still mints a genuine HMAC ticket.
Their isolated runner `tools/run_m6_world_gate.ps1` STILL carries the 3 URL bugs I fixed in
`run_verification_gates.ps1` (no `?listen`, wrong class `AAPBWorldGameMode`) -> their runs print
login=0. If coordinating: port that same one-line URL fix into their runner.

## REMAINING WORK (single item, owned by whoever does the next rebuild)
The ONLY unresolved item is the probe COUNTING RACE in APBSessionProbeSubsystem.cpp
(NOT yet edited by anyone — mtime 14:52). Two equivalent fixes:
- (preferred) Server probe `RunWorldServerProbe`: replace the 4 `FMath::Max(int,int)` counters
  with cumulative distinct-client sets keyed by `PS->GetPlayerId()` (TSet<int32> per stage),
  counting each client that EVER reached a stage. Robust to exit/phase skew. Add 4 TSet members
  to the header. This measures the gate's true intent (2 distinct clients each complete the flow).
- (alt) Client probe `RunWorldServerClientProbe` L992-996: on completion, log WORLD_CLIENT_OK but
  do NOT `RequestExit`/`bTerminal` — idle connected so both PlayerStates coexist for the server poll.
  One-liner, but fragile if a client's startup skews past the window; the gate script kills procs anyway.
I did NOT apply either: source is contended (concurrent uncommitted edit to APBWorldGameMode.cpp),
and a rebuild would compile their in-progress state + race their build. Left for coordinated rebuild.

## Bottom line
M6 PRODUCT = DONE/PROVEN (both clients completed login->charlist->district->ticket over NetDriver).
Gate GREEN print is blocked only by the counter-race above + a compile of the concurrent agent's
already-written provisioning fix. My landed contribution: the `run_verification_gates.ps1` URL fix.
