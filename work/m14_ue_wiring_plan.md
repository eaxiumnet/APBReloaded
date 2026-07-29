# M14 — Social (Clans / Friends / Groups / Mail) UE-Side Wiring Plan

**Status:** APPROVED FOR EXECUTION · decision-complete · TDD-first · wave-ordered
**Author:** hyperplan (adversarial 4-critic team) + orchestrator ground-truth verification
**Baseline:** HEAD `a90f379`; M7–M16 tree is uncommitted WIP in a SHARED worktree.
**Scope:** UE-side wiring + verification ONLY. Domain layer is DONE and tested
(`tests/build_and_run.ps1` = all suites FAILS=0). Do NOT modify Domain service logic.

---

## 0. VERIFIED GROUND TRUTH (read directly, not assumed)

- `apb::WorldService` owns `SocialService social` (L122, LEGACY STUB — never use for M14)
  and `MailService mail` (L123). It does NOT own Group/Clan/Friends/SocialStore.
- `WorldService::InitPersistence(dir)` (L188) and `SaveAllNow()` (L191) are the persistence seams.
- **DECIDING FACT:** `AAPBWorldGameMode` holds `TMap<FString,TUniquePtr<FAPBPlayerService>> PlayerServices`
  (`APBWorldGameMode.h:43`); `FAPBPlayerService{ TUniquePtr<apb::WorldService> Service; }` (h:10-12);
  `.cpp:62` allocates `MakeUnique<apb::WorldService>()` **PER CONNECTION KEY**.
  => On the M6 world-server there is ONE `apb::WorldService` **per connection**.
  The process-global `UAPBGameInstanceSubsystem::Service` is a SEPARATE, effectively dormant
  instance on the dedicated server (login/char/ticket all run via `ServiceFor(PC)->Service`).
- Relay presence handlers ALREADY exist on the world GameMode: `MarkRelayPlayerJoined` /
  `MarkRelayPlayerLeft` (h:68-69) + `CharacterDistricts` map (h:62). They currently do NOT
  call any FriendsService.
- `ChatService::SetGroup`/`SetClan` have ZERO callers today; each district owns its own
  `ChatService` (`APBDistrictGameMode.h:78`).
- `UAPBGameInstanceSubsystem`: `FAPBDomainSnapshotUE` at h:28-51 (new structs land beside it),
  `void* Service` h:227, `CanMutateDomain()` h:222 (returns false on `NM_Client`).
- Domain-side unit suites already pass: `run_clan_tests`, `run_group_tests`, `run_friend_tests`,
  `run_social_store_tests`. `Source/APBReloaded/Tests` is empty — no UE Automation harness.
  UE-side proof surface = the `-APBProbe=social_probe` session probe + 2-client gate.

---

## 1. ARCHITECTURE FORK — DEFINITIVE VERDICT

The original framing ("Option A: services as `WorldService` members" vs "Option B: in the
subsystem") was the WRONG axis. Adversarial review + direct code read resolved it as follows.

### 1a. Service TYPE placement → inside `apb::WorldService` (facade-respecting)
Add `ClanService clans; FriendsService friends_svc; GroupService groups; SocialStore social_store;`
as members of `apb::WorldService`, mirroring the existing `mail` member. This respects the
Domain `AGENTS.md` contract ("Keep `WorldService` as the facade… UE code does not reach through
the facade into service internals") and lets UE reach them via the existing `Svc(Service)->clans`.
The legacy `SocialService social` stub STAYS untouched and unused.

### 1b. Authoritative INSTANCE → ONE shared authority owned by `AAPBWorldGameMode` (the crux)
Cross-player social state must NOT live in the per-connection `PlayerServices` instances.
If it did: Alice creates a clan in *her* `WorldService`; Bob's per-connection copy never sees it;
and each connection's `SaveClans()` clobbers the whole `clans.json` from a partial view (split-brain
+ lost-update). Therefore the world GameMode owns ONE dedicated authority instance
(`apb::WorldService SocialAuthority;` member on `AAPBWorldGameMode`, persistence-active).
ALL social mutations + presence route to THAT single instance. Per-connection `PlayerServices`
remain ONLY for per-player login/economy/character/ticket.

Rationale for GameMode (not the GI subsystem): relay presence (`MarkRelayPlayerJoined/Left`) and
per-connection routing already live on `AAPBWorldGameMode`; it is the true server-process singleton.
This is the architecture-critic's "process-global authority + world→district projection", re-homed
to where the presence + routing seams already are.

### 1c. Client authority → NEW validated Server RPCs are a HARD PREREQUISITE
`CanMutateDomain()` is false on `NM_Client` and `AAPBPlayerState` has ZERO social RPCs. Clients
CANNOT mutate social state today. Add `Server, Reliable, WithValidation` RPCs on `AAPBPlayerState`
that dispatch to `AAPBWorldGameMode`'s single `SocialAuthority`. Without these the 2-client gate is
impossible and a naive standalone probe passes on dead, single-process state (false green).

### 1d. Read path → replicated `AAPBPlayerState` fields (never client-local queries)
The in-district `UAPBSocialWidget` reads social state via REPLICATION, not client-local
`OnlineFriendsOf` (which returns 0 on a client). Add replicated fields pushed world→PlayerState:
`ClanId, ClanRole, GroupId, OnlineFriendCount, bHasPendingClanInvite, bHasPendingGroupInvite,
bGroupAllReady`. Bob's probe gates its Accept on a replicated pending-invite field, not a timer.

### 1e. Chat seam → district GameMode concern
`APBFreeroamGameMode` reads `clans.ClanOf`/`groups.GroupOf` from the authority and calls its OWN
`ChatService::SetClan/SetGroup` on PostLogin AND after each membership mutation. Never wire this at
the subsystem level (the subsystem holds no district `ChatService`).

---

## 2. WAVE-ORDERED ATOMIC TASK LIST

Task format: `path: <action> for <scenario-id> — verify by <check>`.
TDD-first: the probe RED skeleton (W1) exists and FAILS before any bridge is implemented.
One concern per commit; match existing git log style (`m14: …` / `feat(M14): …` — check
`git log --oneline -20` before first commit and mirror the dominant shape).

### WAVE 0 — Domain members + shared authority (sequential T01 → {T02,T03,T04})
Blocks all UE work. No UE code until these compile + Domain harness stays green.

- **T01** `Source/APBReloaded/Domain/APBWorldService.h`: add `#include "APBGroup.h","APBClan.h","APBFriends.h","APBSocialStore.h"` (after APBSocial.h L50, BEFORE APBPersistence.h L51 — this order avoids a header cycle since APBSocialStore.h includes APBClan.h+APBFriends.h) and declare members `ClanService clans; FriendsService friends_svc; GroupService groups; SocialStore social_store;` in `WorldService` for S-OWNER — verify by `tests/build_and_run.ps1` FAILS=0 AND `Build.bat APBReloadedEditor` exit 0.
- **T02** `Source/APBReloaded/Domain/APBWorldService.cpp`: in `InitPersistence()`, after `store.Init(dir)`, add `social_store.Init(dir + "/social"); if(social_store.IsActive()){ social_store.LoadClans(clans); social_store.LoadFriends(friends_svc); }` for S-PERSIST-INIT — verify by `run_social_store_tests` FAILS=0; `<SavedDir>/DomainDB/social/` created on first init.
- **T03** `Source/APBReloaded/Domain/APBWorldService.cpp`: in `SaveAllNow()`, after `PersistMail()`, add `if(social_store.IsActive()){ social_store.SaveClans(clans); social_store.SaveFriends(friends_svc); }` for S-PERSIST-SAVE — verify by Domain harness FAILS=0; clans.json+friends.json written.
- **T04** `Source/APBReloaded/Systems/Server/APBWorldGameMode.h+.cpp`: add member `apb::WorldService SocialAuthority;` (persistence-active via `InitPersistence(PersistDir)` in `BeginPlay`), and a private accessor `apb::WorldService& Social()`; for S-AUTHORITY — verify by `Build.bat APBReloadedEditor` exit 0. NOTE: this is the SINGLE cross-player social authority; per-connection `PlayerServices` are untouched.

### WAVE 1 — UE structs + replicated fields + probe RED (parallel; TDD floor)

- **T05** `Source/APBReloaded/Systems/APBGameInstanceSubsystem.h`: add `USTRUCT(BlueprintType)` `FAPBFriendEntryUE{Name,bOnline}`, `FAPBClanInfoUE{Id,Name,Tag,Motd,LeaderName,TArray<FString>Members}`, `FAPBMailMessageUE{Id,From,Subject,Body,bRead,bClaimed,Cash}`, `FAPBGroupInfoUE{Id,Leader,MissionId,bAllReady,TArray<FString>Members}` beside `FAPBDomainSnapshotUE` for S-STRUCTS — verify by editor build exit 0.
- **T06** `Source/APBReloaded/Systems/APBPlayerState.h+.cpp`: add replicated `UPROPERTY(ReplicatedUsing/Replicated)` fields `ClanId, ClanRole, GroupId, OnlineFriendCount, bHasPendingClanInvite, bHasPendingGroupInvite, bGroupAllReady` + `GetLifetimeReplicatedProps` entries for S-REPL — verify by editor build exit 0; lsp clean.
- **T07** `Source/APBReloaded/Systems/APBSessionProbeSubsystem.h`: declare `void RunSocialProbe();` + role/state flags (`SocialRole`, `bSocialClanOk,bSocialFriendsOk,bSocialGroupsOk,bSocialMailOk`) for S-PROBE-DECL — verify by editor build exit 0.
- **T08** `Source/APBReloaded/Systems/APBSessionProbeSubsystem.cpp`: parse `-APBProbe=social_probe` in `StartProbe` (~L112) + `-SocialRole=` ; arm in `ArmProbeTimers` (~L262); implement `RunSocialProbe()` as a STUB that emits `UE_LOG … SOCIAL_PROBE_FAIL reason=not_implemented` and terminates, for S-PROBE-RED — verify by `-APBProbe=social_probe` writing `SOCIAL_PROBE_FAIL` to the scratch log (RED — the failing test that motivates W2-W5).

### WAVE 2 — UGI bridge UFUNCTIONs (parallel T09..T12; depend on T01+T05)
All mutations begin `if(!Service||!CanMutateDomain()) return false;`; reads guard `if(!Service)`.
On the SERVER these run against the world GameMode's `SocialAuthority` (bridge resolves the
authority via `GetWorld()->GetAuthGameMode<AAPBWorldGameMode>()` when present, else `Svc(Service)`).

- **T09** `APBGameInstanceSubsystem.h+.cpp`: 11 clan mutation UFUNCTIONs (Create/Invite/AcceptInvite/DeclineInvite/Kick/SetMotd/AddRank/SetMemberRank/Leave/Disband/TransferLeader) + `FAPBClanInfoUE GetClanInfo(FString) const`; clan `Invite` derives invitee faction SERVER-SIDE (never trust client arg) for S-CLAN-BRIDGE — verify by editor build exit 0.
- **T10** `APBGameInstanceSubsystem.h+.cpp`: 6 friend mutations (Request/Accept/Decline/Remove/Ignore/Unignore) + 5 queries (AreFriends/IsIgnoring/GetFriendList/GetIncomingRequests/GetIgnoreList) for S-FRIENDS-BRIDGE — verify by editor build exit 0.
- **T11** `APBGameInstanceSubsystem.h+.cpp`: 9 group ops (Create/Invite/Accept/Leave/Kick/Disband/TransferLeader/SetReady/AssignMission) + `FAPBGroupInfoUE GetGroupInfo(FString) const` for S-GROUP-BRIDGE — verify by editor build exit 0.
- **T12** `APBGameInstanceSubsystem.h+.cpp`: mail (Send/GetInbox→TArray<FAPBMailMessageUE>/GetUnreadCount/MarkRead/ClaimAttachments/Delete); Claim+Delete verify `message.to == authenticated character` and return a TYPED result enum (Ok/NotOwner/NotFound/AlreadyClaimed/Unclaimed/GrantFailed) for S-MAIL-BRIDGE — verify by editor build exit 0.

### WAVE 3 — Server RPCs + relay presence (depend on W2)
- **T13** `Source/APBReloaded/Systems/APBPlayerState.h+.cpp`: `Server_SocialClan(Op,Arg1,Arg2)`, `Server_SocialFriend(Op,Target)`, `Server_SocialGroup(Op,Arg1,Arg2)`, `Server_SocialMail(Op,Arg1)` as `Server,Reliable,WithValidation`; validate non-empty + identity-bind to owning character; dispatch to `AAPBWorldGameMode::SocialAuthority`; push refreshed replicated fields for S-RPC/S6 — verify by editor build exit 0; lsp clean.
- **T14** `Source/APBReloaded/Systems/Server/APBWorldGameMode.cpp`: in `MarkRelayPlayerJoined`/`MarkRelayPlayerLeft`, call `SocialAuthority.friends_svc.SetOnline(character, joined)` IDEMPOTENTLY (reject stale leave whose district≠current; keyed by character+district generation) for S2/S8 — verify by editor build exit 0; probe observes presence via replication.

### WAVE 4 — Chat seam (depends on T09+T11)
- **T15** `Source/APBReloaded/Systems/District/APBFreeroamGameMode.h+.cpp`: on PostLogin + after clan/group membership change, read `SocialAuthority.clans.ClanOf`/`groups.GroupOf` and call this district's `ChatService::SetClan/SetGroup`; emit `SOCIAL_CHAT_CLAN_SET`/`SOCIAL_CHAT_GROUP_SET` for S-CHAT/S9 — verify by editor build exit 0.

### WAVE 5 — Probe GREEN (2-client, server-marker sequenced; depends on W2+W3+W4)
- **T16** `Source/APBReloaded/Systems/APBSessionProbeSubsystem.cpp`: implement `RunSocialProbe()` per role. alice/bob connect to the WORLD server (lobby); drive mutations via `Server_Social*` RPCs; sequence cross-client steps on SERVER-SIDE markers (bob waits for replicated `bHasPendingClanInvite` before Accept, not a timer); emit `SOCIAL_PROBE_ALICE_OK`/`SOCIAL_PROBE_BOB_OK`; world emits `SOCIAL_GATE_CLAN_OK`/`SOCIAL_GATE_FRIENDS_OK`/`SOCIAL_GATE_GROUP_OK`/`FRIEND_PRESENCE_OK count=1`/`SOCIAL_SERVER_GATE_OK` for S1-S4,S6 — verify by full 2-client run → all markers in world.log (GREEN). Do NOT reuse `RunWorldServerProbe`'s exit-at-2-logins path.

### WAVE 6 — UMG panel (depends on T05+T06+W2)
- **T17** `Source/APBReloaded/Systems/District/APBSocialWidget.h`: `UCLASS() UAPBSocialWidget : public UUserWidget`, `enum ESocialTab{Clan,Friends,Group,Mail}`, `UPROPERTY() TObjectPtr<...>` child pointers, `UFUNCTION() void On*` handlers for S-WIDGET-DECL — verify by editor build exit 0.
- **T18** `Source/APBReloaded/Systems/District/APBSocialWidget.cpp`: build full tree in `NativeConstruct` via `WidgetTree->ConstructWidget<T>()` (mirror `APBFreeroamHUDWidget`); reads from replicated PlayerState fields + UGI queries; action buttons dispatch through `Server_Social*` RPCs for S-WIDGET-IMPL — verify by editor build exit 0.
- **T19** `Source/APBReloaded/Systems/District/APBFreeroamHUD.h+.cpp`: add `UPROPERTY() TObjectPtr<UAPBSocialWidget> SocialWidget=nullptr;` (BOTH files); in `BeginPlay` after `HudWidget->AddToViewport(10)` create + `AddToViewport(11)` + `SetVisibility(Collapsed)`; bind a toggle key for S-HUD-MOUNT — verify by editor build exit 0; lsp clean.

### WAVE 7 — Gate script + spine (depends on T16)
- **T20** `tools/run_m14_social_gate.ps1`: model on `run_m7_chat_gate.ps1` — 1 world + 1 Financial district + 2 `social_probe` clients (`-SocialRole=alice|bob`); `Start-Editor`+`Wait-Log` on the world.log markers + `SOCIAL_PROBE_ALICE_OK`/`SOCIAL_PROBE_BOB_OK`; `finally { Stop-AllGateProcesses }`; `LEAKED=0` check; final `M14_SOCIAL_GATE_OK` for S-GATE — verify by standalone run printing `M14_SOCIAL_GATE_OK` + exit 0.
- **T21** `tools/run_verification_gates.ps1`: add a REQUIRED step invoking `run_m14_social_gate.ps1` before `GATE_PASS`, `Tee-Object` to `$m14Log`, `Require-Fresh $m14Log … "M14_SOCIAL_GATE_OK"`; add `m14_social_gate = "M14_SOCIAL_GATE_OK"` to the hardcoded `gate_summary` hashtable literal for S-GATE-SPINE — verify by full spine run exit 0; gate_summary.json contains the key.

---

## 3. SCENARIO CONTRACT (binary observables + evidence; ALL authoritative markers in world.log)

Each scenario needs TWO captured artifacts: (a) RED→GREEN test/probe transcript, (b) the real-surface
marker line. All authoritative social markers are asserted in **world.log** (never a client log),
because the single authority lives on the world GameMode.

| ID | Scenario | Driver | Binary observable (evidence) |
|----|----------|--------|------------------------------|
| **S1** | Clan: alice Create → Invite bob → bob Accept → alice SetMemberRank(promote) | both clients via `Server_SocialClan` | world.log `SOCIAL_GATE_CLAN_OK` AND bob's replicated `ClanId` == alice's clan; promote reflected in `ClanRole` |
| **S2** | Friends+presence: alice SendRequest → bob Accept → bob travels to Financial → relay PlayerJoined → `friends_svc.SetOnline(bob)` → alice sees online | both clients + district travel | world.log `SOCIAL_GATE_FRIENDS_OK` AND `FRIEND_PRESENCE_OK count=1` (asserted in world.log, driven by REAL relay verb, not a probe `SetOnline`) |
| **S3** | Group shared-queue: alice Invite → bob Accept → both SetReady → AllReady → AssignMission | both clients | world.log `SOCIAL_GATE_GROUP_OK` AND `SHARED_QUEUE_OK`; bob replicated `bGroupAllReady==true`, `GroupId` matches |
| **S4** | Edge: clan wrong-faction invite REJECTED (server-derived faction); friends cap(50) + friend/ignore mutual-exclusion enforced; mail Delete-with-unclaimed REJECTED | probe negative assertions | world.log `SOCIAL_EDGE_OK` (each sub-assert logs its rejected `ClanResult`/`FriendResult`/mail typed-result) |
| **S5** | Regression: M6/M7 world+travel+chat gates unchanged | existing gates | `run_verification_gates.ps1` M6/M7 markers still fresh; no diff to their scripts |
| **S6** | Client-authority: a `NM_Client` process actually mutates via `Server_Social*` (not a silent no-op) | 2 real client procs | world.log shows authority applied the mutation with caller identity; a direct client-local bridge call is proven to NOT mutate (guard holds) |
| **S7** | Simultaneous Alice/Bob mutation + restart parity | 2 clients + world restart | after restart, `LoadClans`/`LoadFriends` reproduce both mutations (no lost-update); group intentionally ABSENT (session-transient) |
| **S8** | Presence robustness: duplicate join, reordered stale leave, hard district kill, world restart roster replay | relay fuzz | presence never stuck-online after a real leave; offline-until-authoritative-replay after restart |
| **S9** | Membership change racing `/c` `/g` across two districts | chat + membership | ex-member cannot send after Leave; new member routes immediately; no BadChannel on same-district member |
| **S10** | Mail: foreign-ID claim DENIED; injected grant-failure mid-claim rolls back; retry-once idempotent | mail probe | typed result `NotOwner`/`GrantFailed`; attachment never lost nor double-granted |
| **S11** | Restart at AllReady+AssignMission; stale invite acceptance | world restart | group dissolved cleanly; stale clan/group invite Accept returns `NoSuchInvite`; UI invalidated |
| **S12** | Multi-invite targeted accept | two invites, click older | Domain acts on the SAME clan/group the UI showed (targeted id or single-active-invite rendering) |

S1–S6 are gate-blocking for `M14_SOCIAL_GATE_OK`. S7–S12 are hardening scenarios: implement the
mitigations in W2–W5 and cover with probe sub-asserts where feasible; any not automatable this pass
must be logged as a documented follow-up in `work/`, never silently dropped.

---

## 4. DELEGATION TABLE (category + skills)

| Task | Category | Skills |
|------|----------|--------|
| T01–T03 (Domain members + persistence) | unspecified-high | programming |
| T04 (world GameMode authority) | ultrabrain | programming |
| T05 (UE structs) | quick | programming |
| T06 (replicated PlayerState fields) | ultrabrain | programming |
| T07–T08, T16 (probe RED + GREEN) | unspecified-high | programming |
| T09–T12 (bridge UFUNCTIONs) | unspecified-high | programming |
| T13 (Server RPCs) | ultrabrain | programming |
| T14 (relay presence) | ultrabrain | programming |
| T15 (chat seam) | unspecified-high | programming |
| T17–T19 (UMG panel + mount) | visual-engineering | frontend, programming |
| T20–T21 (gate script + spine) | quick | programming, git-master |

Commits: one concern per task-cluster; run `git log --oneline -20` + `git log -5 -- <path>` before
the first commit and mirror the dominant subject shape. Stage ONLY M14 hunks (shared dirty worktree).

---

## 5. FILES TO CREATE / EDIT (with anchors)

CREATE: `Source/APBReloaded/Systems/District/APBSocialWidget.h` + `.cpp`;
`tools/run_m14_social_gate.ps1`.
EDIT: `Domain/APBWorldService.h` (L50-51 include block + members), `Domain/APBWorldService.cpp`
(`InitPersistence`, `SaveAllNow`); `Systems/APBGameInstanceSubsystem.h` (beside `FAPBDomainSnapshotUE`
L28-51) + `.cpp`; `Systems/APBPlayerState.h`+`.cpp` (replicated fields + Server RPCs);
`Systems/Server/APBWorldGameMode.h`+`.cpp` (`SocialAuthority` member + relay handlers L~429-466);
`Systems/District/APBFreeroamGameMode.h`+`.cpp` (chat seam); `Systems/District/APBFreeroamHUD.h`+`.cpp`
(SocialWidget field + `BeginPlay` mount, `.cpp` AddToViewport site L~13);
`Systems/APBSessionProbeSubsystem.h`+`.cpp` (`StartProbe` ~L112, `ArmProbeTimers` ~L262);
`tools/run_verification_gates.ps1` (gate step + `gate_summary` hashtable literal ~L389-406).

---

## 6. SHARED-WORKTREE / M8 CONTENTION CHECKPOINT

The whole M7–M16 tree is uncommitted WIP; M14 files are already dirty. Rules:
- M14 OWNS the new `APBSocialWidget.*` outright.
- Coordinate single-integrator hunks on shared files M8 (Social DISTRICT) may also touch:
  `APBFreeroamHUD.*`, `APBSessionProbeSubsystem.*`, `APBReloaded.Build.cs`, and the gate runner.
  Prefer the STANDALONE `run_m14_social_gate.ps1`; touch `run_verification_gates.ps1` only in W7.
- Before every patch: inspect `git diff <path>` for that exact file; stage ONLY M14 hunks; never
  `git add .`; never revert unfamiliar sibling changes; no broad reformatting.

---

## 7. EXECUTION HANDOFF

Execute via `/ulw-loop`: TDD RED (W1 probe skeleton) → implement wave-by-wave → GREEN (W5 2-client
probe) → gate (W7). Definition of done: `M14_SOCIAL_GATE_OK` fresh in the spine, `gate_summary.json`
carries `m14_social_gate`, Domain harness FAILS=0, both UE targets build exit 0, S1–S6 markers in
world.log, S7–S12 mitigations implemented or logged as documented follow-ups.
