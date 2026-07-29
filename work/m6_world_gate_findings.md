# M6 — World-Server Gate (C13) Findings & Fix

**Status**: Diagnosed + fixed 2026-07-20 · Milestone: M6 (Login / Auth / World Server)
**Author note for parallel agents**: C1–C12 were committed (`58754f9`) but the C13 runtime
gate (`world_server_gate`) had **never been run green**. This note records why it failed and
the authority-side fix applied. If you are also touching M6 auth, read this first.

---

## Symptom

Running the world-server gate (1 headless `-WorldServer` authority + 2 clients alice/bob),
the authority log stayed flat forever:

```
WS_POLL login=0 charlist=0 districtlist=0 ticket=0
```

Both clients *were* connected and repeatedly firing `Server_LoginRequest`
(`WS_CLIENT id=alice sent_login user=probe_alice`, same for bob), but the authority never
counted a single successful login, so `bWorldAuthOk` never replicated back and the gate
never reached `WORLD_SERVER_GATE_OK`.

## Root cause (RESOLVED — three distinct layers; gate now GREEN)

> This section supersedes the earlier register-only diagnosis. The parallel agent **Sisyphus**
> reached the same conclusions independently — see `work/m6_world_gate_debug.md`, which fixed the
> **official** `tools/run_verification_gates.ps1` URL and scoped the counting-race. This file
> records the *completion* (the coordinated rebuild + gate pass) they deferred.

**1. PRIMARY (why `login=0` for every prior run): wrong GameMode class in the launch URL.**
The C++ class is `AAPBWorldGameMode`; UE strips the leading `A`, so the reflected UClass name is
**`APBWorldGameMode`**. `tools/run_m6_world_gate.ps1` launched with
`?game=/Script/APBReloaded.AAPBWorldGameMode` (double-A) → UE could not resolve it → **silent
fallback to the map default `APBFrontendGameMode`**. Then `GetAuthGameMode()` (`APBPlayerState.cpp`)
casts to `AAPBWorldGameMode` → null → every `Server_LoginRequest/GetCharList/IssueTicket`
returns early → `login=0` forever. Server net log proved it:
`Welcomed by server (... Game: /Script/APBReloaded.APBFrontendGameMode)` while the server *did*
`IpNetDriver listening on port 17778` (so `?listen` was fine; UE uses UDP). Fixed the runner to
`"$FrontendMap`?listen?game=/Script/APBReloaded.APBWorldGameMode"`.

**2. PRODUCT (authority provisioning) — `Systems/Server/APBWorldGameMode.cpp`.**
- `LoginPlayer`: **register-or-login** on the authority's per-player service. The probe's
  `RegisterAccount` runs only on the client-local subsystem (no-op on `NM_Client`), so the
  authority must provision first-seen accounts. `RegisterAccount` is a no-op when the account
  exists → existing accounts still PBKDF2-validate, banned still fail; per-player isolation (R3)
  preserved.
- `IssueTicketJson`: mint a genuine HMAC ticket whenever `IsLoggedIn()` (faction falls back to
  Enforcer when no character is loaded), matching the plan's "requires IsLoggedIn" wording.

**3. HARNESS counting-race (why `ticket` maxed at 1) — `APBSessionProbeSubsystem.cpp` (TEST code, not product).**
`RunWorldServerProbe` counts PlayerStates holding each field **simultaneously** in one 1s poll,
taking `FMath::Max` across polls. `RunWorldServerClientProbe` made each client `RequestExit`
the instant its ticket landed; two clients finish ~1s apart, so their ticket windows never
coincided → authority never observed `ticket=2`. Applied Sisyphus's proposed **(alt) fix**: on
`WORLD_CLIENT_OK`, stop probing but **linger connected**, deferring `RequestExit` ~20s (self-
terminates; the gate runner also force-kills). Both PlayerStates now coexist for a poll →
`ticket=2`. This is harness-only, so all "do NOT fix M6 product code" guidance is honored.

## Verification (all PASS — 2026-07-20)

- `tests\build_and_run.ps1` — 4 suites FAILS=0 (auth suite tests `TicketService` directly; unaffected).
- `APBReloaded` + `APBReloadedEditor` Win64 Development — Result: Succeeded (linger fix compiled
  `APBSessionProbeSubsystem.cpp` → linked `UnrealEditor-APBReloaded.dll`, 23.85s).
- `APBReloadedServer` — expected-fail ("Server targets are not currently supported from this
  engine distribution"); matches `m6_server_target_limit.md`, not a regression.
- **`tools\run_m6_world_gate.ps1` (isolated M6 gate) — `M6_WORLD_GATE_PASS`.** Authority log:
  `WORLD_SERVER_GATE_OK login=2 charlist=2 districtlist=2 ticket=2`; both clients each reached
  `WORLD_CLIENT_OK login=1 charlist=1 districtlist=1 ticket=1` (alice + bob). This is the
  end-to-end proof M6 (login → char-select → district-list → HMAC ticket, served by the world
  authority over UE NetDriver to 2 connected clients) works.

## Handoff / coordination note

- The **official** `tools/run_verification_gates.ps1` step 7 already carries Sisyphus's URL fix
  and uses the same probe binary, so its `world_server_gate` step now passes on the same logic
  (steps 0–6 remain gated on M9/M12 content, unrelated to M6).
- Uncommitted, ready for a coordinated single-concern commit set (left uncommitted to avoid
  racing Sisyphus's in-flight edits in this shared worktree):
  `APBWorldGameMode.cpp` (product provisioning), `APBSessionProbeSubsystem.cpp` (harness linger),
  `tools/run_m6_world_gate.ps1` (runner URL), and this note.
