# M7 — District Servers, Travel Handoff & Chat (SPEC / DRAFT)

> **STATUS: SUPERSEDED BY LIVE IMPLEMENTATION — Qoder.exe is executing M7 (2026-07-20).**
> M6 gate is GREEN (`WORLD_SERVER_GATE_OK login=2` — see `m6_world_gate_findings.md`).
> Qoder has landed N5 (Domain `ChatService`) in `Source/APBReloaded/Domain/APBChat.{h,cpp}`
> + `tests/run_chat_tests.cpp`, wired into `tests/build_and_run.ps1`. Its implementation is
> BROADER than §6 below (adds profanity filter, flood control, presence, ignore list, slash
> parsing). This doc is now a REFERENCE for the remaining M7 units (N1-N4, N6, N7: ports,
> district PreLogin ticket redeem, travel dispatch, W↔D relay, chat RPC wiring, M7 gate) —
> NOT the source of truth for chat. Do NOT clobber Qoder's `APBChat.*`. Primary session did
> not touch source; this is planning reference only.

## 0. Scope (from `_active.md` deferrals + D6/D10)

M6 proved: client → **world server** (login / charlist / districtlist / issue-ticket) over UE
NetDriver, single process. M7 makes the ticket **actually travel the player into a district
server** and adds chat. Explicitly in scope (deferred here from M6 R2/R4):

- **W↔D relay**: world↔district TCP/JSON control channel (D6's real deliverable).
- **Cross-district travel**: `ClientTravel` into a district GM, carrying the M6 ticket.
- **Ticket redemption**: district-side verify of the world-issued ticket (replay-safe).
- **Ed25519 ticket signing**: upgrade M6's symmetric HMAC ticket to asymmetric.
- **Chat**: Domain `ChatService`; in-district over UE RPC, cross-district via W↔D relay.

Out of scope (stays later): matchmaking/clans/groups (D10 tail), TLS on relay, mission
district triggers (D14).

## 1. What already exists (verified 2026-07-20, do NOT re-create)

| Piece | State | File |
|---|---|---|
| `TicketService` | **DONE** — `IssueTicket`/`VerifyTicket`/`ConsumeJti`, HMAC `payload.sig`, jti replay cache, 90s expiry | Domain `APBTicket.h` |
| World issue-ticket | **DONE** — `IssueTicketJson` (M6) | `APBServerControl.cpp` |
| `AAPBDistrictGameMode` | **SHELL** — `BeginPlay` sets SessionId, `PostLogin`→`JoinDistrictAsPeer(SessionId,Name)` | `Systems/District/` |
| District freeroam host | **DONE** — streamer, placement loader, freeroam GM/char/HUD | `Systems/District/`, `Systems/Freeroam/` |
| `UAPBServerControl` | world-role only (no district role, no relay) | `Systems/Server/` |

## 2. The gaps M7 closes

1. **District `PostLogin` does NOT verify the ticket** — it blindly `JoinDistrictAsPeer`.
   Anyone connecting to the district port joins. M7: redeem ticket before admit.
2. **No W↔D control channel** — world can't tell a district "expect account N", district
   can't report load back. `UAPBServerControl` has no district role.
3. **No travel dispatch** — nothing turns an issued ticket into a `ClientTravel` to the
   district's address. M6 gate stops at "ticket issued".
4. **No `ChatService`** — 0 hits in Domain.
5. **HMAC not Ed25519** — symmetric key means every district holds the signing secret.

## 3. Port and relay contract (resolved)

`Source/APBReloaded/Systems/APBPorts.h` is the sole allocation authority. `[APBServer]` in
`Config/DefaultGame.ini` mirrors it, and the launch scripts parse the header rather than
holding their own port defaults. District game ports derive from the stable apbdb
`numeric_id` in `Content/Data/districts.json`: `DistrictPort(numeric_id)` is
`17810 + numeric_id` for positive IDs and `0` for invalid input.

| Role | Port | Resolution |
|---|---:|---|
| World game / NetDriver (UDP) | 17778 | `apb::ports::World` |
| World-district relay (TCP) | 17800 | `apb::ports::Relay` |
| District game base (reserved) | 17810 | `apb::ports::DistrictBase` |

| District | numeric_id | Port |
|---|---:|---:|
| Financial | 1 | 17811 |
| FinancialChaos | 2 | 17812 |
| PGAsylum | 4 | 17814 |
| PGBeacon | 5 | 17815 |
| PGCrate | 6 | 17816 |
| Social | 9 | 17819 |
| Waterfront | 11 | 17821 |
| FinancialRiot | 12 | 17822 |

The W-district relay is newline-delimited JSON v1. Every frame requires `version`,
`request_id`, `sent_ms`, and `auth`; frames are limited to 64 KiB; request timestamps must
fall within 2 s; the receive queue holds at most 256 frames; reconnect backoff is exponential
from 250 ms to 5000 ms; two missed 5 s heartbeats evict a district.

## 4. Travel handoff flow (the M7 spine)

```
CLIENT              WORLD (17778)              RELAY (17800)         DISTRICT (DistrictPort(numeric_id))
  | login/charlist/districtlist (M6, done)      |                        |
  | issue_ticket(char,district) ─────►          |                        |
  |                              IssueTicket()   |                        |
  |                              ── ASK_DISTRICT_EXPECT{acct,jti} ──►      | (district pre-authorises jti)
   | ◄── ticket + district addr(DistrictPort(numeric_id)) ─────────         |
   | ClientTravel("ip:DistrictPort(numeric_id)?ticket=<payload.sig>") ───► PreLogin
  |                                              |         VerifyTicket()+ConsumeJti()  ← redeem
  |                                              |         faction/char from claims     |
  | ◄──────────────── admitted to district freeroam (or kicked if bad) ─────────────────
```

- **Redeem in `PreLogin`/`PreLoginAsync`**, NOT `PostLogin` — reject before a pawn spawns.
  Pass token via `?ticket=` URL option (`UGameplayStatics::ParseOption`).
- **`ConsumeJti` is the single source of replay safety** — one ticket = one admit. Second
  connect with same jti → `ErrorMessage` set, connection refused.
- **District pre-authorise (ASK_DISTRICT_EXPECT) is optional-but-preferred**: lets the
  district reject unknown jti even before the client dials, and lets world load-balance.
  MVP path can skip it (district verifies HMAC standalone) since the signing key is shared;
  it becomes mandatory once Ed25519 lands (§ below).
- **Envelope enforcement**: `APBRelayProtocol` rejects malformed, oversized, expired,
  unauthenticated, duplicate, stale-JTI, replay-JTI, and queue-full frames before a relay
  message reaches a Domain service. The limits in §3 apply to both relay directions.

## 5. Ed25519 upgrade (do AFTER travel MVP works with HMAC)

- World holds private key, signs `payload`. Districts hold only the **public** key.
- `TicketService` gains `IssueTicketEd25519` / `VerifyTicketEd25519`; keep HMAC path for
  the domain unit tests. UE side: libsodium is not vendored — decision needed (vendor
  libsodium vs. use `FGenericPlatformMisc` crypto vs. keep HMAC + ASK_DISTRICT_EXPECT for
  authority). **Recommend: keep HMAC + mandatory ASK_DISTRICT_EXPECT for M7**; defer real
  asymmetric to M8 to avoid a new third-party dep mid-milestone.
- Changing the ticket signature mechanism does not change the versioned relay envelope or
  its mandatory fields, 64 KiB cap, 2 s timeout, bounded queue, reconnect schedule, or
  heartbeat eviction contract in §3.

## 6. Chat (Domain `ChatService`)

- New pure-C++ `Domain/APBChat.h` — `ChatService` with channels: `District`, `Whisper`,
  `Group`(stub). In-district delivery over UE RPC (client→district GM→multicast). Cross-
  district (Whisper to player on another district) rides the W↔D relay. Unit-testable in
  isolation like `WorldService` (add to `tests/`).
- Cross-district chat uses the same authenticated JSON-lines envelope and backpressure
  limits as registration and ticket messages; no chat payload bypasses relay validation.

## 7. Task breakdown (proposed hyperplan units, ordered by dependency)

| # | Task | Files | Verify |
|---|---|---|---|
| N1 | Port constants: add `RelayPort=17800`, `DistrictPortBase=17810` to `DefaultGame.ini` + a `APBPorts.h` | `Config/`, new header | build green | **✅ DONE** (see progress note) |
| N2 | District `PreLogin` ticket redeem: parse `?ticket=`, `VerifyTicket`+`ConsumeJti`, reject on fail | `APBDistrictGameMode.cpp` | domain test: replayed jti rejected |
| N3 | World→client travel dispatch: after `IssueTicketJson`, send district addr; client `ClientTravel` | `APBServerControl.cpp`, client probe | gate: client lands in district |
| N4 | `UAPBServerControl` district role + W↔D relay (TCP/JSON-lines v1, `ASK_DISTRICT_EXPECT`/`REPORT_LOAD`) | `Systems/Server/` | 2-process gate plus relay contract tests |
| N5 | Domain `ChatService` + tests | `Domain/APBChat.*`, `tests/` | `tests\build_and_run.ps1` FAILS=0 | **✅ DONE** (see progress note below) |
| N6 | In-district chat RPC wiring | district GM + PC | manual: 2 clients see msgs |
| N7 | **M7 gate**: world proc + district proc; client logs in → travels → redeems → chats → `M7_TRAVEL_GATE_OK` | new `tools/run_m7_gate.ps1` | terminal OK print |

## 8. Acceptance criteria (M7 GREEN)

1. Client completes M6 flow, then **`ClientTravel`s into a district server** (separate
   NetDriver port) and spawns a freeroam pawn.
2. Ticket is **redeemed exactly once** — replay of the same jti is refused at `PreLogin`.
3. A **connect without a valid ticket is rejected** before pawn spawn.
4. Two clients in the same district **exchange chat**; domain `ChatService` tests FAILS=0.
5. Build green (`APBReloadedEditor`), `lsp_diagnostics` clean on changed files.
6. `tools/run_m7_gate.ps1` prints a single terminal `M7_TRAVEL_GATE_OK`.

## 9. Risks / open decisions

- **R-M7-a** Ed25519 vs HMAC+relay-authority — recommend HMAC+mandatory `ASK_DISTRICT_EXPECT`
  for M7, defer libsodium to M8 (avoids new dep mid-milestone). **Needs user ratification.**
- **R-M7-b** District process model: one binary per district (`-District=<id>`) vs. one
  multi-district host. D6 says role-by-CLI → **one binary, `-District=<id>` sets port + map.**
- **R-M7-c** Relay transport: raw `FSocket` TCP/JSON (matches D6 wording) vs. a second
  NetDriver connection. Recommend `FSocket` TCP/JSON to keep world↔district decoupled from
  gameplay replication. **Needs ratification at hyperplan time.**
- **R-M7-d** Does M7 require the district `.APB` map export (D5) first, or can it run on an
  existing freeroam map? → Existing freeroam map is enough to prove travel; `.APB` export
  stays a separate track. **M7 does NOT block on asset export.**

---

*Draft ends. Next step: when M6 gate is GREEN, run this through hyperplan (adversarial
review) → ulw-loop. Until then this is reference only; no source touched by this doc.*

---

## Progress log

### 2026-07-20 — N5 (Domain `ChatService`) DONE — landed by ulw-loop implementer session

M6 gate is GREEN (see `work/m6_world_gate_findings.md`), so M7 work has started with the
one task that touches **zero contended source** (fully additive, merge-friendly): the pure
Domain chat layer. **No server/GM files edited** — N2/N3/N4/N6 remain free for Sisyphus.

**New files (all additive):**
- `Source/APBReloaded/Domain/APBChat.h` — pure C++17, no platform/UE headers.
- `Source/APBReloaded/Domain/APBChat.cpp` — routing impl.
- `tests/run_chat_tests.cpp` — 34 assertions, 8 test groups.
- `tests/build_and_run.ps1` — added 5th suite `APBChatTests` (standalone: `APBChat.cpp`
  has no other Domain deps, so it links alone).

**What `ChatService` does (grounded 1:1 on retail `APBUserInterface.int [Chat]`):**
- Channels: `Local, District, Group, Whisper, Faction, Clan, Trade, System`
  (spec §6 asked District/Whisper/Group wired for M7 — done; the rest share the same
  routing primitives, ready for M11/M14 to enable clan/faction/trade membership).
- **Authoritative router**: `Submit(sender, channel, target, body, now_ms)` returns a
  per-recipient `ChatDelivery` list. UE district GM (N6) just feeds a roster + multicasts
  the deliveries — no gameplay logic in the RPC layer.
- Ignore list (retail: "blocked including mail and chat"), per-recipient profanity mask
  (opt-out), flood control (retail flood-kick → domain mutes for the window; tunable
  `flood_limit`/`flood_window_ms`), presence (Available/Away/DoNotDisturb; DND refuses
  whispers), slash-command parsing (`/w /d /l /g /f /c /trade` + aliases).
- `SystemBroadcast` bypasses filter/flood/ignore (server messages always land).

**Verify:** `powershell -File tests\build_and_run.ps1` → all 5 suites `FAILS=0`, exit 0.

**Handoff for N6 (in-district chat RPC wiring):** on the district GM, hold one
`apb::ChatService`; `PostLogin`→`AddPlayer(name)`, `Logout`→`RemovePlayer(name)`. A client
chat RPC calls `SubmitRaw(sender, rawLine, defaultChannel, nowMs)` then the GM iterates
`result.deliveries` and unicasts each `ChatDelivery` to its recipient's controller. Cross-
district whisper (spec §6) rides the N4 W↔D relay: if `Submit` returns `RecipientOffline`,
forward the whisper over the relay to the district that holds the target.

> Left UNCOMMITTED per shared-worktree policy (Sisyphus has uncommitted edits). Ready for a
> single-concern commit: `feat(M7/N5): Domain ChatService + tests`.

### 2026-07-20 — N1 (port allocation) DONE — unblocks §3 for N2/N3/N4

Resolved the §3 port BLOCKER so the server/relay/district tasks can proceed without a
number clash. **Additive only — no server/GM source edited.**

**New / changed files:**
- `Source/APBReloaded/Systems/APBPorts.h` (NEW) — constexpr single source of truth:
  `apb::ports::World=17778`, `Relay=17800`, `DistrictBase=17810`, and
  `DistrictPort(numeric_id) = DistrictBase + numeric_id` (0 sentinel for invalid ids).
  Pure C++17, no UE/platform headers, so tools + world + district code share one table.
- `Config/DefaultGame.ini [APBServer]` — added `RelayPort=17800`, `DistrictPortBase=17810`
  (ops override; header holds the code defaults).

**Design note:** district ports key off the **stable apbdb `numeric_id`** in
`Content/Data/districts.json`, not a positional index — order-independent and 1:1 with the
live game (Financial=1→17811 … Waterfront=11→17821 … FinancialRiot=12→17822).

**Verify:** compiled standalone with `cl /std:c++17`; 5 `static_assert`s
(World/Relay/DistrictPort(1)=17811/DistrictPort(11)=17821/DistrictPort(0)=0) hold, exit 0.

**Handoff for N3/N4:** world hands the client `ip:apb::ports::DistrictPort(numeric_id)`;
the relay listens on `apb::ports::Relay`. Include `Systems/APBPorts.h` — do NOT hardcode.

> Left UNCOMMITTED per shared-worktree policy. Ready as: `feat(M7/N1): port allocation (APBPorts.h + ini)`.
