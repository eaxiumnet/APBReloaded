# M16 Zero-Trust Server Hardening

**Created:** 2026-07-28
**Extends:** milestone M16 (Dedicated server hardening + anti-cheat posture) IN PLACE
**Status:** planned, not started
**Architecture authority:** two Oracle consultations, 2026-07-28
(`ses_0580ef405ffeQTyebRJ4BERuwW` identity/authority, `ses_0580d7d0dffe0bByF4O0YVxZYh` crypto/persistence/replay)
**Evidence dossier:** 6-agent audit, findings inline below with file:line

## Directive

User, verbatim: *"we need to make it all secure and only server sided, we cannot ever trust the
client, we are expecting the client to be hacked 24/7 and could be malicous. only server is the
truth."* Then, when offered a cheap-vs-correct tradeoff: *"Proper security at all time."*

No dev-only escape hatches. No scope reduction. No partial delivery.

## Threat model (settled — do not widen or narrow)

**In scope:**
- Malicious / hacked game client, continuously.
- Offline save-file editing.
- Accidental file corruption.
- Malware or another local service that can *write* the persistence directory but cannot *read*
  process secrets.

**Out of scope — never claim protection against these:**
- Malicious server operator.
- Full host compromise or RCE as the server account.
- Process memory dump.
- Rollback of an *old, validly tagged* file.
- Deletion of a file.

Consequence: the save MAC (Wave 5) is defense-in-depth against offline edits and corruption. It is
**not** operator protection. If deployment secrets are not stored separately from persistence
directory permissions, it degrades to corruption detection only — document it that way, do not
oversell it.

## The binding invariant

```
owning UE connection
  -> owned Server RPC
  -> server FPlayerSession
  -> authenticated account + SERVER-selected character
  -> command-specific authorization
  -> Domain mutation + invariant validation
  -> persistence
  -> replicated result / snapshot
```

Client **may** supply: target, item, district, requested operation.
Client **must never** supply: acting account, acting character, session owner, **clock**, reward,
damage, progress amount, authoritative transform.

## Defect inventory

Severity: **P0** = remotely exploitable today. **P1** = authority inversion or crypto weakness.
**P2** = confused-deputy / hardening. Every row cites the audited location; re-grep by symbol
before editing, the worktree is shared and line numbers drift.

| ID | Sev | Location | Defect |
|---|---|---|---|
| C1 | P0 | `Config\DefaultGame.ini` `[APBServer]` | Committed placeholder `TicketSecret` (64 hex) — anyone can forge tickets |
| C2 | P0 | `APBDistrictGameMode.cpp` `RequiresDistrictTicket()` | Defaults `bRequireTicket=false`; `RequireTicket` absent from every `.ini` — ticketless admission is live; gates only pass via `-RequireTicket` |
| N6 | P0 | `APBTicket.cpp` `json_int` | `std::stoll` with no exception containment — malformed ticket **terminates the process**, remote DoS |
| N7 | P1 | `APBTicket.cpp` payload write | Ticket payload strings written without JSON escaping |
| N8 | P1 | `APBTicket.cpp` `b64url_decode` | Silently ignores invalid characters |
| N9 | P1 | `APBCrypto.h` `hex_decode` | Accepts malformed and odd-length input |
| N10 | P0 | `APBTicket.cpp` `TicketService()` ctor | Implicit random secret — world and district cannot verify each other |
| N13 | P1 | `APBTicket.cpp` `Global()` + `SetSecret` | Mutable singleton + hidden wall clock: test hooks on the production path |
| C3 | P0 | ~30 entry points (see below) | Identity spoofing: acting character taken from a client-supplied name |
| N1 | P0 | `APBSocial.h` `MailService::SendMail` | **Mints currency** — no sender debit |
| N2 | P0 | `APBWorldGameMode.cpp` `LoginPlayer` | Auto-registers every first-seen username |
| N3 | P1 | `APBWorldGameMode.cpp` `MarkRelayPlayerJoined` | Marks admitted **before** reservation / JTI binding |
| N4 | P0 | `APBGameInstanceSubsystem` mail surface | Six spoofable mail entry points, not four |
| N5 | P1 | `APBFreeroamCharacter.cpp` `ServerEnterNearestVehicle` | Possession from 5000 units |
| C4 | P1 | `APBGameInstanceSubsystem.cpp` `CanMutateDomain()` | Fails **open** on null world |
| C5 | P1 | `APBGameInstanceSubsystem.cpp` | In-process client-side `RegisterAccount` / `Login` |
| N11 | **P0** | `APBWorldGameMode.cpp` `PlayerServices` | Every per-connection `WorldService` initialises the same persistence root; **different accounts** independently read-modify-write global `accounts.json`, `auction.json`, `mail.json`. `SocialAuthority` and the temporary return-path `WorldService` are additional writers. One-session-per-account narrows this but does **not** make global writes linearizable |
| N12 | **P0** | world NetDriver | Passwords traverse an unauthenticated plaintext transport. **IN SCOPE** — see the correction below |
| N14 | **P0** | `APBRelayProtocol.h` / `.cpp`, `APBServerControl.cpp` | Relay puts the shared bearer secret verbatim in every frame as `auth` and verifies by string equality; after `Register`, frame `numeric_id` is never bound to `Client.NumericId`, so any holder of the secret forges joins/returns/chat as **any district**. Listener binds all interfaces with unbounded client accepts |
| N15 | **P0** | `APBPlayerState.cpp` `GetLifetimeReplicatedProps` | **Confidentiality:** unconditional `DOREPLIFETIME` on `Cash`, `G1C`, progression, mission internals, `CharListJson`, `IssuedTicketJson`. `APlayerState` is relevant to all clients, so a hacked client reads every other player's private economy and auth state |
| N16 | P1 | T04A design | Boot epoch has **no cross-process distribution protocol**: if the district compares against its own epoch every world ticket fails; if it accepts any signed epoch, pre-restart tickets survive a world restart |
| D6 | P0 | `APBWorldService` `TickMission(NowSec)` | Client supplies the mission clock — authority inversion |
| D7 | P1 | 7 persistence targets | No integrity tag; offline save editing undetectable |
| D8 | P1 | `APBPersistence.cpp` loaders | Permissive: skip malformed records, return true if ≥1 survived |
| D9 | P1 | `APBSocial.h` `Login` | Plaintext password branch still present |
| D12 | P1 | `APBPersistence.cpp` `Sanitize()` | Maps every non-`[A-Za-z0-9_-]` char to `_` — `foo/bar`, `foo.bar`, `foo bar`, `foo_bar` all collide |
| D13 | P1 | `APBFrontendWidget.cpp` | `OnRegisterClicked` bypasses the server; `NativeConstruct` seeds a QA account unconditionally |

**C3 spoofable surface (~30):** `IssueTicketJson` / `Server_IssueTicket` (will `CreateCharacter`
from a client-supplied name), MAIL 6 (`SocialMailSend`, `MarkRead`, `ClaimAttachments`, `Delete`,
`GetInbox`, `UnreadCount` — mailbox resolved by name-scan through `ServiceForCharacter`), CLAN 11,
FRIENDS 6, GROUP 9, `JoinDistrictAsPeer` (arbitrary `PlayerName` + `SessionId`).

**RPC gaps:** `ServerFireWeapon` and `ServerEnterNearestVehicle` are `Server, Reliable` with **no
`_Validate` at all**; 4 unconditional-`true` and 4 length-only `_Validate` bodies. Epic 5.8
confirms `_Validate` failure **disconnects**, `_Validate` cannot establish identity, and UE ships
**no** RPC rate limiter.

**Numeric sinks:** `AuctionListItem(Qty,Price)`, `TickMission(NowSec)`, `AdvanceOpposition(Amount)`,
`FireCatalogWeapon(AimX,AimY)`, `AddSymbolLayer`, `EquipClothingColored`, `ApplyBodyProfile`,
`SocialClanAddRank(Permissions)`, `SocialClanSetMemberRank(RankIndex)`, `SocialMailSend(Cash)`.

### Corrections to earlier diagnosis (Oracle-ruled — do not re-litigate)

| My claim | Correction |
|---|---|
| `BlueprintCallable` social mutators are remotely callable | **False.** A remote client cannot execute a `UGameInstanceSubsystem` function in the server process. They are latent confused-deputy defects (P2), not P0. `Server_IssueTicket` **is** genuinely remotely exploitable. Do not overstate severity in commit messages. |
| Predictable salts invalidate stored hashes | **False.** Salts require uniqueness, not secrecy. **No emergency forced re-salt.** Upgrade opportunistically on next successful login. |
| No-arg `_Validate` returning `true` is a security gap | **Not a gap.** `_Validate` is for wire-malformed rejection only. |
| `LoadHandoffSnapshot` needs hardening | It has **no caller** — dead surface. Prefer deleting it. |

### Correction to this plan's own first draft (review round 1, 2026-07-28)

Oracle **BLOCKED** the first draft with 9 blockers (`ses_057e2d8f2ffeBr3PYDvzg322OW`). The
substantive corrections, all now folded in:

| First-draft claim | Correction |
|---|---|
| **N12 transport encryption is out of scope** — "UE5 ships no TLS, a custom NetDriver needs a source engine, the binary engine cannot build the server target" | **The premise was false and unverified.** UE 5.8 ships `AESGCMHandlerComponent` as a runtime plugin (`D:\UE58\UE_5.8\Engine\Plugins\Runtime\PacketHandlers\AESGCMHandlerComponent`), `BaseEngine.ini:3575` exposes `EncryptionComponent=AESGCMHandlerComponent`, and the binary engine's networking path already carries encryption-token delegates plus `UNetConnection` encryption support. **No engine edit, no custom NetDriver, no `TargetType.Server`.** N12 is now **P0 and in scope** (T00C). The out-of-scope entry was rationalising an unverified defect. |
| Confidentiality was not considered at all | N15 added. All 24 original defects were integrity/authority; a hacked client reading other players' `Cash`/`G1C`/`IssuedTicketJson` is squarely inside the stated threat model. |
| Relay was only noted as "a natural rate-bucket home" | N14 added. Bearer secret in every frame + unbound `numeric_id` = district identity forgery. `APBServerControl.cpp` appeared in no task. |
| N11 solved by one-session-per-account | Insufficient — narrows the race only. Different accounts still race on global documents. Now requires a process-owned persistence authority (T07B). |
| Commit 1 removes `TicketSecret`, commit 5 injects it | Only the *world* consumer was covered. District, handoff, chat, and relay all read `GConfig` and would stay broken for four commits, pressuring implementers toward the forbidden `RequireTicket=False` workaround. Secret provider now lands in **commit 1**, across **every** consumer. |
| KDF at 600k in commit 4, bounded executor in commit 15 | Commits 4–14 would hand attackers a *cheaper* game-thread DoS than exists today. Merged into one commit. |
| Async auth (T06C) and one-session (T07B) as separate commits | Creates a session-fixation / use-after-disconnect window: a stale PBKDF callback can land after disconnect, reconnect, or a second login. Merged. |
| T03A -> T03B and T04A -> T04B are "forced" | Both overstated. T03A-before-T03B is a design preference, not a logical necessity. T04A and T04B share **no** file; T04B is independent. Downgraded to notes. |
| Six scenarios | S-W3-2, S-W5-1, S-W5-3, S-W6-3, S-W7-3, S-W2-4 would each pass against a broken or no-op implementation. All strengthened with positive controls — see the revised contract. |

### Self-found defects in the first draft (round 1, found while applying the Oracle corrections)

Oracle did not raise these. They surfaced from checking the draft against the actual harness and the
scoped `tests\AGENTS.md`, and every one would have blocked an implementer mid-task.

| Defect | Correction |
|---|---|
| The contract named three suites that **do not exist**: `run_world_tests.cpp`, `run_social_tests.cpp`, `run_mission_tests.cpp` | Remapped to the real owners — `run_domain_tests.cpp` (WorldService, mission; mission tests already live at `run_domain_tests.cpp:248-311`) and `run_clan_tests.cpp`. An implementer hitting a missing file would have created a fourth suite or, worse, written a test that asserts nothing. |
| Four rows named a **domain** suite for behavior the harness structurally cannot reach — encryption handshake, owner-only replication, secret-provider halt, plaintext-login refusal | `tests\AGENTS.md` states the harness "does not cover replication, GameModes, UMG, or runtime networking" and names "treating a standalone Domain pass as proof that replication or UI behavior works" as an anti-pattern. Added an explicit harness-boundary policy with three scenario natures (pure Domain / mixed / pure UE) and reassigned every offending row. Pure-UE scenarios now take their RED->GREEN from a gate assertion, not an invented domain test. |
| `$exe20` was described as owning the save-MAC, loader, and migration suites | The contract assigns those to `run_persistence_tests.cpp` (S-W5-1/2/3, S-W6-1), where T05B and T06A already write them. `$exe20` now lists exactly its nine pure-Domain suites. |
| `$exe20` had no creating task — T01 referenced it, T08 claimed to author it | T01 creates `tests\run_zerotrust_tests.cpp` and registers `$exe20`, because that is where the first CSPRNG test must live. T08 only closes out the remaining suites. |
| T06A and T06B still listed withdrawn **T06C** as a parallel dependency | Both corrected. T06C's heading is retained for traceability only. |
| T08 was instructed to record "the N12 known limitation" in `work\_active.md` | N12 is fixed by T00C. Recording it as a limitation would have written a false statement into the master roadmap. Changed to record N12 as resolved. |
| S-W4-2 (ticket replay refused) — the single scenario that proves the replay defence — had **no** TODO in any task | Added to T04A as `TestReplayRefused`. |

### Correction to the first revision (review round 2, 2026-07-28)

Oracle **BLOCKED** the first revision (`ses_057a7b7c5ffeVlPPgmN9FsCXMk`). Blockers 1-6 and 8 were
confirmed resolved; four findings remained. All now folded in:

| First-revision state | Correction |
|---|---|
| **Blocker 9's ordering half was never actually applied.** The prose at "Downgraded to preference" said T03A->T03B and T04A->T04B were preferences, but the wave graph still drew `T03A -> T03B`, T03B's `Dependencies:` still said `T03A`, T04B's still said `T04A`, and the Wave 3 heading still read `T03A -> T03B` | All four sites corrected. Wave 3 is `T03A ∥ T03B ∥ T03C` with the single binding edge being the commit order T03C-before-T03B. T04B's dependency is `none`. The prose now names the exact sites so the correction is checkable rather than asserted. |
| **Blocker 7's district-restart half had no security outcome and no executable scenario** — only "verify by world-restart and district-restart gates both correct". Nothing said whether a consumed-but-still-valid ticket must fail after a district restart, or what retains the consumption fact once the district's in-memory `ReplayWindow` dies with the process | Ruled: **each district instance also gets a 128-bit boot epoch**, published to the world over the same authenticated relay registration path, stamped into every ticket as `target_district_epoch`, and refused on mismatch with reason `epoch_mismatch`. After a district restart every pre-restart ticket for it is invalid **whether or not it was consumed**, so the lost replay window is unexploitable. Still no disk persistence (decision 11 holds). New scenario **S-W4-5** covers both the consumed and the unconsumed pre-restart ticket, plus a fresh-ticket positive control. Three TODOs added to T04A. |
| **The pure-UE gate was scheduled in T08/commit 20 — after every task whose RED evidence it carries.** Commits 1, 3, 6, 10, 11, 18 and 19 were therefore structurally unable to capture the mandated "fails before the change" state, turning the gate into retrospective validation | New task **T00E** creates the assertion-registry harness as **commit 0**, before T00A. Each owning task now adds its own assertion, observes RED, then implements. T08 no longer creates the gate — it appends the M6/M7 runs and verifies the registry is complete. An explicit ownership table maps all ten assertions to their owning task and scenario. |
| **Four pure-UE TODOs still named nonexistent domain tests** — `TestEncryptionTokenHandshake`, `TestLoginRefusedWithoutEncryption`, `TestOwnerOnlyReplication`, `TestServerRegisterRequest` — exactly the escape route the round-1 harness-boundary policy was written to close | All replaced with named gate assertions (`ENCRYPTION_ACTIVE`, `AUTH_REFUSED_PLAINTEXT`, `OWNER_ONLY_REPLICATION_OK`, `SERVER_REGISTER_ROUTED`), each with a paired RED-first TODO carrying an expected failure reason. The mixed rows (S-W2-8, S-W3-3, S-W3-4, S-W7-2, S-W7-3) gained their missing UE-half gate TODOs too. |

### Correction to the second revision (review round 3, 2026-07-28)

Oracle **approved both design rulings** — per-district-instance epoch is "the correct no-persistence
solution", T00E's no-RED infrastructure carve-out is "legitimate because it contains no behavioral
assertion", and the ordering corrections plus historical residue are acceptable. Three mechanical
blockers remained. All fixed:

| Second-revision state | Correction |
|---|---|
| **S-W4-5's real surface never actually consumed a ticket before the restart.** It issued one ticket, restarted the district, then presented it — so the "consumed" case was never exercised, and the scenario could pass without proving anything about the lost consumed-jti set | Surface rewritten: ticket **T** is issued **and admitted so it is genuinely consumed**; ticket **U** is issued and held unpresented; the district restarts; **both** are refused; then a fresh ticket admits. The binary observable now spells out why both halves matter — refusing only T means the district still trusts its lost window, refusing only U means consumption is doing the epoch's job. |
| **S-W4-5 had no RED->GREEN for its UE half, and `TestDistrictEpochDistribution` was structurally impossible** as a standalone Domain test — the harness cannot prove that `APBServerControl` publishes an epoch or that `APBDistrictGameMode` compares it against the *running* district's epoch | S-W4-5 reclassified as **mixed**. New gate assertion `DISTRICT_EPOCH_RESTART_REFUSED` owned by T04A; T04A now depends on T00E. The impossible test is gone, and distribution + enforcement are one TODO because neither is provable alone. |
| **The gate TODOs could not produce a per-assertion RED transcript.** The registry reports only `FAIL reason=<first-failure>`, yet T03B added two assertions in one step (masking the second RED, and preventing the first from ever turning the gate green while the second was RED). T00C listed plugin and config edits *before* its RED assertions, and T07A claimed `SERVER_REGISTER_ROUTED` went GREEN on merely declaring the RPC, before `OnRegisterClicked` was routed to it | `-Only <ASSERTION_NAME>` is now a **mandatory** harness capability with the reason stated, and a binding rule added to T00E: add one assertion, run it alone, capture RED, make the **complete** production change, re-run for GREEN. T03B's pair is split into two RED->GREEN cycles. T00C's plugin + config + key-establishment edits are one step, because a plugin and a config alone do not establish an encrypted session. T07A's RPC declaration and caller routing are one step, because a declared-but-unrouted RPC leaves the assertion RED. All **eleven** assertions now use the `-Only` form on both their RED and GREEN steps. |

**Round-3 count changes:** gate assertions 10 became **11** (`DISTRICT_EPOCH_RESTART_REFUSED`).
Scenarios stay at 46; commits stay at 21; S-W4-5 changed nature from pure-Domain to mixed.

**Round-2 count changes (counted, not estimated):** 20 commits became **21** (T00E is commit 0; 1-20
keep their numbers). Scenarios 45 became **46** (S-W4-5). Task headings became **22** — 21 live plus
the withdrawn T06C heading retained for traceability.

A stray count claim was also corrected here: an earlier working note put the scenario total at 48. The
counted value is 46 unique `S-W*` ids. The inflated number came from counting table rows that mention
a scenario id in prose (for example the round-1 defect row about S-W4-2) as if they were contract
rows. Contract rows are the ones in the Wave 0-8 scenario tables only.

### Explicitly not built (Oracle pragmatic cuts)

RFC 8785 JCS canonicalization. Hand-vendored Argon2. Disk-persisted replay caches.

## Settled architecture decisions

Thirteen decisions, both Oracle consultations. Implementers apply these; they are not open
questions.

1. **Identity (Q1).** Four typed aggregate RPCs — `Server_SubmitClanCommand`,
   `Server_SubmitFriendCommand`, `Server_SubmitGroupCommand`, `Server_SubmitMailCommand` — plus
   `RequireAuthenticatedPlayer(APlayerController*) -> FAuthenticatedPlayer` and
   `Server_SelectCharacter(Name)`. Delete public `ServiceForCharacter`. Strip `BlueprintCallable`
   from every authoritative mutator.
2. **Fail-closed (Q2).** `CanMutateDomain` = `World && NetMode != NM_Client && World->GetAuthGameMode()`.
3. **Session key (Q3).** `PCKey` string -> `uint64 PlayerSessionId` in `FPlayerSession`.
   `TravelReservations.OwnerKey` -> `OwnerSessionId`. `AdmittedRoster` stays character-keyed.
4. **Validate semantics (Q4).** `_Validate` = wire-malformed **only** (failure disconnects).
   `ServerFireWeapon` becomes **Unreliable**.
5. **Rate limiting (Q5).** `apb::RequestLimiter` on a caller-supplied **monotonic** millisecond
   clock (`FPlatformTime::Seconds`, never UTC). Instances live on `AAPBPlayerState` and
   `FRelayClient`.
6. **Numeric bounds (D6).** Split across UE `_Validate` / dispatcher / Domain. Strip the clock from
   client `TickMission`. `AdvanceOpposition` becomes server-derived.
7. **Save MAC (D7).** Envelope `{format, version, key_id, data, mac}`. HMAC over the exact `data`
   bytes. Domain separation `"APB-SAVE\0" "v1\0" <logical_doc_name> "\0" <data>`.
   Deployment-injected key, ≥32 bytes: UE reads env var or secret file and passes it into Domain.
   Per-purpose derived keys. `key_id` keyring. Signed provisioning marker.
8. **Loader strictness (D8).** Typed result `{Missing, Valid, IntegrityFailure, SchemaFailure,
   IoFailure}`. Parse to temp -> validate whole document -> atomic commit. **Any** bad record
   rejects the whole document. Prefer deleting `LoadHandoffSnapshot`.
9. **KDF (D9).** Versioned `{scheme, iterations, dk_bytes, salt, hash}`, 600,000 iterations,
   result `{AuthenticatedCurrent, AuthenticatedNeedsRehash, Rejected}`. PBKDF2 runs **off the game
   thread** on a bounded pool. Delete the plaintext branch. Offline migration command. No
   hand-vendored Argon2.
10. **CSPRNG (D10).** New `Domain\APBCrypto.cpp` with `SecureRandomBytes` / `SecureRandomHex` over
    `BCryptGenRandom(nullptr, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` behind `#ifdef _WIN32`.
    `bcrypt.lib` added to **both** `build_and_run.ps1` and `Build.cs`. **Never** fall back to a
    PRNG; entropy failure fails closed.
11. **Replay (D11).** No disk persistence. A 128-bit **boot epoch** per authority process baked
    into tickets. `apb::ReplayWindow` with a hard capacity that **rejects when full**. Collapse
    `VerifyTicket` + `ConsumeJti` into `VerifyAndConsume` — the current split races. UTC unix
    seconds for cross-process comparison; security time = startup UTC + monotonic elapsed.
12. **Filenames (D12).** Injective encoding is impossible under MAX_PATH + Unicode case folding,
    so accounts get a random stable 128-bit id, `a_<32 lowercase hex>`. Username becomes indexed
    data only. Migration is explicit and operator-driven: **HALT on collision**, idempotent
    journal, **no** runtime legacy fallback.
13. **Loopback shim (D13).** Make the existing `bWorldServerMode` branch mandatory, add
    `Server_RegisterRequest`, fix `OnRegisterClicked` and the `NativeConstruct` QA seed.

## Execution constraints (binding on every delegated task)

- **clangd is NOT installed.** C++ `lsp_diagnostics` is unusable. The **build exit code is the type
  gate**. Do not claim a diagnostics pass.
- **Shared worktree, ~3379 dirty paths, concurrent agents.** Implementers MUST NOT run `git add`,
  `commit`, `stash`, `checkout`, or `reset`. Only the orchestrator commits. Never revert a change
  you did not make — unexpected edits are someone else's in-progress work.
- **Never edit `D:\UE58\UE_5.8`.**
- Domain is standard **C++17 only**: `namespace apb`, no UE headers, caller-supplied clocks.
- The installed binary engine **cannot** build `TargetType.Server` (`work\m6_server_target_limit.md`).
  Run the Game target with `-WorldServer -nullrhi -nosound -unattended`.
- Never cache line numbers; re-grep by symbol.
- Error shape is `{"error":"<token>"}`. Existing tokens: `no_live_node`, `no_ticket`,
  `character_unavailable`, `unknown_district`, `over_capacity`, `relay_unavailable`.
- Gate spine order: bind, domain, modelreg, client_loop, mp_observe, playable, `frontend_menu`,
  [frontend_flow opt], world_server, m7_travel, m7_directory, m16_persistence -> `GATE_PASS`.
  `gate_summary.json` keys are lower_snake_case.
- Test harness: 19 suites `$exe..$exe19`, flags `/nologo /EHsc /std:c++17 /O2`, pattern
  `static int fails` + `CHECK` macro + `printf FAILS=%d` + `return fails?1:0`. Harness links **zero**
  libs today; `Build.cs` has no `PublicSystemLibraries`; `bUseUnity=false`. `_WIN32` precedent
  already exists in Domain (`APBPersistence.cpp` `gmtime_s`/`gmtime_r`).
- MSVC env: `vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64
  -property installationPath` then `Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64`.
- Every edit containing comments trips the comment hook. Each occurrence needs an explicit priority
  justification (1 = pre-existing, 2 = BDD, 3 = necessary, 4 = remove).

### Known pre-existing conditions (not caused here, do not "fix")

- `APBCatalog.cpp(1): error: Expected APBCatalog.h to be first header included.` — clean in git,
  untouched, build still exits 0.
- `APBPlayerState.cpp` `AppendPeerObserve` hardcodes a leaked temp path
  (`C:/Users/Support/AppData/Local/Temp/grok-goal-9ca60165ac93/implementer/mp_client_observe.log`)
  and appends on every peer observe. **Not mine — flagged, not deleted.** Removing it is a separate
  authorized task.
- M14 S10 item-grant stays **RED by explicit decision**: item mail is refused and preserved via
  `UnsupportedAttachment`, never granted. Report RED; never green it.

## Wave graph

Waves are strictly ordered. Tasks inside a wave marked `∥` run in parallel; tasks marked `->` are
serialized because they touch the same file.

Revised after review round 1. Wave 0 grew because three blockers proved that secret provisioning,
transport encryption, and relay authentication cannot follow the work that depends on them.

```
Wave 0  T00E                         FIRST: gate harness scaffold (no assertions yet)
        T00A ∥ T00B ∥ T00C ∥ T00D   live P0s + secret provider, ticket codec DoS,
                                     transport encryption, relay authentication
Wave 1  T01                          CSPRNG foundation (blocks all crypto below)
Wave 2  T02A ∥ T02B ∥ T02C           auth pipeline (KDF+executor+limiter+uniqueness),
                                     ticket de-singleton, fail-closed
Wave 3  T03A ∥ T03B ∥ T03C           session key, identity RPCs, PlayerState confidentiality
                                     (only forced edge: T03C commits before T03B)
Wave 4  T04A ∥ T04B                  replay + epoch protocol + admission parity, vehicle range
Wave 5  T05A -> T05B                 account-id filenames, THEN save MAC
Wave 6  T06A ∥ T06B                  loader strictness, clock authority
Wave 7  T07A -> T07B                 frontend loopback, persistence authority
Wave 8  T08                          gate script, exe20, _active.md
```

**Forced orderings (verified, not assumed):**
- **T00E is the first commit of the milestone.** Review round 2 caught that the gate script was
  scheduled in T08/commit 20, *after* every task whose pure-UE evidence it is supposed to provide.
  That made the mandated "fails before the change, passes after" impossible for commits 1, 3, 6, 10,
  11 and 18, and turned the gate into retrospective validation. The harness scaffold therefore lands
  **before T00A**, and each owning task adds its own assertion, watches it go RED, then implements.
  T08 no longer creates the gate; it wires the finished gate into the spine.
- **T00A internally serialized.** The secret provider must exist in the same commit that removes the
  committed secret, and must convert *every* consumer — world, district, handoff, chat, relay. A
  role with a missing or malformed secret halts before it listens.
- **T00C before any password RPC.** Registration and login must not be reachable over an
  unencrypted connection at any commit boundary. This is why transport encryption is Wave 0.
- **T00D before Wave 4.** Handoff and replay work builds on relay frames; authenticating those
  frames afterwards would mean re-testing every admission path.
- **T01 before Wave 2.** PBKDF2 salts and ticket jti both need `SecureRandomBytes`.
- **T02A is indivisible.** The 600k iteration count, the bounded auth executor, the per-connection
  and global auth limiter, and account uniqueness ship together or not at all. Landing the cost
  increase without the executor creates a cheaper DoS than exists today; landing async auth without
  uniqueness creates session fixation.
- **T03C before or with T03B.** Do not widen the RPC surface while private state is still broadcast
  to every client.
- **T05A before T05B.** The MAC's `logical_doc_name` embeds the account id. Convenience, not
  security — if `logical_doc_name` is defined independently of the physical filename the order can
  relax, but keeping it avoids recomputing every character MAC.

**Downgraded to preference (review round 1 correction, re-applied round 2 after the first pass left
the forced edges in the graph and the task bodies):**
- T03A before T03B reads well but is not logically forced; identity could attach to an opaque
  session object without first rekeying the map. The wave graph and T03B's `Dependencies:` line now
  say so. The one edge that *is* binding in Wave 3 is the commit order T03C before T03B.
- T04A and T04B share no file. T04B is independent once its prerequisites land. T04B's
  `Dependencies:` line now says so.

## Scenario contract

Binding. Every scenario needs **two** captured artifacts: a RED->GREEN test transcript and a
real-surface artifact. "Tests pass" alone is not evidence.

**Harness boundary (self-found defect, review round 1).** `tests\AGENTS.md` states the standalone
harness "does not cover replication, GameModes, UMG, or runtime networking", and names "treating a
standalone Domain pass as proof that replication or UI behavior works" as an anti-pattern. The first
draft of this contract violated that in four rows — it named domain suites for an encryption
handshake, for owner-only replication, and for a UE-side secret provider. It also named three suites
that **do not exist** (`run_world_tests.cpp`, `run_social_tests.cpp`, `run_mission_tests.cpp`). Both
classes of error would have been discovered by an implementer mid-task, with the cheapest escape being
a domain test that asserts nothing about the real behavior.

The rule from here:

| Scenario nature | RED->GREEN artifact | Real-surface artifact |
|---|---|---|
| Pure Domain logic | the owning `run_*_tests.cpp` suite, which must already exist or be added to `build_and_run.ps1` in the same commit | gate or process run |
| Mixed (domain rule + UE enforcement) | domain half in the owning suite **and** a gate assertion for the UE half — both required | gate or process run |
| Pure UE layer (replication conditions, RPC identity, encryption, connection lifecycle) | a **gate assertion** in `run_m16_zerotrust_gate.ps1` that fails before the change and passes after; there is no domain half and none may be invented | mandatory — process run, log, or dump |

Never name a suite that does not exist. The real suites are fixed by `build_and_run.ps1`
(`$exe..$exe19`); the only new one this milestone is `$exe20`.

**Wave exit criteria (why some scenarios carry no TODO).** Five scenarios are deliberately not owned
by any task TODO: S-W0-1 (validly ticketed admission still works), S-W0-4 (travel path unchanged),
S-W2-5 (all existing suites still `FAILS=0`), S-W3-6 (`frontend_menu` probe unchanged), and S-W5-4
(`m16_persistence` still green). These are **wave exit criteria**, not task work — a task cannot
"implement" them. S-W6-3 is superseded by S-W2-6 and is retained only for traceability.

A wave is not complete until its regression scenarios are re-run and captured. Assign them to the
wave, not to a task:

| Wave | Must re-pass before the wave closes |
|---|---|
| 0 | S-W0-1, S-W0-4 |
| 1-2 | S-W2-5 |
| 3 | S-W3-6 |
| 4-5 | S-W5-4 |
| 6-8 | S-W8-1, S-W8-2 |

S-W0-1 in particular is the positive control for all of Wave 0: it proves that hardening admission did
not simply break admission. Capturing only the refusals would leave "everything is refused" passing.

### Wave 0

| ID | Class | Binary observable | Test | Real surface |
|---|---|---|---|---|
| S-W0-1 | happy | District admits a validly ticketed client | `run_auth_tests.cpp` `TestTicketRoundTrip` | `tools\run_m7_directory_gate.ps1` -> `M7_DIRECTORY_GATE_OK` + `LEAKED=0` |
| S-W0-2 | malicious | Ticketless join is **refused**: client connects to the district with no ticket, server replies `{"error":"no_ticket"}` and the client never enters `AdmittedRoster` | `run_auth_tests.cpp` `TestTicketlessAdmissionRefused` | Game target `-WorldServer -nullrhi -nosound -unattended`, no `-RequireTicket` flag; log shows `no_ticket` |
| S-W0-3 | malicious | Malformed ticket does **not** kill the process: `json_int` receives `"exp":"999999999999999999999999"` and returns a typed failure; process stays alive | `run_auth_tests.cpp` `TestJsonIntOverflowContained` | Harness exit 0 where it previously aborted |
| S-W0-4 | regression | Existing travel path unchanged | `run_handoff_tests.cpp` full suite | `tools\run_m6_world_gate.ps1` -> `WORLD_SERVER_GATE_OK` |
| S-W0-5 | malicious | A role with a **missing or malformed** secret halts before it listens: no port bound, loud failure, not a silent degrade | mixed: `run_zerotrust_tests.cpp` `TestSecretMaterialRejected` (domain half — the length/encoding predicate rejects empty, short, and non-hex input) **and** a `run_m16_zerotrust_gate.ps1` assert (UE half — the role halts before binding) | Launch with the env var unset; `Get-NetTCPConnection -LocalPort 7777` returns nothing |
| S-W0-6 | happy + malicious | **Positive control first:** two processes with the **same** injected secret admit successfully. Then the **mismatched** case is refused, and the **missing** case halts. All three cases, not just the refusal | `run_auth_tests.cpp` `TestSecretInterop` | Three two-process runs: same / mismatched / missing |
| S-W0-7 | happy | Connection reports **encryption active** before any password RPC is reachable | pure UE: `run_m16_zerotrust_gate.ps1` assert `ENCRYPTION_ACTIVE` — no domain half exists and none may be invented | Netdriver log shows `AESGCMHandlerComponent` engaged |
| S-W0-8 | malicious | Login and registration are **refused** on an unencrypted connection | pure UE: `run_m16_zerotrust_gate.ps1` assert `AUTH_REFUSED_PLAINTEXT` | Force-disable the handler client-side; server refuses auth |
| S-W0-9 | malicious | A frame with a **valid bearer secret but a forged HMAC** is refused; constant-time verify | `run_relay_tests.cpp` `TestRelayFrameHmac` | Hand-crafted frame over the relay socket, refused |
| S-W0-10 | malicious | District X, already registered, sends `PlayerJoined` with `numeric_id` of district Y -> refused; Y's roster unchanged | `run_relay_tests.cpp` `TestCrossDistrictIdentityRefused` | Two-district relay run, forged frame, roster dump identical |
| S-W0-11 | edge | Relay connection flood hits the pre-auth cap and is refused; existing districts stay connected | `run_relay_tests.cpp` `TestRelayConnectionFlood` | Open N+1 sockets, confirm cap and that registered districts survive |

Literal invocations:

```powershell
powershell -File D:\APBReloaded\tests\build_and_run.ps1
powershell -File D:\APBReloaded\tools\run_m7_directory_gate.ps1
powershell -File D:\APBReloaded\tools\run_m6_world_gate.ps1
```

### Wave 1-2 (crypto foundation)

| ID | Class | Binary observable | Test | Real surface |
|---|---|---|---|---|
| S-W1-1 | happy | `SecureRandomHex(32)` returns 64 lowercase hex chars, 1000 calls yield 1000 distinct values | `run_zerotrust_tests.cpp` `TestSecureRandomDistinct` | `APBZeroTrustTests.exe` exit 0 |
| S-W1-2 | malicious/edge | Entropy failure **fails closed**: forced `BCryptGenRandom` failure propagates a typed error and no caller receives a PRNG value | `run_zerotrust_tests.cpp` `TestEntropyFailureFailsClosed` | exit 0 |
| S-W2-1 | happy | PBKDF2-HMAC-SHA256 at 600k iterations matches a published RFC 6070-style vector | `run_zerotrust_tests.cpp` `TestPBKDF2Vector600k` | exit 0 |
| S-W2-2 | malicious | Plaintext-password login is **impossible**: a hand-written `accounts.json` with a plaintext field is `Rejected`, not authenticated | `run_zerotrust_tests.cpp` `TestPlaintextBranchDeleted` | Dump `accounts.json`, attempt login through `run_m6_world_gate.ps1`, expect refusal |
| S-W2-3 | malicious | Unknown username does **not** auto-register: `LoginPlayer("nobody")` refuses and `accounts.json` is byte-identical before and after | `run_domain_tests.cpp` `TestNoAutoRegister` | `Get-FileHash accounts.json` identical pre/post |
| S-W2-4 | malicious | A district constructed **without** an injected secret refuses to verify rather than minting its own. Paired with the S-W0-6 positive control, which proves same-secret interop actually works — the first draft asserted only the refusal, which passes even if injection is wired to nothing | `run_auth_tests.cpp` `TestNoImplicitSecret` | Two-process gate, unconfigured district rejects a world ticket |
| S-W2-5 | regression | All 19 existing suites still `FAILS=0` | `build_and_run.ps1` | `HARNESS_EXIT=0` |
| S-W2-6 | edge | The bounded auth queue **rejects** when full rather than growing; the game thread stays responsive while it is saturated **and overflowed** | `run_zerotrust_tests.cpp` `TestAuthQueueRejectsWhenFull` | Saturate then overflow the queue while sampling a game-thread heartbeat against a fixed deadline |
| S-W2-7 | malicious | Auth attempts past the per-connection limit are refused; the global limit holds under many connections | `run_zerotrust_tests.cpp` `TestAuthRateLimited` | Scripted burst of login attempts, refusals logged |
| S-W2-8 | **malicious** | A **stale** PBKDF callback is rejected: disconnect the client mid-derivation, then reconnect. The old result must not authenticate the new session, and a second account login during an outstanding derivation must not cross wires | mixed: `run_zerotrust_tests.cpp` `TestStaleAuthCallbackRejected` (domain half — a result carrying a superseded generation nonce is discarded) **and** a `run_m16_zerotrust_gate.ps1` assert (UE half — disconnect/reconnect mid-derivation) | Two-client script with an artificial derivation delay; log shows generation-nonce rejection |

### Wave 3 (identity — the core of the directive)

| ID | Class | Binary observable | Test | Real surface |
|---|---|---|---|---|
| S-W3-1 | happy | Authenticated player A sends `Server_SubmitMailCommand{op=GetInbox}` and receives **only** A's inbox | `run_mail_tests.cpp` `TestInboxOwnerScoped` | Two-client `run_m6_world_gate.ps1`, log shows A's mail ids only |
| S-W3-2 | **malicious** | **Positive control first:** B deletes B's own mail and it succeeds, proving delete is actually wired. **Then** A sends `Server_SubmitMailCommand{op=Delete, target=<B's mail id>}` -> denied, and after a **forced save and reload** B's mail is still present. The first draft asserted only a byte-identical global file, which passes when delete never dispatches at all and also misses an in-memory mutation persisted later | `run_mail_tests.cpp` `TestCrossAccountMailDeleteDenied` + `TestOwnerMailDeleteSucceeds` | Owner delete succeeds, cross-owner denied, then save/reload and re-read the inbox |
| S-W3-3 | **malicious** | Client A sends `Server_SubmitClanCommand{op=SetMemberRank, actor=<B's character>, rank=0}` -> denied; acting character is taken from `FPlayerSession`, never from the payload | `run_clan_tests.cpp` `TestClanActorSpoofDenied` (domain half: rank change requires an explicit actor identity) + `run_m16_zerotrust_gate.ps1` assert (UE half: actor comes from `FPlayerSession`) | Server log denial + clan state unchanged |
| S-W3-4 | **malicious** | `Server_IssueTicket` with an unowned character name -> denied and **no character is created** | `run_domain_tests.cpp` `TestIssueTicketNoImplicitCreate` (domain half: character lookup never creates) + `run_m16_zerotrust_gate.ps1` assert (UE half: RPC denial) | `characters\` directory listing unchanged |
| S-W3-5 | **malicious** | Mail send debits the sender atomically: A sends 1000 cash to B, A's balance drops exactly 1000, total currency across A+B is conserved | `run_mail_tests.cpp` `TestMailSendDebitsSender` | Dump both character files, sum cash pre/post |
| S-W3-6 | regression | `frontend_menu` probe still emits `TRAVEL_OPENLEVEL_CALLED` then `FRONTEND_MENU_OK` | `tools\run_verification_gates.ps1` | `GATE_PASS` |
| S-W3-7 | **malicious** | Owner A receives its own `Cash`/`G1C`/`IssuedTicketJson` sentinel values; client B **never receives them at all**. Assert on B's received property values, not on B's UI | pure UE: `run_m16_zerotrust_gate.ps1` assert `OWNER_ONLY_REPLICATION_OK` — replication is explicitly outside the harness | Two-client run with distinctive sentinel values; dump B's replicated `APBPlayerState` for A and confirm the sentinels are absent |

### Wave 4-5 (replay + persistence integrity)

| ID | Class | Binary observable | Test | Real surface |
|---|---|---|---|---|
| S-W4-1 | happy | A ticket verifies and consumes in one atomic `VerifyAndConsume` call | `run_auth_tests.cpp` `TestVerifyAndConsumeAtomic` | `run_m7_directory_gate.ps1` -> `M7_DIRECTORY_GATE_OK` |
| S-W4-2 | **malicious** | Replaying the same ticket twice: the second attempt is refused with `no_ticket`, even when issued in the same millisecond | `run_auth_tests.cpp` `TestReplayRefused` | Two sequential joins with an identical ticket blob; second logs refusal |
| S-W4-3 | **malicious** | A ticket carrying a **stale boot epoch** (server restarted) is refused | `run_auth_tests.cpp` `TestStaleBootEpochRefused` | Restart the world process, replay a pre-restart ticket, expect refusal |
| S-W4-4 | edge | `ReplayWindow` at hard capacity **rejects** rather than evicting | `run_zerotrust_tests.cpp` `TestReplayWindowRejectsWhenFull` | exit 0 |
| S-W4-5 | **malicious** | **District** restart invalidates every pre-restart ticket for that district, so the lost in-memory consumed-jti set cannot be exploited. Ticket **T** is issued **and actually consumed** by a successful admission to district D; ticket **U** is issued for D and never presented. D restarts. **Both** T and U are then refused with `epoch_mismatch`. Both cases matter: refusing only T would mean the district is still trusting its lost replay window, and refusing only U would mean consumption is doing the work the epoch is supposed to do | mixed: `run_auth_tests.cpp` `TestStaleDistrictEpochRefused` (domain half — both a consumed and an unconsumed jti bearing a stale `target_district_epoch` are refused) **and** the `DISTRICT_EPOCH_RESTART_REFUSED` gate assertion (UE half — `APBServerControl` publishes the epoch and `APBDistrictGameMode` compares it against the *running* district's epoch; the harness cannot reach either) | Two-process run: issue T, **admit with T so it is consumed**, issue U and hold it, `Stop-Process` district D, relaunch D, present T -> refusal, present U -> refusal, then issue and present a fresh ticket -> **admits**, proving the path still works |
| S-W5-1 | **malicious** | Edit cash to a **different but syntactically valid number** so the document still parses, then require `IntegrityFailure` **before** parsing and confirm a previously loaded sentinel is unchanged. The first draft's "flip one byte" passes via schema rejection or default-initialisation without the MAC ever being checked | `run_persistence_tests.cpp` `TestSaveMacDetectsEdit` | Edit `"cash": 500` to `"cash": 999999`, launch, assert `IntegrityFailure` and that the in-memory balance is neither 999999 nor a default |
| S-W5-2 | **malicious** | Cross-document MAC transplant fails: copy A's `data`+`mac` into B's file -> `IntegrityFailure` (domain separation binds `logical_doc_name`) | `run_persistence_tests.cpp` `TestMacDomainSeparation` | Swap files, expect refusal |
| S-W5-3 | edge | **Two separate cases.** (a) Collision -> HALT, nothing written. (b) **Successful** migration is idempotent: a clean run converts every account, and a second run changes nothing. The first draft's single "run twice" passes by halting twice and never proving migration works | `run_persistence_tests.cpp` `TestMigrationHaltsOnCollision` + `TestMigrationIdempotent` | Case (a): seeded collision, directory unchanged. Case (b): clean run, diff directory after run 2 |
| S-W5-4 | regression | `m16_persistence` gate still green | `run_verification_gates.ps1` | `GATE_PASS` |

### Waves 6-8

| ID | Class | Binary observable | Test | Real surface |
|---|---|---|---|---|
| S-W6-1 | **malicious** | One malformed record rejects the **whole** document: a 3-account `accounts.json` with account 2 corrupted loads **zero** accounts and returns `SchemaFailure` | `run_persistence_tests.cpp` `TestLoaderRejectsWholeDoc` | Corrupt a record, launch, log shows `SchemaFailure` |
| S-W6-2 | **malicious** | Client cannot advance the mission clock: `TickMission` no longer accepts a client timestamp; a client sending a huge delta cannot complete a mission early | `run_domain_tests.cpp` `TestMissionClockServerAuthoritative` (mission tests already live here, `run_domain_tests.cpp:248-311`) | Build exit 0 + gate log shows server-derived elapsed |
| S-W6-3 | moved | Superseded by S-W2-6. "No hitch warning" was machine- and threshold-dependent and proved neither bounded execution nor game-thread responsiveness; the replacement measures a game-thread heartbeat against a fixed deadline while the auth queue is saturated and overflowed | — | — |
| S-W7-1 | happy | Registration goes through the server: `OnRegisterClicked` -> `Server_RegisterRequest` -> account exists | `run_m16_zerotrust_gate.ps1` assert (UE-layer RPC; no domain half exists) | `run_m6_world_gate.ps1`, `accounts.json` gains the account |
| S-W7-2 | **malicious** | No in-process registration path remains reachable from the client build; the unconditional `NativeConstruct` QA seed is gone | grep-assert in `run_m16_zerotrust_gate.ps1` | Launch the client with no flags; `accounts.json` gains **no** `player1` |
| S-W7-3 | **malicious** | Second login for the same account kicks the first, **and the kicked session's next mutation attempt is denied**. A logged kick alone does not prove the old connection lost mutation authority | `run_domain_tests.cpp` `TestOneSessionPerAccount` (domain half: authority refuses a second live session for the same account) + `run_m16_zerotrust_gate.ps1` assert (UE half: kicked connection denied) | Two clients same account; after the kick, drive a mutation from the old connection and confirm denial |
| S-W7-4 | **malicious** | **Different accounts** mutating global documents concurrently lose no updates: A and B both write mail/auction in the same tick, and both writes survive | `run_persistence_tests.cpp` `TestConcurrentDifferentAccountWrites` | Two-client concurrent write script, then read both records back from disk |
| S-W8-1 | happy | Full gate spine green with the new gate wired in | `run_verification_gates.ps1` | `GATE_PASS` + `M16_ZEROTRUST_GATE_OK` |
| S-W8-2 | regression | 20 suites `FAILS=0`, all gates green, `LEAKED=0` | `build_and_run.ps1` | `HARNESS_EXIT=0` |

## Tasks

### Wave 0 — gate scaffold FIRST, then live P0s, secrets, transport, relay
### (T00E, then T00A ∥ T00B ∥ T00C ∥ T00D)

**T00E — gate harness scaffold (must be the milestone's first commit)**

Review round 2 blocker: every pure-UE scenario's RED->GREEN evidence lives in
`run_m16_zerotrust_gate.ps1`, but the first revision only created that file in T08 — after all six
tasks that need it. This task creates the harness **empty of assertions** so each owning task can add
its assertion, observe RED, then implement. It asserts nothing about zero-trust behavior itself, so it
has no RED->GREEN of its own; it is infrastructure, and the scenario contract's test-first rule
applies to the assertions added later by their owning tasks.

Files:
- `tools\run_m16_zerotrust_gate.ps1` — **new**. Harness only: `-Scratch` path, process launch and
  log-capture helpers, an assertion registry that reports `FAIL reason=<first-failure>`, and teardown
  on the `run_m7_directory_gate.ps1` precedent (`Stop-AllGateProcesses`, then
  `leaked = GateProcesses.Count + BoundPortCount` -> `LEAKED=n`). Emits `M16_ZEROTRUST_GATE_OK` when
  the registry is empty or all-green.
- **`-Only <ASSERTION_NAME>` is mandatory, not a convenience** (review round 2 blocker 3). Because the
  registry reports only `FAIL reason=<first-failure>`, a whole-gate run cannot produce a per-assertion
  RED transcript: the second RED is masked by the first, and an assertion that legitimately goes GREEN
  cannot turn the whole gate green while a later one is still RED. `-Only` runs exactly one assertion
  and reports that assertion's own state. It also accepts a repeatable form so a task that owns two
  assertions can still run them separately.

**Binding rule for every task that adds an assertion (round-2 blocker 3):** add **one** assertion, run
it **alone** with `-Only`, capture its RED, then make the **complete** production change that assertion
describes, then re-run `-Only` for GREEN. Never add two assertions in one step. Never sequence a
production edit before the RED of the assertion covering it. An assertion's GREEN step must be the
*whole* behavior it names — declaring an RPC is not routing a caller to it, and enabling a plugin is
not establishing an encrypted session.

Assertion ownership (each added by its own task, RED before that task's production edit):

| Assertion | Owning task | Scenario |
|---|---|---|
| `ENCRYPTION_ACTIVE` | T00C | S-W0-7 |
| `AUTH_REFUSED_PLAINTEXT` | T00C | S-W0-8 |
| `SECRET_PROVIDER_HALTS` | T00A | S-W0-5 (UE half) |
| `STALE_AUTH_CALLBACK_REJECTED` | T02A | S-W2-8 (UE half) |
| `CLAN_ACTOR_SPOOF_DENIED` | T03B | S-W3-3 (UE half) |
| `ISSUE_TICKET_DENIED` | T03B | S-W3-4 (UE half) |
| `OWNER_ONLY_REPLICATION_OK` | T03C | S-W3-7 |
| `SERVER_REGISTER_ROUTED` | T07A | S-W7-1 |
| `NATIVECONSTRUCT_SEED_GATED` | T07A | S-W7-2 |
| `DISTRICT_EPOCH_RESTART_REFUSED` | T04A | S-W4-5 (UE half) |
| `ONE_SESSION_KICK_DENIES_MUTATION` | T07B | S-W7-3 (UE half) |

Atomic TODO:
```
tools\run_m16_zerotrust_gate.ps1: create the assertion-registry harness with teardown receipts for S-W8-1 - verify by exit 0 with M16_ZEROTRUST_GATE_OK and LEAKED=0 on an empty registry
```

Delegation: `category="unspecified-high"`, `load_skills=["programming"]`
Dependencies: none. **Blocks T00A, T00C, T02A, T03B, T03C, T07A, T07B.**
Commit: `chore(m16): zerotrust gate harness scaffold with teardown receipts`

---

**T00A — C1 + C2 + secret provider: one commit, every consumer**

Blocker 1 rewrote this task. The first draft removed the committed secret in commit 1 but only taught
the *world* role to read an injected one in commit 5. Every other consumer reads `GConfig` directly,
so districts, handoff, chat, and relay would have been broken across four commits — and the only
way to keep gates green in that window is the `RequireTicket=False` workaround this plan forbids.
The provider therefore ships in the same commit as the removal.

Files:
- `Source\APBReloaded\Systems\Server\APBSecretProvider.h` / `.cpp` — **new**. Single UE-side
  provider: reads an env var or a secret file, validates length and encoding, exposes per-purpose
  secrets (ticket, handoff, relay, save). A missing or malformed secret **halts the role before it
  listens** — no listener, no partial service, loud failure.
- `Config\DefaultGame.ini` — remove the committed placeholder `TicketSecret` from `[APBServer]`.
  Add `RequireTicket=True`.
- Convert **every** consumer in the same commit, both roles:
  `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` (ticket issue + handoff paths),
  `Source\APBReloaded\Systems\District\APBDistrictGameMode.cpp` (verify + chat paths),
  `Source\APBReloaded\Systems\Server\APBServerControl.cpp` (relay bearer).
  Re-grep for `GConfig` reads of any secret before declaring this complete; the audit found five
  call sites and there may be more.
- `Source\APBReloaded\Systems\District\APBDistrictGameMode.cpp` — `RequiresDistrictTicket()`
  defaults to **true**; absent configuration means "require", not "allow".
- `Source\APBReloaded\Systems\APBFreeroamCharacter.cpp` — `ServerFireWeapon` -> `Unreliable`.
- `tools\run_m6_world_gate.ps1`, `tools\run_m7_directory_gate.ps1` — inject the secret via the
  provider's env var. **Do not restore the placeholder to keep a gate green.**

Atomic TODOs:
```
Source\...\Domain\APBCrypto.h: add the secret-material validity predicate (length, encoding) for S-W0-5 - verify by TestSecretMaterialRejected RED then GREEN
tools\run_m16_zerotrust_gate.ps1: add ONLY the SECRET_PROVIDER_HALTS assertion, run it alone for S-W0-5 - verify by gate -Only SECRET_PROVIDER_HALTS RED reason=role_listened_without_secret
Source\...\Server\APBSecretProvider.cpp: halt the role before it listens on missing or malformed secret material for S-W0-5 - verify by TestSecretMaterialRejected RED then GREEN and gate -Only SECRET_PROVIDER_HALTS flipping RED to GREEN, port 7777 unbound
Config\DefaultGame.ini: remove committed TicketSecret and set RequireTicket=True for S-W0-2 - verify by grep finds no hex secret in tracked config
Source\...\APBWorldGameMode.cpp: read ticket and handoff secrets from the provider for S-W0-6 - verify by two-process same-secret admission succeeds
Source\...\APBDistrictGameMode.cpp: read verify and chat secrets from the provider for S-W0-6 - verify by two-process mismatched-secret admission refused
Source\...\APBServerControl.cpp: read the relay bearer from the provider for S-W0-6 - verify by relay registration succeeds with injected secret
Source\...\APBDistrictGameMode.cpp: default RequiresDistrictTicket to true for S-W0-2 - verify by TestTicketlessAdmissionRefused RED then GREEN
Source\...\APBFreeroamCharacter.cpp: mark ServerFireWeapon Unreliable for decision 4 - verify by build exit 0
tools\run_m6_world_gate.ps1: inject the secret via provider env var for S-W0-6 - verify by WORLD_SERVER_GATE_OK
```

Delegation: `category="ultrabrain"`, `load_skills=["programming", "ast-grep"]`
Dependencies: T00E (this task adds the `SECRET_PROVIDER_HALTS` assertion to the harness). Parallel
with T00B, T00C, T00D.
Commit: `sec(secrets): deployment secret provider, drop committed TicketSecret, RequireTicket default on`

---

**T00C — N12: transport encryption (was wrongly ruled out of scope)**

The first draft's technical premise was false. UE 5.8 ships the AES-GCM packet handler; this needs no
engine edit, no custom `NetDriver`, and no `TargetType.Server` build.

Files:
- `Config\DefaultEngine.ini` — enable the shipped handler
  (`EncryptionComponent=AESGCMHandlerComponent`, per `BaseEngine.ini:3575`) and require encryption on
  the world NetDriver.
- `APBReloaded.uproject` — enable the `AESGCMHandlerComponent` runtime plugin.
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` — implement authenticated per-connection
  key establishment through UE's encryption-token delegates. Authenticate the **server** with a
  pinned deployment public key; derive an ephemeral session key using vetted platform crypto
  (`BCrypt*`, consistent with T01). Never roll a key exchange by hand.
- Login and registration **refuse** unless the connection reports encryption active. A nonce-based
  password proof over plaintext is not an acceptable substitute: an active relay can still hijack the
  session.

Atomic TODOs:
```
tools\run_m16_zerotrust_gate.ps1: add ONLY the ENCRYPTION_ACTIVE assertion, run it alone for S-W0-7 - verify by gate -Only ENCRYPTION_ACTIVE RED reason=encryption_inactive
APBReloaded.uproject + Config\DefaultEngine.ini + APBWorldGameMode.cpp: enable AESGCMHandlerComponent, require encryption on the world NetDriver, and establish an authenticated per-connection key with a pinned server key for S-W0-7 - verify by gate -Only ENCRYPTION_ACTIVE flipping RED to GREEN (one step: the plugin and config alone do not establish an encrypted session, so neither can carry this assertion's GREEN)
tools\run_m16_zerotrust_gate.ps1: add ONLY the AUTH_REFUSED_PLAINTEXT assertion, run it alone for S-W0-8 - verify by gate -Only AUTH_REFUSED_PLAINTEXT RED reason=plaintext_auth_accepted
Source\...\APBWorldGameMode.cpp: refuse login and registration on an unencrypted connection for S-W0-8 - verify by gate -Only AUTH_REFUSED_PLAINTEXT flipping RED to GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: T00E (this task adds `ENCRYPTION_ACTIVE` and `AUTH_REFUSED_PLAINTEXT`). Parallel with
T00A, T00B, T00D.
Commit: `sec(transport): AES-GCM packet encryption, refuse auth on plaintext (N12)`

---

**T00D — N14: relay message authentication and identity binding**

Files:
- `Source\APBReloaded\Domain\APBRelayProtocol.h` / `.cpp` — stop shipping the bearer secret in the
  frame. Per-frame HMAC over the deterministically encoded envelope **excluding** the `auth` field,
  verified in constant time, using a distinct per-purpose relay key from the T00A provider. Strict
  verb-direction allowlists.
- `Source\APBReloaded\Systems\Server\APBServerControl.cpp` — bind `numeric_id` to the connection at
  `Register` and reject any later frame claiming a different district. Duplicate-district connection
  policy. Total connection cap, pre-auth connection cap, and handshake rate limit. This is also where
  the `FRelayClient` limiter from decision 5 actually lives — the first draft named it but never
  listed this file.
- Encrypt handoff bodies if the relay is ever permitted off a private host network.

Atomic TODOs:
```
Source\...\Domain\APBRelayProtocol.cpp: per-frame HMAC over the envelope excluding auth, constant-time verify for S-W0-9 - verify by TestRelayFrameHmac RED then GREEN
Source\...\Domain\APBRelayProtocol.cpp: strict verb-direction allowlists for N14 - verify by TestRelayVerbDirection RED then GREEN
Source\...\APBServerControl.cpp: bind numeric_id to the connection after Register for S-W0-10 - verify by TestCrossDistrictIdentityRefused RED then GREEN
Source\...\APBServerControl.cpp: add connection cap, pre-auth cap, and handshake rate limit for S-W0-11 - verify by TestRelayConnectionFlood RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: none. Parallel with T00A, T00B, T00C.
Commit: `sec(relay): per-frame HMAC, connection-bound district identity, caps (N14)`

---

**T00B — N6 + N7 + N8 + N9: ticket codec hardening**

Files:
- `Source\APBReloaded\Domain\APBTicket.cpp` — `json_int`: contain `std::stoll`
  (`try`/`catch` on `std::invalid_argument` and `std::out_of_range`, or a hand-rolled bounded
  parse) and return a typed failure. Payload write: escape strings. `b64url_decode`: reject
  invalid characters instead of skipping them.
- `Source\APBReloaded\Domain\APBCrypto.h` — `hex_decode`: reject odd length and non-hex digits.

Atomic TODOs:
```
Source\...\APBTicket.cpp: contain std::stoll in json_int for S-W0-3 (remote DoS) - verify by TestJsonIntOverflowContained RED then GREEN
Source\...\APBTicket.cpp: escape ticket payload strings for N7 - verify by TestTicketPayloadEscaped RED then GREEN
Source\...\APBTicket.cpp: reject invalid b64url characters for N8 - verify by TestB64UrlRejectsInvalid RED then GREEN
Source\...\APBCrypto.h: reject odd-length and non-hex in hex_decode for N9 - verify by TestHexDecodeRejectsMalformed RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: none. Parallel with T00A.
Commit: `sec(ticket): harden json_int/b64url/hex_decode/payload-escape (N6-N9)`

---

### Wave 1 — CSPRNG foundation (T01, blocks all crypto below)

**T01 — Q4b: CSPRNG foundation and the exe20 suite**

Files:
- `Source\APBReloaded\Domain\APBCrypto.cpp` — **new**. `SecureRandomBytes` / `SecureRandomHex` over
  `BCryptGenRandom(nullptr, buf, len, BCRYPT_USE_SYSTEM_PREFERRED_RNG)` behind `#ifdef _WIN32`
  (precedent: `APBPersistence.cpp` `gmtime_s`/`gmtime_r`). No PRNG fallback. Entropy failure returns
  a typed error and every caller fails closed.
- `Source\APBReloaded\Domain\APBCrypto.h` — declare the two functions; `random_hex` becomes a
  deleted or forwarding shim, never a PRNG path.
- `Source\APBReloaded\APBReloaded.Build.cs` — add `bcrypt.lib` under a Win64 conditional
  (`PublicSystemLibraries` does not exist yet; create it).
- `tests\run_zerotrust_tests.cpp` — **new, created here.** This is the task that brings `$exe20`
  into existence, because the first CSPRNG tests need somewhere to live. T08 only closes out the
  remaining suites; it does not create the file. Follow the harness convention exactly:
  `static int fails`, `CHECK`, `printf("FAILS=%d")`, `return fails ? 1 : 0`.
- `tests\build_and_run.ps1` — add `APBCrypto.cpp` to `$srcs`, add `$exe20 = "$out\APBZeroTrustTests.exe"`
  with its `cl` line, and add `/link bcrypt.lib` to every affected exe. Affected set: the `$srcs` base
  (because `APBWorldService.cpp` -> `RegisterAccount` -> salt), plus `$exe4` `APBAuthTests`
  (`APBTicket.cpp` jti), `$exe18` `APBHandoffTests` (`APBHandoff.cpp` nonce), and the new `$exe20`.
  The harness links zero libs today, so this is the first link flag it carries — verify all 20 exes
  still build.

Atomic TODOs:
```
tests\run_zerotrust_tests.cpp: create the exe20 suite file following the harness CHECK/FAILS convention for S-W1-1 - verify by cl compiles it and it prints FAILS=
tests\build_and_run.ps1: add exe20 APBZeroTrustTests with its cl line for S-W1-1 - verify by HARNESS_EXIT=0 across 20 suites
Source\...\Domain\APBCrypto.cpp: add SecureRandomBytes/SecureRandomHex over BCryptGenRandom for S-W1-1 - verify by TestSecureRandomDistinct RED then GREEN
Source\...\Domain\APBCrypto.cpp: fail closed on entropy failure for S-W1-2 - verify by TestEntropyFailureFailsClosed RED then GREEN
APBReloaded.Build.cs: add Win64 bcrypt.lib for T01 - verify by APBReloadedEditor build exit 0
tests\build_and_run.ps1: add APBCrypto.cpp to srcs and bcrypt.lib to exe4/exe18/exe20 for T01 - verify by HARNESS_EXIT=0 across all suites
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: Wave 0 complete.
Commit: `sec(crypto): BCryptGenRandom CSPRNG, no PRNG fallback (Q4b)`

---

### Wave 2 — auth pipeline, ticket de-singleton, fail-closed (T02A ∥ T02B ∥ T02C)

**T02A — the auth pipeline, indivisible (D9 + bounded executor + limiter + account uniqueness)**

Blockers 2 and 3 collapsed four first-draft tasks into one. Raising PBKDF2 from 10,000 to 600,000
iterations while login still runs synchronously on the game thread makes the game-thread DoS
**cheaper than it is today** — that state must never exist at a commit boundary. And dispatching auth
to a worker without account-uniqueness enforcement in the same commit opens session fixation: a stale
PBKDF result can land after disconnect, reconnect, a duplicate login, or a second account's login.

Files:
- `Source\APBReloaded\Domain\APBSocial.h` — `AccountRecord` gains the versioned KDF record
  `{scheme, iterations, dk_bytes, salt, hash}`. `Register` draws a ≥16-byte salt from
  `SecureRandomBytes`. `Login` returns `{AuthenticatedCurrent, AuthenticatedNeedsRehash, Rejected}`;
  the **plaintext branch is deleted**. Opportunistic rehash on success, **no** forced re-salt.
  Expose a **pure** verification entry point that takes an immutable credential snapshot and mutates
  nothing — `LoginAccount` currently mutates `LoginService::session` and may rehash, so it must never
  be called from a worker thread.
- `Source\APBReloaded\Systems\Server\APBAuthExecutor.h` / `.cpp` — **new**. Bounded worker pool.
  One in-flight auth per connection. A bounded global queue that **rejects** when full rather than
  growing. Per-connection and global auth/registration rate limits.
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` — the full pipeline:
  1. Game thread: snapshot immutable credential data.
  2. Worker: pure KDF verification only.
  3. Return via a **weak controller reference plus a session-generation nonce**.
  4. Game thread: re-resolve the live session, **reject stale generations**, atomically enforce
     one-active-session-per-account, apply any rehash, persist, mark authenticated.
- Offline migration command for existing records.

Atomic TODOs:
```
Source\...\Domain\APBSocial.h: versioned KDF record at 600k iterations for S-W2-1 - verify by TestPBKDF2Vector600k RED then GREEN
Source\...\Domain\APBSocial.h: add a pure verification entry point that mutates no session state for blocker 3 - verify by TestPureVerifyNoMutation RED then GREEN
Source\...\Domain\APBSocial.h: delete plaintext login branch, return typed AuthResult for S-W2-2 - verify by TestPlaintextBranchDeleted RED then GREEN
Source\...\Domain\APBSocial.h: opportunistic rehash on AuthenticatedNeedsRehash - verify by TestRehashOnLogin RED then GREEN
Source\...\Server\APBAuthExecutor.cpp: bounded pool with one in-flight auth per connection and a rejecting global queue for S-W2-6 - verify by TestAuthQueueRejectsWhenFull RED then GREEN
Source\...\Server\APBAuthExecutor.cpp: per-connection and global auth rate limits for S-W2-7 - verify by TestAuthRateLimited RED then GREEN
tools\run_m16_zerotrust_gate.ps1: add ONLY the STALE_AUTH_CALLBACK_REJECTED assertion, run it alone for S-W2-8 - verify by gate -Only STALE_AUTH_CALLBACK_REJECTED RED reason=stale_result_authenticated
Source\...\APBWorldGameMode.cpp: snapshot credentials, verify on worker, return via weak ref plus generation nonce for S-W2-8 - verify by TestStaleAuthCallbackRejected RED then GREEN and gate -Only STALE_AUTH_CALLBACK_REJECTED flipping RED to GREEN
Source\...\APBWorldGameMode.cpp: atomically enforce one active session per account on the game thread for S-W7-3 - verify by TestOneSessionPerAccount RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming", "debugging"]`
Dependencies: T01, and T00E (this task adds `STALE_AUTH_CALLBACK_REJECTED`). Parallel with T02B,
T02C. Largest task in the milestone alongside T03B; scope it a
full session.
Commit: `sec(auth): PBKDF2@600k on a bounded executor, stale-callback rejection, account uniqueness (Q3b,N11)`

---

**T02B — N10 + N13 + N2: ticket secret injection and no auto-register**

Files:
- `Source\APBReloaded\Domain\APBTicket.h` / `.cpp` — delete the implicit-random-secret constructor;
  a `TicketService` without an injected secret refuses to sign or verify. Remove `Global()` and
  mutable `SetSecret` from the production path; remove the hidden wall clock (callers supply time).
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` — `LoginPlayer` no longer auto-registers
  an unknown username; it refuses. Read the ticket secret from deployment (env var or secret file)
  and inject it.

Atomic TODOs:
```
Source\...\Domain\APBTicket.cpp: remove implicit random secret, require injection for S-W2-4 - verify by TestNoImplicitSecret RED then GREEN
Source\...\Domain\APBTicket.h: remove Global()/SetSecret and hidden clock for N13 - verify by build exit 0 + HARNESS_EXIT=0
Source\...\APBWorldGameMode.cpp: refuse unknown username in LoginPlayer for S-W2-3 - verify by TestNoAutoRegister RED then GREEN + accounts.json hash identical
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: T01. Parallel with T02A, T02C.
Commit: `sec(ticket,auth): de-singleton TicketService, injected secret, no auto-register (N2,N10,N13)`

---

**T02C — C4: `CanMutateDomain` fail-closed**

Files:
- `Source\APBReloaded\Systems\APBGameInstanceSubsystem.cpp` — `CanMutateDomain()` becomes
  `World && World->GetNetMode() != NM_Client && World->GetAuthGameMode() != nullptr`. Precedent for
  the fail-closed shape already exists in `SocialSvc`.

Atomic TODO:
```
Source\...\APBGameInstanceSubsystem.cpp: CanMutateDomain fail-closed on null world for C4 - verify by TestCanMutateDomainFailsClosed RED then GREEN
```

Delegation: `category="quick"`, `load_skills=["programming"]`
Dependencies: T01. Parallel with T02A, T02B.
Commit: `sec(subsystem): CanMutateDomain fail-closed (Q2)`

---

### Wave 3 — identity and confidentiality (T03A ∥ T03B ∥ T03C; T03C commits before T03B)

**T03A — Q3: `PCKey` -> `uint64 PlayerSessionId`**

Mechanical refactor, no behavior change, lands alone so the behavioral commit that follows is
reviewable.

Files:
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.h` / `.cpp` — `FPlayerSession` gains
  `uint64 PlayerSessionId`. `PlayerServices` is keyed by it instead of
  `PCKey = Printf("%p", PC)`. `TravelReservations.OwnerKey` -> `OwnerSessionId`.
  `AdmittedRoster` **stays character-keyed** — do not change it.

Atomic TODO:
```
Source\...\APBWorldGameMode.cpp: replace PCKey string with uint64 PlayerSessionId for Q3 - verify by build exit 0 + HARNESS_EXIT=0 + run_m6_world_gate WORLD_SERVER_GATE_OK
```

Delegation: `category="ultrabrain"`, `load_skills=["programming", "ast-grep"]`
Dependencies: Wave 2 complete.
Commit: `refactor(world): PCKey to uint64 PlayerSessionId (Q3)`

---

**T03B — Q1 + N1 + N4: typed command RPCs, server-selected identity, atomic mail debit**

One task, not three. D1, N1 and N4 all touch `APBGameInstanceSubsystem.*` and `APBSocial.h`; the
mail-identity fix and the currency-minting fix are the same code path and cannot be split without
leaving an intermediate commit that still mints currency.

Files:
- `Source\APBReloaded\Systems\APBPlayerState.h` / `.cpp` — four typed aggregate RPCs
  (`Server_SubmitClanCommand`, `Server_SubmitFriendCommand`, `Server_SubmitGroupCommand`,
  `Server_SubmitMailCommand`) plus `Server_SelectCharacter(Name)`. `_Validate` bodies check
  **wire-malformed only**. Attach an `apb::RequestLimiter` instance (decision 5) on a **monotonic**
  clock via `FPlatformTime::Seconds`.
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.h` / `.cpp` —
  `RequireAuthenticatedPlayer(APlayerController*) -> FAuthenticatedPlayer`. **Delete public
  `ServiceForCharacter`.** `Server_IssueTicket` resolves the character from `FPlayerSession` and
  **never** creates one.
- `Source\APBReloaded\Systems\APBGameInstanceSubsystem.h` / `.cpp` — all ~30 spoofable entry points
  stop taking an actor name; identity arrives from `FAuthenticatedPlayer`. Strip `BlueprintCallable`
  from every authoritative mutator. All six mail entry points included.
- `Source\APBReloaded\Domain\APBSocial.h` — `MailService::SendMail` debits the sender atomically;
  a send that cannot be funded fails and mutates nothing.
- `Source\APBReloaded\Domain\APBRequestLimiter.h` — **new**, monotonic-ms token bucket.

Atomic TODOs:
```
Source\...\Domain\APBRequestLimiter.h: add monotonic-ms token bucket for decision 5 - verify by TestRequestLimiter RED then GREEN
Source\...\APBWorldGameMode.cpp: add RequireAuthenticatedPlayer and delete public ServiceForCharacter for S-W3-3 - verify by build exit 0
Source\...\APBPlayerState.h: add four typed aggregate command RPCs plus Server_SelectCharacter for S-W3-1 - verify by build exit 0
Source\...\APBGameInstanceSubsystem.cpp: resolve identity from FAuthenticatedPlayer across all six mail entry points for S-W3-2 - verify by TestCrossAccountMailDeleteDenied RED then GREEN + mail.json hash identical
tools\run_m16_zerotrust_gate.ps1: add ONLY the CLAN_ACTOR_SPOOF_DENIED assertion, run it alone for S-W3-3 - verify by gate -Only CLAN_ACTOR_SPOOF_DENIED RED reason=payload_actor_honoured
Source\...\APBGameInstanceSubsystem.cpp: resolve clan/friend/group actor from session for S-W3-3 - verify by TestClanActorSpoofDenied RED then GREEN and gate -Only CLAN_ACTOR_SPOOF_DENIED flipping RED to GREEN
tools\run_m16_zerotrust_gate.ps1: add ONLY the ISSUE_TICKET_DENIED assertion, run it alone for S-W3-4 - verify by gate -Only ISSUE_TICKET_DENIED RED reason=unowned_character_accepted
Source\...\APBWorldGameMode.cpp: Server_IssueTicket must not implicitly create a character for S-W3-4 - verify by TestIssueTicketNoImplicitCreate RED then GREEN and gate -Only ISSUE_TICKET_DENIED flipping RED to GREEN
Source\...\Domain\APBSocial.h: debit sender atomically in SendMail for S-W3-5 - verify by TestMailSendDebitsSender RED then GREEN
Source\...\APBGameInstanceSubsystem.h: strip BlueprintCallable from authoritative mutators for Q1 - verify by build exit 0 + GATE_PASS
```

Delegation: `category="ultrabrain"`, `load_skills=["programming", "ast-grep"]`
Dependencies: T00E (this task adds `CLAN_ACTOR_SPOOF_DENIED` and `ISSUE_TICKET_DENIED`). None
otherwise within Wave 3. T03A is a **preference, not a dependency** (round-1 correction,
re-applied round 2): identity may attach to an opaque session handle without T03A's rekey landing
first. The only binding edge is the commit order **T03C before T03B** — never widen the RPC surface
while private state is still broadcast. This is the largest task in the milestone; scope it a full
session.
Commit: `sec(social,rpc): typed aggregate RPCs, RequireAuthenticatedPlayer, atomic mail debit, mailbox identity (Q1,N1,N4)`

---

**T03C — N15: PlayerState confidentiality**

Blocker 8. The first draft had no defect, task, or scenario for this. Every listed property on
`AAPBPlayerState` currently uses unconditional `DOREPLIFETIME`, and `APlayerState` is relevant to all
clients, so a hacked client reads other players' private state directly off the wire. Runs before or
with T03B — do not widen the RPC surface while private state is still broadcast.

Files:
- `Source\APBReloaded\Systems\APBPlayerState.cpp` `GetLifetimeReplicatedProps` — switch to
  `DOREPLIFETIME_CONDITION(..., COND_OwnerOnly)` for authentication results, `CharListJson`,
  `IssuedTicketJson`, `Cash`, `G1C`, inventory and progression, private mission state, and invite
  state. Keep only deliberately public APB state — faction, display threat — broadly replicated.
- Audit the full property list rather than the six the audit happened to name; treat "is this
  deliberately public in APB?" as the test, defaulting to owner-only.

Atomic TODOs:
```
tools\run_m16_zerotrust_gate.ps1: add ONLY the OWNER_ONLY_REPLICATION_OK assertion, run it alone for S-W3-7 - verify by gate -Only OWNER_ONLY_REPLICATION_OK RED reason=sentinels_visible_to_non_owner
Source\...\APBPlayerState.cpp: switch auth, ticket, char-list, currency, progression, mission and invite properties to COND_OwnerOnly for S-W3-7 - verify by gate -Only OWNER_ONLY_REPLICATION_OK flipping RED to GREEN
Source\...\APBPlayerState.cpp: audit remaining properties and default to owner-only unless deliberately public for N15 - verify by build exit 0 plus two-client sentinel check
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: Wave 2 complete, and T00E (this task adds `OWNER_ONLY_REPLICATION_OK`). Runs with
T03A/T03B, but commits before T03B.
Commit: `sec(replication): owner-only replication for private PlayerState (N15)`

---

### Wave 4 — replay and admission (T04A ∥ T04B)

**T04A — D11 + N3: `VerifyAndConsume`, boot epoch, admission parity**

Serialized with N3 because both touch `APBWorldGameMode.cpp`.

Files:
- `Source\APBReloaded\Domain\APBTicket.h` / `.cpp` — collapse `VerifyTicket` + `ConsumeJti` into
  `VerifyAndConsume` (the current split races). Reject negative or excessive expiry, future issuance,
  and timestamp overflow. Security time = startup UTC + monotonic elapsed; UTC unix seconds on the
  wire for cross-process comparison.
- **N16, blocker 7: the epoch needs a distribution protocol, not just a field.** The first draft said
  "a 128-bit boot epoch per authority process baked into tickets" without defining how a district
  learns which issuer epoch is current. If the district compares against its own epoch, every valid
  world ticket fails; if it accepts any signed epoch, pre-restart tickets survive a world restart.
  Name the field `issuer_world_epoch`. Publish and bind the current world epoch through the
  **authenticated** relay registration / expect-ticket path (which is why T00D precedes this), and
  have districts accept tickets only for that epoch.
- **N16 continued, blocker 7 round 2: the district-restart half was undefined.** The first revision
  specified world restart (S-W4-3) but left district restart as "gates both correct" with no binary
  outcome, and never said what retains the consumption fact when a district's in-memory
  `ReplayWindow` dies with the process. Ruling, consistent with decision 11 (no disk persistence):
  **each district instance also gets a 128-bit boot epoch.** A district publishes its current epoch to
  the world over the same authenticated relay registration path; the world stamps every ticket with
  both `issuer_world_epoch` and `target_district_epoch`; the district refuses any ticket whose
  `target_district_epoch` is not its own with reason `epoch_mismatch`. The consequence is the point:
  after a district restart **every** pre-restart ticket for it is already invalid, consumed or not, so
  losing the replay window cannot be exploited — the window is a defence only *within* one district
  lifetime, and the epoch covers across lifetimes. Nothing is persisted to disk. The accepted cost is
  that a player mid-travel when a district restarts must obtain a fresh ticket; the world reissues on
  the next travel request rather than admitting a stale one.
- `Source\APBReloaded\Domain\APBReplayWindow.h` — **new**. Hard capacity that **rejects when full**;
  no disk persistence; retain a jti only until its expiry.
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` — `MarkRelayPlayerJoined` marks admitted
  **after** reservation and JTI binding, not before. `ConsumedReturnNonces` gets the same bounded
  treatment.

Atomic TODOs:
```
Source\...\Domain\APBReplayWindow.h: add bounded replay window that rejects when full for S-W4-4 - verify by TestReplayWindowRejectsWhenFull RED then GREEN
Source\...\Domain\APBTicket.cpp: collapse VerifyTicket and ConsumeJti into VerifyAndConsume for S-W4-1 - verify by TestVerifyAndConsumeAtomic RED then GREEN
Source\...\Domain\APBTicket.cpp: refuse a second VerifyAndConsume of the same jti for S-W4-2 - verify by TestReplayRefused RED then GREEN
Source\...\Domain\APBTicket.cpp: add issuer_world_epoch to tickets for S-W4-3 - verify by TestStaleBootEpochRefused RED then GREEN
Source\...\APBServerControl.cpp: publish the current world epoch over authenticated relay registration for N16 - verify by TestEpochDistribution RED then GREEN
Source\...\Domain\APBTicket.cpp: add target_district_epoch to tickets for S-W4-5 - verify by TestStaleDistrictEpochRefused RED then GREEN on both consumed and unconsumed pre-restart tickets
tools\run_m16_zerotrust_gate.ps1: add ONLY the DISTRICT_EPOCH_RESTART_REFUSED assertion, run it alone for S-W4-5 - verify by gate -Only DISTRICT_EPOCH_RESTART_REFUSED RED reason=stale_district_epoch_admitted
Source\...\APBServerControl.cpp + APBDistrictGameMode.cpp: publish each district boot epoch over authenticated registration AND compare it against the running district epoch for S-W4-5 - verify by gate -Only DISTRICT_EPOCH_RESTART_REFUSED flipping RED to GREEN (distribution and enforcement are one change; neither is provable alone)
Source\...\APBDistrictGameMode.cpp: refuse tickets whose issuer_world_epoch or target_district_epoch is not current, reason epoch_mismatch for S-W4-3/S-W4-5 - verify by TestStaleBootEpochRefused and TestStaleDistrictEpochRefused GREEN, then the two-process restart run refuses consumed T and unconsumed U and admits a fresh ticket
Source\...\Domain\APBTicket.cpp: reject bad expiry, future issuance, timestamp overflow - verify by TestTicketTimeBounds RED then GREEN
Source\...\APBWorldGameMode.cpp: bind reservation and JTI before marking admitted for N3 - verify by TestAdmissionOrderedAfterBinding RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: Wave 3 complete, and T00E (this task adds `DISTRICT_EPOCH_RESTART_REFUSED`).
Commit: `sec(ticket,admission): VerifyAndConsume, ReplayWindow boot epoch, admission parity (D11,N3)`

---

**T04B — N5: vehicle possession range**

Files:
- `Source\APBReloaded\Systems\APBFreeroamCharacter.cpp` — `ServerEnterNearestVehicle` caps the
  search radius at 500 units (down from 5000) and re-validates server-side distance at possession
  time; add the missing `_Validate` (wire-malformed only).

Atomic TODO:
```
Source\...\APBFreeroamCharacter.cpp: cap ServerEnterNearestVehicle to 500 units and validate server-side for N5 - verify by TestVehicleRangeCapped RED then GREEN
```

Delegation: `category="quick"`, `load_skills=["programming"]`
Dependencies: none. T04A is a **preference, not a dependency** (round-1 correction, re-applied
round 2): T04A and T04B share no file, so T04B may land in either order once Wave 0-3 prerequisites
are in.
Commit: `sec(vehicle): cap EnterNearestVehicle to 500 units (N5)`

---

### Wave 5 — persistence identity, then integrity (T05A -> T05B, strictly serialized)

**T05A — D12: account-id filenames**

Files:
- `Source\APBReloaded\Domain\APBPersistence.h` / `.cpp` — accounts get a stable random 128-bit id
  rendered `a_<32 lowercase hex>`. Username becomes indexed data only, never a path component.
  Update every path builder. `Sanitize()` stops being load-bearing for identity.
- Migration: explicit, operator-driven, idempotent journal, **HALT on collision**, **no** runtime
  legacy fallback.

Atomic TODOs:
```
Source\...\Domain\APBPersistence.cpp: derive account paths from a_<hex> account id for D12 - verify by TestAccountIdPaths RED then GREEN
Source\...\Domain\APBPersistence.cpp: add idempotent migration journal that halts on collision for S-W5-3 - verify by TestMigrationHaltsOnCollision RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: Wave 4 complete.
Commit: `sec(persist): account-id filenames, migration journal (Q6b)`

---

**T05B — D7: save MAC envelope**

Must follow T05A: the MAC's `logical_doc_name` embeds the account id.

Files:
- `Source\APBReloaded\Domain\APBPersistence.h` / `.cpp` — envelope
  `{format, version, key_id, data, mac}` across all **seven** targets: `accounts.json`,
  `characters/<a>_<slot>.json`, `..._progress.json`, `..._handoff.json`, `auction.json`,
  `mail.json`, and `social/mail_claims.json` (that last one lives outside `JsonDomainStore` — do not
  miss it). HMAC over the exact `data` bytes. Domain separation
  `"APB-SAVE\0" "v1\0" <logical_doc_name> "\0" <data>`. Per-purpose derived keys, `key_id` keyring,
  signed provisioning marker.
- Key injection is UE-side: read env var or secret file, pass a ≥32-byte key into Domain. Domain
  never reads the environment.
- Unify the write path: `APBPersistence.cpp` `WriteFile` uses rename while
  `APBMailClaimJournal.cpp` uses remove-then-rename, which opens a missing-file window. Standardize
  on atomic rename.

Atomic TODOs:
```
Source\...\Domain\APBPersistence.cpp: add MAC envelope with domain separation across all 7 targets for S-W5-1 - verify by TestSaveMacDetectsEdit RED then GREEN
Source\...\Domain\APBPersistence.cpp: bind logical_doc_name into the MAC for S-W5-2 - verify by TestMacDomainSeparation RED then GREEN
Source\...\Domain\APBMailClaimJournal.cpp: standardize on atomic rename to close the missing-file window - verify by TestJournalWriteAtomic RED then GREEN
Source\...\APBWorldGameMode.cpp: inject the deployment save key into Domain for D7 - verify by build exit 0 + GATE_PASS
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: T05A.
Commit: `sec(persist): save MAC envelope, domain separation, key injection (Q1b)`

---

### Wave 6 — loader strictness and clock authority (T06A ∥ T06B)

**T06A — D8: strict whole-document rejection**

Files:
- `Source\APBReloaded\Domain\APBPersistence.cpp` — `LoadAccounts`, `LoadCharacter`, `LoadProgress`
  and the remaining loaders reject any document with unknown top-level keys, missing required keys,
  or wrong value types. No partial-load fallback; an empty array is valid, one bad record is not.
  Parse to temp -> validate the whole document -> atomic commit. Typed result
  `{Missing, Valid, IntegrityFailure, SchemaFailure, IoFailure}`.
- Delete `LoadHandoffSnapshot` (no caller, accepts any non-empty payload).

Atomic TODOs:
```
Source\...\Domain\APBPersistence.cpp: strict whole-doc rejection with typed result for S-W6-1 - verify by TestLoaderRejectsWholeDoc RED then GREEN
Source\...\Domain\APBPersistence.cpp: delete dead LoadHandoffSnapshot for D8 - verify by build exit 0 + HARNESS_EXIT=0
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: T05B. Parallel with T06B. (T06C is withdrawn — see below.)
Commit: `sec(persist): strict loader whole-doc rejection (D8)`

---

**T06B — D6: server-authoritative clock**

Files:
- `Source\APBReloaded\Domain\APBWorldService.h` / `.cpp` — `TickMission` stops taking a
  client-supplied `NowSec`; elapsed time is derived server-side from a per-mission start timestamp.
  `AdvanceOpposition` becomes server-derived; no client-triggered amount.
- `Source\APBReloaded\Systems\APBPlayerState.cpp` — remove the client timestamp from the RPC
  surface.
- Numeric bounds for the remaining sinks (`AuctionListItem`, `FireCatalogWeapon`, `AddSymbolLayer`,
  `EquipClothingColored`, `ApplyBodyProfile`, `SocialClanAddRank`, `SocialClanSetMemberRank`) split
  across `_Validate` / dispatcher / Domain per decision 6.

Atomic TODOs:
```
Source\...\Domain\APBWorldService.cpp: derive mission elapsed time server-side for S-W6-2 - verify by TestMissionClockServerAuthoritative RED then GREEN
Source\...\Domain\APBWorldService.cpp: server-derive AdvanceOpposition amount for D6 - verify by TestOppositionServerDerived RED then GREEN
Source\...\APBPlayerState.cpp: remove client timestamp from TickMission RPC for S-W6-2 - verify by build exit 0
Source\...\APBGameInstanceSubsystem.cpp: bound remaining numeric sinks per decision 6 - verify by TestNumericBounds RED then GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming"]`
Dependencies: T05B. Parallel with T06A. (T06C is withdrawn — see below.)
Commit: `sec(mission): server-authoritative mission and opposition clock (D6)`

---

**T06C — withdrawn (merged into T02A)**

The first draft shipped PBKDF2 at 600k in commit 4 and moved it off the game thread in commit 15.
Blocker 2: that leaves eleven commits during which a malicious client has a **cheaper** game-thread
DoS than against today's 10,000-iteration code. Blocker 3: dispatching auth to a worker without
one-session enforcement in the same commit opens a stale-callback / session-fixation window. Both now
live in T02A, which is indivisible for exactly this reason. This heading is retained so the task
numbering in review round 1 stays traceable.

---

### Wave 7 — frontend loopback and persistence authority (T07A -> T07B)

**T07A — D13: frontend loopback shim**

Files:
- `Source\APBReloaded\Systems\APBPlayerState.h` / `.cpp` — add `Server_RegisterRequest`, mirroring
  the existing `Server_LoginRequest` path into `WorldService.RegisterAccount`.
- `Source\APBReloaded\Systems\Frontend\APBFrontendWidget.cpp` — `OnRegisterClicked` drops the
  in-process `UAPBGameInstanceSubsystem::RegisterAccount` call and routes through
  `Server_RegisterRequest`. The existing `bWorldServerMode` branch in `OnLoginClicked` is **already
  correct** — leave it, but make the branch mandatory rather than conditional. `NativeConstruct`
  stops seeding a QA account unconditionally: gate it behind `WITH_EDITOR` or an explicit
  `-apbseedqa` command-line opt-in.

Atomic TODOs:
```
tools\run_m16_zerotrust_gate.ps1: add ONLY the SERVER_REGISTER_ROUTED assertion, run it alone for S-W7-1 - verify by gate -Only SERVER_REGISTER_ROUTED RED reason=register_not_routed
Source\...\APBPlayerState.h + APBFrontendWidget.cpp: add the Server_RegisterRequest RPC AND route OnRegisterClicked through it for S-W7-1 - verify by gate -Only SERVER_REGISTER_ROUTED flipping RED to GREEN, then run_m6_world_gate WORLD_SERVER_GATE_OK + accounts.json gains the account (one step: declaring the RPC without routing the caller leaves the assertion RED, so the declaration cannot carry its GREEN)
tools\run_m16_zerotrust_gate.ps1: add ONLY the NATIVECONSTRUCT_SEED_GATED grep-assert, run it alone for S-W7-2 - verify by gate -Only NATIVECONSTRUCT_SEED_GATED RED reason=unconditional_qa_seed
Source\...\APBFrontendWidget.cpp: gate the NativeConstruct QA seed behind WITH_EDITOR or -apbseedqa for S-W7-2 - verify by gate -Only NATIVECONSTRUCT_SEED_GATED flipping RED to GREEN, then launching with no flags and confirming no player1 in accounts.json
```

Delegation: `category="unspecified-high"`, `load_skills=["programming"]`
Dependencies: Wave 6 complete, and T00E (this task adds `SERVER_REGISTER_ROUTED` and
`NATIVECONSTRUCT_SEED_GATED`). Parallel with T07B.
Commit: `sec(frontend): Server_RegisterRequest loopback, gate QA seed (D13)`

---

**T07B — N11: process-owned persistence authority**

Blocker 4 rejected the first draft's claim that one-session-per-account fixes N11. It does not. It
prevents same-account character collisions only. Every per-connection `WorldService` initialises the
same persistence root, so **different accounts** still independently load, modify, and overwrite the
global documents; `SocialAuthority` and the temporary return-path `WorldService` are further
independent writers. Account uniqueness itself moved into T02A, where it belongs with the async auth
handoff. What remains here is the actual linearizability fix.

Files:
- `Source\APBReloaded\Domain\APBPersistenceAuthority.h` / `.cpp` — **new**. One process-owned
  authority for the global documents: `accounts.json`, `auction.json`, `mail.json`,
  `social/mail_claims.json`, and handoff-return persistence. One authoritative in-memory owner, one
  serialized write path, all mutations expressed as read-modify-write operations against that owner
  rather than against a freshly loaded copy.
- `Source\APBReloaded\Systems\Server\APBWorldGameMode.cpp` — per-connection `WorldService` instances
  keep isolated **character** state but route every global-document mutation through the authority.
  Remove the independent persistence-root initialisation at the per-connection construction site.
- `SocialAuthority` and the temporary return-path `WorldService` route through the same authority.

Atomic TODOs:
```
Source\...\Domain\APBPersistenceAuthority.cpp: add a single owner and serialized write path for the global documents for S-W7-4 - verify by TestConcurrentDifferentAccountWrites RED then GREEN
Source\...\APBWorldGameMode.cpp: route all global-document mutations through the authority for S-W7-4 - verify by TestNoIndependentGlobalWriters RED then GREEN
Source\...\APBWorldGameMode.cpp: remove per-connection persistence-root initialisation for N11 - verify by build exit 0 plus HARNESS_EXIT=0
tools\run_m16_zerotrust_gate.ps1: add ONLY the ONE_SESSION_KICK_DENIES_MUTATION assertion, run it alone for S-W7-3 - verify by gate -Only ONE_SESSION_KICK_DENIES_MUTATION RED reason=kicked_session_still_mutates
Source\...\APBWorldGameMode.cpp: strip mutation authority from the kicked session, not just its connection for S-W7-3 - verify by gate -Only ONE_SESSION_KICK_DENIES_MUTATION flipping RED to GREEN
```

Delegation: `category="ultrabrain"`, `load_skills=["programming", "debugging"]`
Dependencies: T07A, T02A for account uniqueness, and T00E (this task adds
`ONE_SESSION_KICK_DENIES_MUTATION`).
Commit: `sec(persist): process-owned persistence authority for global documents (N11)`

---

### Wave 8 — gate wiring, exe20, roadmap (T08)

**T08 — gate wiring, exe20 close-out, roadmap record**

**Round-2 correction:** this task no longer *creates* the gate script. T00E created the harness as the
milestone's first commit and each owning task added its own assertion RED-first, because a gate
authored here would arrive after every task it is supposed to hold to account. T08 wires the finished
gate into the spine and closes out.

Files:
- `tools\run_m16_zerotrust_gate.ps1` — **already exists** (T00E harness + per-task assertions). Here:
  append the M6 and M7 gate runs, confirm the registry holds all ten assertions from the T00E
  ownership table, and confirm teardown still reports `LEAKED=0`. Do **not** re-create the file.
- `tools\run_verification_gates.ps1` — wire the new gate into the spine after `m16_persistence`.
  Summary key `m16_zerotrust`, lower_snake_case.
- `tests\run_zerotrust_tests.cpp` — the `$exe20` `APBZeroTrustTests` source. It is **created in T01**
  (that is where `bcrypt.lib` and the first CSPRNG test land); T08 only closes it out. Nine domain
  suites, and only these — every one is pure Domain logic:
  `TestSecretMaterialRejected`, `TestSecureRandomDistinct`, `TestEntropyFailureFailsClosed`,
  `TestPBKDF2Vector600k`, `TestPlaintextBranchDeleted`, `TestAuthQueueRejectsWhenFull`,
  `TestAuthRateLimited`, `TestStaleAuthCallbackRejected`, `TestReplayWindowRejectsWhenFull`.
  **Correction (self-found, round 1):** the first draft put the save-MAC envelope, loader
  whole-document rejection, and account-id migration collision into `$exe20`. The scenario contract
  assigns all three to `run_persistence_tests.cpp` (S-W5-1, S-W5-2, S-W5-3, S-W6-1), which is where
  they belong and where T05B/T06A already write them. Do not duplicate them here.
- `tools\run_m16_zerotrust_gate.ps1` owns every **pure-UE** assertion, because the harness
  structurally cannot. The ten assertions and their owning tasks are fixed by the T00E ownership
  table. Each was authored by its owning task and observed RED **before** that task's production edit.
  T08 only verifies the registry is complete — it authors no assertion.
- `work\_active.md` M16 section — extend in place. Record every task id, the two Oracle session ids
  and date, the N11 decision, and that **N12 is resolved by T00C rather than carried as a
  limitation** — the first draft told this task to record a limitation that no longer exists.

Atomic TODOs:
```
tools\run_m16_zerotrust_gate.ps1: append the M6/M7 gate runs and assert the registry holds all 10 T00E-table assertions for S-W8-1 - verify by exit 0 with M16_ZEROTRUST_GATE_OK and LEAKED=0
tools\run_verification_gates.ps1: wire m16_zerotrust into the gate spine for S-W8-1 - verify by GATE_PASS
tests\run_zerotrust_tests.cpp: close out the 9 domain exe20 suites for S-W8-2 - verify by HARNESS_EXIT=0 with FAILS=0 across 20 suites
work\_active.md: extend the M16 section with task ids, Oracle rulings, N11 decision, N12 resolved by T00C - verify by human review
```

Delegation: `category="unspecified-high"`, `load_skills=["programming"]`
Dependencies: T00E (harness exists), T07A, T07B.
Commit: `chore(m16): zerotrust gate wiring, exe20, update active plan (T08)`

## Commit strategy

One atomic commit per verified increment, in this order. The orchestrator commits; implementers do
not.

Revised after review round 1: 18 commits became 20. T00C and T00D are new, T03C is new, T02A absorbed
the old commits 4 and 15, T06C is withdrawn, and T07B changed meaning. Revised again after review
round 2: **21 commits**, numbered from 0. T00E's gate-harness scaffold is now commit 0 — the gate must
exist before the tasks whose RED evidence it carries. The existing 1-20 numbering is unchanged so the
constraint text below still reads true. Commit order follows wave order; within a wave, parallel tasks
commit in whatever order they finish **except** where a forced ordering below says otherwise.

| # | Wave | Task | Message |
|---|---|---|---|
| 0 | 0 | T00E | `chore(m16): zerotrust gate harness scaffold with teardown receipts` |
| 1 | 0 | T00A | `sec(secrets): deployment secret provider, drop committed TicketSecret, RequireTicket default on` |
| 2 | 0 | T00B | `sec(ticket): harden json_int/b64url/hex_decode/payload-escape (N6-N9)` |
| 3 | 0 | T00C | `sec(transport): AES-GCM packet encryption, refuse auth on plaintext (N12)` |
| 4 | 0 | T00D | `sec(relay): per-frame HMAC, connection-bound district identity, caps (N14)` |
| 5 | 1 | T01 | `sec(crypto): BCryptGenRandom CSPRNG, no PRNG fallback (Q4b)` |
| 6 | 2 | T02A | `sec(auth): PBKDF2@600k on a bounded executor, stale-callback rejection, account uniqueness (Q3b,N11)` |
| 7 | 2 | T02B | `sec(ticket,auth): de-singleton TicketService, injected secret, no auto-register (N2,N10,N13)` |
| 8 | 2 | T02C | `sec(subsystem): CanMutateDomain fail-closed (Q2)` |
| 9 | 3 | T03A | `refactor(world): PCKey to uint64 PlayerSessionId (Q3)` |
| 10 | 3 | T03C | `sec(replication): owner-only replication for private PlayerState (N15)` |
| 11 | 3 | T03B | `sec(social,rpc): typed aggregate RPCs, RequireAuthenticatedPlayer, atomic mail debit, mailbox identity (Q1,N1,N4)` |
| 12 | 4 | T04A | `sec(ticket,admission): VerifyAndConsume, ReplayWindow boot epoch, admission parity (D11,N3)` |
| 13 | 4 | T04B | `sec(vehicle): cap EnterNearestVehicle to 500 units (N5)` |
| 14 | 5 | T05A | `sec(persist): account-id filenames, migration journal (Q6b)` |
| 15 | 5 | T05B | `sec(persist): save MAC envelope, domain separation, key injection (Q1b)` |
| 16 | 6 | T06A | `sec(persist): strict loader whole-doc rejection (D8)` |
| 17 | 6 | T06B | `sec(mission): server-authoritative mission and opposition clock (D6)` |
| 18 | 7 | T07A | `sec(frontend): Server_RegisterRequest loopback, gate QA seed (D13)` |
| 19 | 7 | T07B | `sec(persist): process-owned persistence authority for global documents (N11)` |
| 20 | 8 | T08 | `chore(m16): zerotrust gate wiring, exe20, update active plan (T08)` |

**Commit-order constraints that are not merely wave order:**
- **Commit 0 is first, unconditionally.** The gate harness precedes every task that adds an assertion
  to it: commits 1, 3, 6, 10, 11, 18 and 19. Without it those tasks cannot capture the RED the
  scenario contract requires, and the gate degrades into after-the-fact validation.
- **Commit 1 is indivisible.** Removing the committed `TicketSecret`, adding the provider, and
  converting every consumer land together. There is no valid intermediate state where a role reads a
  secret that no longer exists. Never restore the placeholder or set `RequireTicket=False` to keep a
  gate green in this window.
- **Commit 6 is indivisible.** Iteration count, bounded executor, auth limiter, and account
  uniqueness are one commit. This is blockers 2 and 3; splitting it reintroduces either a cheaper
  game-thread DoS or a session-fixation window.
- **Commit 10 precedes commit 11.** Owner-only replication lands before the RPC surface widens, so
  private state is never broadcast while new command paths are being added.
- **Commit 15 follows commit 14.** The MAC's `logical_doc_name` embeds the account id.

**Withdrawn from the first draft:** the old commit 15 (`perf(auth): PBKDF2@600k off game thread`) no
longer exists as a separate commit — it is inside commit 6. The old commit 17
(`sec(auth): one active session per account`) split: uniqueness went to commit 6, linearizability
became commit 19.

Each commit requires: RED captured, GREEN captured, build exit 0, `FAILS=0` on affected suites.
Study the existing history before writing each message (`git log --oneline -20` plus
`git log -5 -- <touched paths>`) and match its shape.

Also outstanding and separately authorized: the M14 S10 atomic commits along the boundaries in
`work\m14_s10_claim_plan.md` (mail fail-closed; journal + WorldService wiring; handoff reconcile;
UE bridge + `UnsupportedAttachment`; `SocialMailSend` sender). Stage only S10 files. Never
`git add .`.

## Per-task TDD loop

1. Write the failing test in the appropriate `tests\run_*.cpp`. Run
   `powershell -File D:\APBReloaded\tests\build_and_run.ps1`. Confirm it fails **for the right
   reason** — assertion message, not a syntax or include error. Capture the transcript.
2. Implement the smallest change that flips RED to GREEN. Re-run. Capture.
3. Exercise the real surface named in the scenario. Capture the artifact.
4. Re-run the affected gates to confirm the regression scenarios still hold.
5. Record both evidence paths before marking the todo complete.

Build gate (the type gate, since clangd is absent):

```powershell
& D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat APBReloadedEditor Win64 Development `
  -Project=D:\APBReloaded\APBReloaded.uproject -WaitMutex
```

## Out of scope, documented rather than silently omitted

**N12 transport encryption was removed from this table in review round 1.** The entry claimed UE5
ships no usable transport encryption and that adding it required a source engine build. That premise
was false and had never been verified — UE 5.8 ships `AESGCMHandlerComponent` as a runtime plugin and
`BaseEngine.ini:3575` already exposes it. The entry was rationalising an unfixed P0, which is exactly
what this table must not be used for. N12 is now in scope as T00C. Every remaining row below states a
limit that was checked, not assumed.

| Item | Reason |
|---|---|
| RFC 8785 JCS canonicalization | Domain separation on `logical_doc_name` achieves the MAC binding without a canonicalizer. Oracle cut. |
| Hand-vendored Argon2 | PBKDF2 at 600k iterations meets current OWASP guidance. Argon2 would need a vetted C library, not a hand port. Oracle cut. |
| Disk-persisted replay caches | The in-memory `ReplayWindow` plus boot epoch covers any realistic session window. Oracle cut. |
| `TargetType.Server` build | Installed binary engine limitation; see `work\m6_server_target_limit.md`. |
| Forced password re-salt | Salts need uniqueness, not secrecy. Opportunistic rehash on login is sufficient. Oracle correction. |
| Removing the leaked `AppendPeerObserve` temp path | Not caused by this milestone. Flagged, not deleted. Needs its own authorization. |
| M14 S10 item grants | RED by explicit decision. Item mail is refused and preserved via `UnsupportedAttachment`. |
