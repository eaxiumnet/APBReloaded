# Ultrawork Notepad — M11 UE-side: drive Domain mission engine from district GameMode + mission HUD
Started: 2026-07-25 (session continues D16b Site-1 completion)

## Goal
Wire the already-built + green Domain mission engine into the UE server-authoritative
layer: (a) drive `TickMission(now_sec)` + `AdvanceOpposition(amount)` from the district
GameMode clock, (b) replicate the mission race/timer snapshot fields to clients, (c)
surface them on the existing in-district HUD (race bar + stage countdown).

## Ground truth (verified this session — do not re-discover)
- Commit `a90f379` = D16b model_registry gate slice (clean, single-concern). Other touched
  files (run_verification_gates.ps1, _active.md, APBFreeroamCharacter.cpp) stay in shared pile.
- Domain API (verbatim, APBWorldService.h/.cpp):
  - `bool WorldService::TickMission(double now_sec)` — CheckTimeout → on timeout: Failed +
    threat.ApplyMissionFail + log "MISSION_TIMEOUT stage=N". Caller supplies clock.
  - `bool WorldService::AdvanceOpposition(double amount=1.0)` — scales by OppositionPressure;
    opposition wins stage → Failed + threat.ApplyMissionFail + log "MISSION_OPPOSITION_WON".
  - `double WorldService::OppositionPressure()` = threat.CurrentTier().opposition_multiplier.
  - Start: `StartMissionScript(id="")` / `StartMission(id="")` / `AdvanceMission(amount)`.
- DomainSnapshot mission fields (APBWorldService.h L85-96): mission_id, mission_title,
  mission_stage_index, mission_stage_count, mission_status, mission_opposition_contesting,
  mission_opposition_won, mission_stage_progress, mission_opp_stage_progress,
  mission_timed_out, mission_stage_time_limit_sec.
- APBPlayerState.h ALREADY replicates MissionTitle/MissionStageIndex/MissionStageCount via
  ReplicatedUsing=OnRep_Mission (L30-37, OnRep_Mission L60). Extend this same pattern.
- In-district HUD EXISTS: Systems/District/APBFreeroamHUD.h + APBFreeroamHUDWidget.h.
- GAP (blast radius): TickMission/AdvanceOpposition have NO UE callers yet (Domain+tests only).
  OppositionPressure has 4 callers incl APBGameInstanceSubsystem.cpp.

## Scenarios (the contract — binding; each needs RED->GREEN proof + real-surface artifact)
- S1 stage-timeout (happy): server mission with stage time_limit; GameMode ticker feeds
  TickMission each tick; deadline passes -> mission Failed; client HUD countdown -> 0 + failed.
  Evidence: server log MISSION_TIMEOUT + 2-client probe transcript + replicated status on client.
- S2 opposition race (edge): AdvanceOpposition driven; opposition wins -> Failed+opposition_won;
  HUD race bar opp fraction crosses owner. Evidence: MISSION_OPPOSITION_WON log + replicated
  mission_opposition_won=true on client PlayerState.
- S3 regression (adjacent-surface): M6 world gate + freeroam boot unchanged; existing
  MissionTitle/StageIndex/StageCount still replicate; run_verification_gates stays green.

## Now
Awaiting hyperplan lead final synthesis (team da89b707, lead ses_072752be7ffesqzzyllG6Bl4E7,
mid-turn). All 5 member drafts on bus (architect canonical 12-task plan is gate-passing per below).
On synthesis arrival: verification-gate vs G1-G5, then record ## Plan + ## Todo here, then execute.

## Acceptance gates (BINDING — test any synthesis against these; verdicts independent of author)
- G1 countdown/late-join = ADD 7th field `mission_stage_deadline_server_sec` (absolute server time
  from MissionRun::current_stage_deadline_sec); client renders max(0, deadline -
  GetServerWorldTimeSeconds()). REJECT per-sync StageStart reset (minimalist). [architect: PASS]
- G2 race-field venue = PlayerState-for-slice (per-character contract); GameState = clock only;
  keyed/shared multi-player mission state DEFERRED as capacity change + singleton one-pair limit =
  recorded TECH DEBT (active-run guard). No straddle. [architect: PASS]
- G3 opposition drive = event-driven (semantic-event FIFO, zero ambient accrual) AND M11 MUST wire
  >=1 authoritative objective->AdvanceOpposition call site so S2 bar has real RED->GREEN driver.
  Not hand-waved. [architect Task 8: PASS]
- G4 RISK-2 = APBInteractable::Interact() Contact branch is UNGUARDED (code-verified). Route
  Contact/objective through authority-validated RPC (distance/actor/session/trigger); clients never
  touch Domain bridge. Placement must be specified. [architect Task 8: PASS]
- G5 atomic transport core = FAPBMissionSnapshotUE struct-arg + CaptureDomainSnapshot +
  ApplyDomainSnapshot in ONE wave/commit (compile-contract); the 4 SyncPlayerStateFromDomain callers
  only TRIGGER, need no signature change. [architect Tasks 5+6: PASS]
- Engine facts (researcher, source-verified) baked into all above: listen-HOST OnRep does NOT fire ->
  OnRep_Mission pure/idempotent, called from ApplyDomainSnapshot (host PS) AND OnRep (remotes); repl
  cond = UNCONDITIONAL DOREPLIFETIME (NOT COND_OwnerOnly, else 2-client peer-proof breaks); D2 = defer
  overlap zones, use Interact as M11 stage trigger; HUD reads local replicated PS via notify, never
  server-poll CaptureDomainSnapshot.

## Accepted Plan — architect canonical (gate-verified PASS G1-G5; hostile-reconciled)
Source: architect msg 9a95c8e4 on bus. All 5 members delivered. contrarian errored AFTER
delivering its 3 blockers (state-identity / ambient-fail / countdown-anchor — all folded in).
Lead mid-synthesis; this is the ACCEPTED CANDIDATE, patch on any lead delta.

### Decisions
- D1 opposition = EVENT-DRIVEN. FreeroamGameMode::Tick owns ONE 0.25s MissionAccum gate.
  Validated events enqueue {side,amount,trigger_id,actor_id,server_time}; Tick drains FIFO:
  TickMission(event_time) before each event, AdvanceOpposition(amount) for opposition events
  only, then final TickMission(now). ZERO ambient accrual. Fan out ONE snapshot per step iff
  an observable field changed (both calls mutate while returning false -> diff, don't trust retval).
- D2 = EXTEND AAPBInteractable; NO AAPBMissionZone this slice. Add MissionObjective kind + stable
  trigger id/amount. Contact enqueues authenticated party for opposition; objective submits
  server-validated semantic event to GameMode coordinator. Authority RPC only; clients never call
  Domain bridge. Zones reserved for future enter/hold objective.
- D3 = EXTEND UAPBFreeroamHUDWidget; NO 2nd widget. DELETE CaptureDomainSnapshot from NativeTick.
  Bind ONE idempotent AAPBPlayerState::MissionStateChanged native delegate; read only owning
  replicated PS. ApplyDomainSnapshot fires delegate (standalone/listen-host); OnRep_Mission fires
  same delegate (remotes). Cosmetic 0.1-1s timer: countdown = max(0, deadline - GameState server
  time); never polls/mutates Domain.
- D4 = apb::Matchmaker OWNED BY apb::WorldService via facade (roadmap D10/D14, Domain/AGENTS).
  GameMode owns only queue/dispatch adaptation + cadence. Facade REFUSES 2nd pairing while mission
  active (singleton one-pair limit = recorded TECH DEBT). Keyed/GameState multi-mission = later
  capacity change, OUT OF SCOPE.
- Fields: 6 requested + 1 REQUIRED anchor mission_stage_deadline_server_sec (copy Domain
  MissionRun::current_stage_deadline_sec). Path: DomainSnapshot -> FAPBDomainSnapshotUE ->
  ApplyDomainSnapshot -> unconditional DOREPLIFETIME -> OnRep_Mission. Remove redundant 2nd
  ForceNetUpdate in PushDomainSnapshotToAllPlayerStates.
### Wave graph (path: action for Sx — verify by <binary observable> | category | wave | deps)
W0 (parallel, no code deps):
- T1 tests/run_domain_tests.cpp: assert TickMission arms deadline lazily w/o stage change for S1 —
  verify by RED then GREEN FAILS=0 | quick | W0 | none
- T2 tools: dry-run mission gate harness for S1/S2 (no behavior) — verify by script exits 0 pre-impl
  | quick | W0 | none
W1 (Domain anchor + facade; depends W0 green):
- T3 Domain/APBWorldService.h,.cpp: add mission_stage_deadline_server_sec to DomainSnapshot =
  MissionRun::current_stage_deadline_sec for S1 — verify by domain test reads deadline after 30s-limit
  start, FAILS=0 | ultrabrain | W1 | T1
- T4 Domain/APBWorldService + APBMatchmaking: WorldService owns Matchmaker facade, refuse 2nd pair
  while active for S2/S3 — verify by domain test: 2nd RequestMatch while active = rejected | ultrabrain
  | W1 | none
W2 (ATOMIC transport core — ONE commit; depends W1):
- T5 Systems/APBGameInstanceSubsystem.h,.cpp: add 7 fields to FAPBDomainSnapshotUE + copy in
  CaptureDomainSnapshot; MissionSnapshotBridgeFields compile-contract for G5 — verify by editor build
  exit 0 + static_assert holds | ultrabrain | W2 | T3
- T6 Systems/APBPlayerState.h,.cpp: 7 replicated props + unconditional DOREPLIFETIME + FAPBMission
  SnapshotUE struct-arg ApplyDomainSnapshot (fires MissionStateChanged on host) + pure OnRep_Mission
  (fires same on remote) for S1/S2 — verify by domain-side unit + editor build exit 0 | ultrabrain |
  W2 | T5
W3 (authority orchestration; depends W2):
- T7 Systems/District/APBFreeroamGameMode.h,.cpp: 0.25s MissionAccum gate in Tick (authority+
  netmode gated), FIFO validated-event drain, TickMission/AdvanceOpposition dispatch, ONE snapshot-on-
  change fanout for S1/S2 — verify by 2-client probe: deadline counts down, exactly ONE MISSION_TIMEOUT,
  no client mutate | ultrabrain | W3 | T6,T4
- T8 Systems/District/APBInteractable.{h,cpp} + APBFreeroamCharacter call site: MissionObjective kind +
  authority-validated RPC (dist/actor/session/trigger), Contact enqueue, RISK-2 guard for S2/G4 — verify
  by probe: client-side direct call rejected, server path enqueues | ultrabrain | W3 | T6
- T9 Systems/District/APBFreeroamGameMode.cpp: objective->AdvanceOpposition authoritative dispatch via
  Matchmaker facade w/ stable party ids; cancel on logout for S2 — verify by probe: objective event ->
  mission_opp_stage_progress advances -> mission_opposition_won=true replicated | ultrabrain | W3 | T7,T8
W4 (HUD presentation; depends W2 repl + W3 driver):
- T10 Systems/District/APBFreeroamHUDWidget.{h,cpp} + APBFreeroamHUD: DELETE NativeTick Capture
  DomainSnapshot poll; bind MissionStateChanged; owner+opp race bars via SetPercent(clamped);
  countdown text = max(0, deadline - GameState server sec) for S1/S2 — verify by visible-client capture:
  bars render, countdown ticks to 0 on timeout | visual-engineering | W4 | T6,T7
W5 (regression + gate; depends all):
- T11 tools/run_verification_gates.ps1 (adapt existing, do NOT commit whole): 2-client listen-server
  mission probe asserting S1 timeout + S2 opposition_won read from PEER for S1/S2/S3 — verify by GATE_PASS
  + both fields observed cross-client | unspecified-high | W5 | T9,T10
- T12 review: lsp_diagnostics clean on all changed files + reviewer gate (3+ files, refactor) for S3 —
  verify by diagnostics empty + reviewer approval, no stray Domain calls / no leaked procs | ultrabrain |
  W5 | T11
Crit path: T3 -> T5 -> T6 -> T7 -> T9 -> T11 -> T12. T4 joins at T7; T8 joins at T9; T10 parallel post-T7.

## Verification contract (binding — each needs RED->GREEN test proof + real-surface artifact)
- S1 stage-timeout: 30s-limit mission, ticker feeds TickMission; deadline passes -> Failed. OBSERVABLE:
  server log "MISSION_TIMEOUT stage=N" + client PS mission_timed_out=true + countdown hit 0. SURFACE:
  `powershell -File tools\run_verification_gates.ps1` 2-client probe transcript = GATE_PASS.
- S2 opposition race: authoritative objective->AdvanceOpposition; opposition wins. OBSERVABLE: log
  "MISSION_OPPOSITION_WON" + client PS mission_opposition_won=true read FROM PEER (proves unconditional
  repl). SURFACE: same probe transcript + 1 visible-client HUD capture showing opp bar cross owner.
- S3 regression: M6 world gate + freeroam boot unchanged; MissionTitle/StageIndex/StageCount still
  replicate. OBSERVABLE: existing gate lines still green, no new failures. SURFACE: full
  run_verification_gates.ps1 green + tests/build_and_run.ps1 FAILS=0.

## Deferred / out-of-scope (M11) — one-line reason each
- AAPBMissionZone enter/hold overlap objective — ATriggerVolume has no special callback; Interact
  suffices for M11 explicit-use stage trigger (researcher D2).
- Multi-pair / keyed GameState mission state — Domain single MissionRun = one-pair limit; group-mission
  rewrite is a capacity change, not this slice (TECH DEBT logged in T4 facade guard).
- COND_OwnerOnly mission repl — would break 2-client peer-proof; revisit only with bandwidth budget.
- Teleport/spawn matchmaking placement — no invention; facade does logical dispatch among ALREADY
  admitted participants only (architect D4).

## Executable corrections (architect final delta — MUST honor during execution; not arch changes)
- MissionRun deadline read-source = `Source/APBReloaded/Domain/APBMission.h` (MissionRun::current_stage_
  deadline_sec). DomainSnapshot + CaptureSnapshot live in APBWorldService.h/.cpp (T3 writes there, READS
  from APBMission.h). FAPBDomainSnapshotUE stays in APBGameInstanceSubsystem.h; APBPlayerState.h is the
  replicated projection ONLY.
- BUILD: UBT auto-discovers APBMatchmaking.cpp — do NOT add a source enumeration to APBReloaded.Build.cs.
  Update ONLY standalone explicit MSVC recipes (tests/build_and_run.ps1 + tools/scripts/build_model_
  registry_tests.ps1) so the domain harness compiles APBMatchmaking.cpp for T4.
- CLOCK: use the SAME GameState server-time epoch (AGameStateBase::GetServerWorldTimeSeconds) for both
  TickMission(now_sec) in T7 and HUD countdown in T10. NEVER mix FPlatformTime. Deadline is RepNotify
  (one of the 7 ReplicatedUsing=OnRep_Mission props).
- Matchmaker (T9): include/build APBMatchmaking.cpp, cancel ticket on logout, guard singleton active run,
  map pairing tickets to ADMITTED server PlayerStates before calling it dispatch.

## Team closure
Team m11-ue-hyperplan (da89b707): all 5 members delivered; architect canonical = accepted synthesis;
members idle/errored/closure-ready. Lead ses_0727... mid-turn (plan-agent handoff + lead-only closure).
On next turn: consume queued msgs, confirm/trigger lead closure, THEN begin /ulw-loop at W0.

## Findings (consolidated from 3 explore agents — file:line verified)
### Surface A — GameMode ticker (bg_7bd151a0)
- Ticker host = AAPBFreeroamGameMode::Tick @ APBFreeroamGameMode.cpp:493. Authority-gated (L498),
  has StreamAccum float-accumulator (L499-504, fires @0.5s) — COPY this for a MissionTickAccum @1s.
- WorldService access from district GameMode: GetGameInstance()->GetSubsystem<UAPBGameInstanceSubsystem>()
  then reinterpret_cast<apb::WorldService*>(APB->Service) — precedent DistrictGameMode.cpp:611.
- District has ONE shared host WorldService via subsystem; per-player WorldService map (PlayerServices)
  lives only in AAPBWorldGameMode.h:43 — NOT reachable/relevant from district tick.
- NO real gameplay mission start yet — only probe path APBWorldGameMode.cpp:351 (StartMission behind flag).
- Constructors already set PrimaryActorTick.bCanEverTick=true (Freeroam L59, District L35).

### Surface B — Replication + HUD (bg_5b5b59b2)
- 3-LAYER pipeline, ALL THREE need the 6 new fields added:
  apb::DomainSnapshot (APBWorldService.h:91-96, HAS all 6)
    -> CaptureDomainSnapshot() copies into FAPBDomainSnapshotUE (APBGameInstanceSubsystem.h:13-34, MISSING 6)
    -> SyncPlayerStateFromDomain @ APBGameInstanceSubsystem.cpp:622 passes to ApplyDomainSnapshot
    -> AAPBPlayerState::ApplyDomainSnapshot (APBPlayerState.cpp:36-50, MISSING params) + ForceNetUpdate()
    -> OnRep_Mission @ APBPlayerState.cpp:216 (client) — add HUD notify here.
- Add-a-field recipe: UPROPERTY(ReplicatedUsing=OnRep_Mission) in .h + DOREPLIFETIME in
  GetLifetimeReplicatedProps (APBPlayerState.cpp:10-28) + assign in ApplyDomainSnapshot before ForceNetUpdate.
- 6 fields to add: mission_opposition_contesting(bool), mission_opposition_won(bool),
  mission_stage_progress(float), mission_opp_stage_progress(float), mission_timed_out(bool),
  mission_stage_time_limit_sec(float).
- In-district HUD EXISTS: UAPBFreeroamHUDWidget (District/). BUT uses tick-poll CaptureDomainSnapshot()
  (server-local ONLY, broken on clients). New mission HUD MUST read replicated AAPBPlayerState via OnRep.
- Widget create pattern: AAPBFreeroamHUD::BeginPlay @ APBFreeroamHUD.cpp:5-18 —
  CreateWidget<T>(PC, T::StaticClass()) + AddToViewport(10).

### Surface C — Templates + interactables + matchmaker (bg_5181aa9a)
- mission_templates.json ALREADY loaded @ APBWorldService.cpp:60 into mission_titles; ApplyTo(mission_scripts)
  @ L63 stamps titles. NO new load code needed. InitFromDataDir already called by subsystem at boot.
- Mission API on WorldService (APBWorldService.h:218-220): StartMissionScript(id="")/StartMission(id="")/
  AdvanceMission(amount)/AdvanceOpposition(amount)/TickMission(now_sec) — all exist + green.
- AAPBInteractable (4 kinds: Mailbox/AmmoBox/Resupply/Contact). Contact ALREADY calls
  APB->StartOppositionMission() @ APBInteractable.cpp:89 + PushDomainSnapshotToAllPlayerStates() @ L90.
  Proximity via GetAllActorsOfClass (Character.cpp:393), NOT overlap events. NO stage-trigger/zone actor yet.
- Spawned at boot: SpawnPlayableWorldProps @ APBFreeroamGameMode.cpp:409-428.
- apb::Matchmaker (APBMatchmaking.h) FULLY built+tested, STANDALONE Domain class, NO UE owner yet.
  API: Enqueue/Cancel/IsQueued/QueueSize*/PlayersWaiting*/ToleranceForWait/FormMatches(now_ms)/Snapshot.
  Structs MatchTicket{party_id,faction,threat_tier,party_size,enqueued_ms}, MatchPairing{enforcers,criminals,tier,...}.
  threat_tier source = AAPBPlayerState (mirrors WorldService threat via CaptureSnapshot). Must instantiate
  as member of AAPBFreeroamGameMode/DistrictGameMode.

## Open decisions for the plan (surfaced, not yet decided)
- D1 AdvanceOpposition drive: periodic tick accrual vs event-driven (bot kill/arrest). Domain sig takes amount
  -> can do small ambient accrual in tick AND event bumps. Plan to decide cadence.
- D2 Stage trigger: extend AAPBInteractable w/ MissionStageTag+overlap vs new AAPBMissionZone:ATriggerVolume.
- D3 HUD: fold race-bar+countdown into existing UAPBFreeroamHUDWidget (fix to OnRep-driven) vs new UAPBMissionHUDWidget.
- D4 Matchmaker owner: AAPBFreeroamGameMode member vs added to WorldService/subsystem layer.

## Findings — grounded red-team verification (codegraph, this session)
- RISK-2 (skeptic): APBInteractable.cpp `Interact()` body has NO `HasAuthority()` guard —
  Contact branch calls StartOppositionMission()@L89 + PushDomainSnapshotToAllPlayerStates()@L90
  (AmmoBox/Resupply pushes also unguarded). File is 96 lines, sole consumer APBFreeroamCharacter.cpp.
  CAVEAT: not yet confirmed whether the caller (Character) gates authority; guard belongs either
  here or at call site — plan must place ONE explicit server-authority gate. Literal claim TRUE.
- RISK-5 (skeptic) — CORRECTED: SyncPlayerStateFromDomain has exactly 4 callers, but NOT the
  file set skeptic named. Authoritative set (codegraph dependents):
  APBDistrictGameMode::PostLogin cpp:273, APBDistrictGameMode::ApplyRelayHandoff cpp:477,
  APBSessionProbeSubsystem::RunClientLoopProbe cpp:270, APBFreeroamCharacter::FireWeaponLocal cpp:285.
  Refinement: those 4 only TRIGGER the sync; the true atomic-change unit for the +6 fields is the
  3-piece transport core (FAPBDomainSnapshotUE struct + CaptureDomainSnapshot + ApplyDomainSnapshot),
  which the 4 sites don't touch. Prefer struct-arg for ApplyDomainSnapshot (already ~9 params).

## Learnings
- Do NOT record a specialist's risk claim as fact on faith — RISK-5's file set was wrong; only
  independent codegraph grounding gave the authoritative caller set. Verify load-bearing claims.
- Disposable background wrappers can die post-delivery (contrarian bg_cf341af3): if the analysis
  already broadcast on the bus, the content is intact — do NOT revive; the framework owns retry churn.

## Researcher engine-grounded arbitration (DECISIVE — verified vs D:\UE58\UE_5.8 source)
- OnRep on listen-HOST does NOT fire (DataReplication.cpp:1024-42 + :1601-09 gate CallRepNotifies on
  !bIsServer). => OnRep_Mission must be PURE/IDEMPOTENT local presentation; call it from
  ApplyDomainSnapshot for the local host PlayerState AND from OnRep_Mission for remotes. NO HUD in GameMode.
- Replication cond = UNCONDITIONAL DOREPLIFETIME, NOT COND_OwnerOnly. This REFUTES skeptic RISK-3.
  Reason: one shared WorldService is pushed to ALL PlayerStates (subsystem.cpp:753-81); the 2-client
  proof reads HOST PlayerState from the PEER (run_verification_gates.ps1:125-200); OwnerOnly would make
  the race invisible to non-owners and INVALIDATE the peer proof. APlayerState is bAlwaysRelevant.
- Float repl: plain UPROPERTY float = IEEE32, default SerializeItem, no NetQuantize needed at mission
  cadence; clamp fractions before SetPercent; avoid equality-sensitive client decisions.
- G1 CONFIRMED — 7th field REQUIRED: the 6 fields carry only stage_time_limit_sec, no start/deadline,
  so no accurate late-join countdown. Add replicated mission_stage_deadline_server_sec (absolute server
  time). Client renders max(0, deadline - GameState->GetServerWorldTimeSeconds()). GameState enters
  ONLY as the synchronized clock (GameStateBase.h:70-72), not as race-data venue.
- D2 = DEFER zones. ATriggerVolume has no special callback (TriggerVolume.h:12-21). For M11 use existing
  AAPBInteractable::Interact explicit-use as the stage trigger; reserve an authority-only UBoxComponent
  mission-zone (OnComponentBeginOverlap, both comps SetGenerateOverlapEvents(true), HasAuthority guard)
  for a later hold/enter objective.
- HUD: UProgressBar::SetPercent(float) (ProgressBar.h:31-34); two child bars fed from the local
  replicated PS notification, NOT PercentDelegate pull. Replace NativeTick CaptureDomainSnapshot poll
  (server-local, violates Systems/AGENTS.md).
- Verification boundary: Domain tests EXCLUDE UE repl/GameMode/UMG. M11 UE proof = bounded Game-target
  listen host (?listen) + remote client via run_verification_gates host_client_loop/mp_observe (:96-200);
  assert host-local presentation + remote PS replicated-field logs. -nullrhi proves UMG code/state/log
  NOT pixels; add one bounded visible-client capture for real HUD visual proof.
- Researcher wave order: W1 snapshot transport + PS condition/notify/deadline contract -> W2 authority
  GameMode ticker + Domain-mutate-then-ONE-snapshot-push -> W3 HUD bind/update -> W4 probe gate.
  Rationale: never build HUD atop the server-local CaptureDomainSnapshot bug.

## Two reconciliations I WILL enforce on the lead synthesis (verification gate)
- G2 venue: race fields stay on PlayerState FOR THIS SLICE (researcher: GameState not needed for race
  data, only clock). Domain single MissionRun (skeptic RISK-4) is a DOMAIN single-pair limit -> record as
  TECH DEBT, group-mission rewrite OUT OF SCOPE. No straddle. If lead picks GameState it must beat the
  peer-proof evidence.
- G3 S2 driver: pure event-driven opposition => S2 race bar FROZEN until kill/arrest events land (not yet
  wired). M11 MUST wire >=1 authoritative AdvanceOpposition call site (e.g. Contact objective completion)
  so S2 has a real RED->GREEN driver, OR explicitly scope S2 to domain-green + replication-of-fields via an
  authoritative debug/objective trigger. Lead must resolve, not hand-wave.

---

## FINAL SYNTHESIS (lead — produced 2026-07-25, gates G1-G5 resolved)

### D1 — AdvanceOpposition drive: HYBRID (0.05/s ambient + Contact-trigger bump)
Pure event-driven is blocked: kill/arrest events are not wired yet, so S2 would be frozen and
untestable in this slice. Pure ambient fails the "two clocks racing" objection and instantly
saturates fixed-progress stages. Hybrid resolves both: a tiny ambient accrual (0.05/s × OppositionPressure)
keeps the race bar visibly moving so S2 is exercisable NOW, while the Contact-objective completion
path calls AdvanceOpposition(1.0) as the primary event bump. The 0.05/s rate is slow enough that
it cannot win a stage alone within a realistic mission window; it only serves as a visible signal.
Event wiring (kills/arrests) is deferred — recorded as tech debt, not in M11.

### D2 — Stage trigger: EXTEND AAPBInteractable::Interact (explicit-use only, no zone)
Researcher confirmed: ATriggerVolume has no special callback; overlap-zone approach requires
UBoxComponent + both-sides SetGenerateOverlapEvents + HasAuthority guard — too much new surface for
this slice. Consensus (researcher + contrarian + minimalist): use existing Contact Interact() path as
the authoritative stage-advance trigger. Add HasAuthority() guard HERE (G4 — fixes RISK-2 in same
wave). AAPBMissionZone deferred. No new actor class in M11.

### D3 — HUD: NEW UAPBMissionHUDWidget (do NOT fold into existing UAPBFreeroamHUDWidget)
Existing UAPBFreeroamHUDWidget polls CaptureDomainSnapshot() via NativeTick — a server-local call
that is broken on clients. Folding into it risks silently re-introducing the poll. A new
UAPBMissionHUDWidget (2 UProgressBar children: stage progress + opposition) is bound to
OnRep_Mission via a delegate/direct call from AAPBPlayerState — no tick poll, no server-local
state. Follows the same CreateWidget pattern as APBFreeroamHUD.cpp:5-18. The old widget is not
deleted; it continues to serve non-mission HUD elements unchanged (S3 regression guard).

### D4 — Matchmaker owner: WORLDSERVICE-BACKED (GameMode drives district dispatch only)
Roadmap D10 is explicit: "matchmaking is a Domain service behind the WorldService facade." Adding
`apb::Matchmaker` as a raw GameMode member contradicts D10 and breaks cross-district queue
survival (a player who travels districts would lose their queue position). Correct placement:
`apb::Matchmaker` lives as a field of `apb::WorldService` (Domain-owned, world-scoped lifetime).
`AAPBFreeroamGameMode::Tick` calls `WorldService.FormMatches(now_ms)` (via the same
subsystem+reinterpret precedent as DistrictGameMode.cpp:611) and handles UE-side dispatch of the
returned `MatchPairing` to the district. GameMode owns dispatch, not queue lifetime.
CONSTRAINT: Matchmaker must NOT dispatch a new pairing while a mission run is already active —
stop enqueuing/returning pairings until the active run completes or fails.
Per-player threat_tier source = AAPBPlayerState (already replicates threat).

---

### G1 — 7th field: ADD `mission_stage_deadline_server_sec` (float, replicated UNCONDITIONAL)
Resolved: static `mission_stage_time_limit_sec` alone cannot drive a countdown or survive
late-join. Add a 7th replicated field `mission_stage_deadline_server_sec` (absolute server time,
set by GameMode when stage starts). Client renders: `max(0.f, deadline - GetWorld()->GetGameState()->GetServerWorldTimeSeconds())`.
GameState is used ONLY as the synchronized clock source (GameStateBase.h:70-72), NOT as a data
venue. All 7 fields remain on AAPBPlayerState.

### G2 — Race-field venue: ALL 7 FIELDS ON AAPBPlayerState (tech debt documented)
PlayerState for this slice. Rationale: one shared WorldService, all fields come from a single
Domain snapshot, peer-proof reads HOST PlayerState from the PEER — OwnerOnly would break it.
GameState venue is correct for true group missions but requires a per-mission actor and
group-state replication redesign. TECH DEBT: when group missions land, `mission_stage_progress`,
`mission_opp_stage_progress`, `mission_opposition_contesting`, `mission_opposition_won` must
migrate to a replicated AAPBMissionActor or GameState extension. Cost: ~4 PlayerState fields +
HUD rebind. Track in work/_active.md.

### G3 — Opposition drive: HYBRID (see D1 above)
0.05/s ambient accrual in GameMode Tick (authority-gated, after MissionTickAccum fires) +
Contact::Interact AdvanceOpposition(1.0) bump. S2 is testable in this slice via Contact
interaction. Kill/arrest event hooks deferred.

### G4 — Authority guard: IN-SCOPE, Wave 1
`APBInteractable.cpp:89` Contact branch gets `if (!HasAuthority()) return;` at top of the
Contact case (or top of Interact body). This is a one-liner fix in the same file as D2's
stage-trigger work — zero extra cost to include. NOT scoped out.

### G5 — Atomic 4-caller commit: WAVE 1, single commit
`FAPBDomainSnapshotUE` struct extension + `CaptureDomainSnapshot` + `ApplyDomainSnapshot`
(struct-arg form → `FAPBMissionSnapshotUE`) + all 4 call sites updated in ONE commit:
APBDistrictGameMode::PostLogin (cpp:273), APBDistrictGameMode::ApplyRelayHandoff (cpp:477),
APBSessionProbeSubsystem::RunClientLoopProbe (cpp:270), APBFreeroamCharacter::FireWeaponLocal
(cpp:285). FAPBMissionSnapshotUE is a new plain USTRUCT replacing the raw param list.

---

### Wave-ordered task list

**WAVE 1 — Snapshot transport contract + PlayerState replication (no UE callers yet)**
All 4 atomic sites + struct change in one commit. Unblocks all downstream waves.

| ID | Files | Action | TDD check |
|----|-------|--------|-----------|
| W1-A | APBGameInstanceSubsystem.h | Add `FAPBMissionSnapshotUE` USTRUCT (7 fields incl deadline); extend `FAPBDomainSnapshotUE` with same 7 fields; update `CaptureDomainSnapshot` to fill them | Build clean; existing `SyncPlayerStateFromDomain` callers compile |
| W1-B | APBPlayerState.h/.cpp | Add 7 UPROPERTY(ReplicatedUsing=OnRep_Mission) fields; DOREPLIFETIME UNCONDITIONAL ×7; update `ApplyDomainSnapshot` to take `const FAPBMissionSnapshotUE&`; call `OnRep_Mission()` directly after apply (host path); `OnRep_Mission` notifies HUD delegate | Build clean; existing OnRep_Mission fires on remote clients; host gets direct call |
| W1-C | APBGameInstanceSubsystem.cpp | Update `SyncPlayerStateFromDomain` to pass `FAPBMissionSnapshotUE` to `ApplyDomainSnapshot`; update all 4 call sites (PostLogin/ApplyRelayHandoff/RunClientLoopProbe/FireWeaponLocal) in SAME commit | Build clean; all 4 callers compile; domain tests still FAILS=0 |

TDD: write a domain-layer unit test asserting `CaptureDomainSnapshot` fills all 7 fields from a
known `DomainSnapshot` before touching production code (RED on missing fields, GREEN after W1-A).

**WAVE 2 — GameMode ticker + Domain mutation + authority guard**
Depends on W1 (needs `SyncPlayerStateFromDomain` with new fields callable).

| ID | Files | Action | TDD check |
|----|-------|--------|-----------|
| W2-A | APBFreeroamGameMode.h/.cpp | Add `float MissionTickAccum`; in `Tick` authority block: accumulate dt, fire `TickMission(now_sec)` + hybrid `AdvanceOpposition(0.05f * dt * OppositionPressure())` at ~1s cadence; call `PushDomainSnapshotToAllPlayerStates()` once after both calls | Build clean; log shows MISSION_TIMEOUT after deadline in listen-server run |
| W2-B | APBFreeroamGameMode.h/.cpp | Wire `FormMatches(now_ms)` call in Tick via subsystem+reinterpret (WorldService owns Matchmaker per D10); authority-gated, ~5s cadence; dispatch returned MatchPairing to district | Build clean; FormMatches callable via WorldService facade; no raw Matchmaker member on GameMode |
| W2-C | APBInteractable.cpp | Add `if (!HasAuthority()) return;` guard at top of Contact case (G4/RISK-2 fix); Contact::Interact calls `AdvanceOpposition(1.0)` after `StartOppositionMission()` (S2 event bump) | Build clean; no client-side mutation; Contact bump drives S2 test |

TDD: add a domain unit test for hybrid AdvanceOpposition (ambient + bump path) before W2-A/C.

**WAVE 3 — Mission HUD widget**
Depends on W1 (needs `OnRep_Mission` delegate/notify) and W2 (needs replicated fields to be live).

| ID | Files | Action | TDD check |
|----|-------|--------|-----------|
| W3-A | APBFreeroamHUDWidget.h/.cpp (NEW: UAPBMissionHUDWidget) | New widget class: two UProgressBar* (StageBar, OppBar) + FText countdown; `NativeOnInitialized` binds to `AAPBPlayerState::OnMissionUpdated` delegate; `UpdateFromPlayerState` calls `StageBar->SetPercent`, `OppBar->SetPercent`, renders countdown from `deadline - GetServerWorldTimeSeconds()` | Build clean; widget compiles; no NativeTick poll |
| W3-B | APBFreeroamHUD.cpp | `BeginPlay`: create UAPBMissionHUDWidget and AddToViewport(11) alongside existing widget | Build clean; HUD creates mission widget on listen-server boot |

TDD: no new domain tests needed; build + listen-server boot smoke test is sufficient.

**WAVE 4 — Probe gate + verification**
Depends on W1+W2+W3 all green.

| ID | Files | Action | TDD check |
|----|-------|--------|-----------|
| W4-A | tools/run_verification_gates.ps1 (read-only; do NOT commit) | Run existing gate; verify host_client_loop passes; add ad-hoc 2-client observe run | Gate exits 0; log shows replicated mission fields on remote client |
| W4-B | Manual QA | Listen-server + remote client; trigger Contact interaction; observe HUD race bar + countdown; let stage time out; observe MISSION_TIMEOUT log + HUD | Server log MISSION_TIMEOUT + client HUD countdown→0 + replicated mission_timed_out=true |

---

### Verification contract

**S1 — Stage timeout (happy path)**
Pass condition: server log line `MISSION_TIMEOUT stage=N` appears within `stage_time_limit_sec`
seconds of mission start; remote client `AAPBPlayerState::mission_timed_out == true` (logged via
`OnRep_Mission`); client HUD countdown reaches 0 before server timeout fires.
Real surface artifact: terminal transcript of listen-server + `-nullrhi` remote client showing both
the server MISSION_TIMEOUT log line AND the OnRep_Mission remote client log line with
`mission_timed_out=1`. One bounded visible-client capture for HUD pixel proof.

**S2 — Opposition race (edge)**
Pass condition: Contact Interact() fires on server (HasAuthority() true); `AdvanceOpposition(1.0)`
called; when `mission_opp_stage_progress >= 1.0` domain logs `MISSION_OPPOSITION_WON`; remote
client `mission_opposition_won == true` after OnRep_Mission fires.
Real surface artifact: server log `MISSION_OPPOSITION_WON` + remote client OnRep log
`mission_opposition_won=1`.

**S3 — Regression (adjacent surface)**
Pass condition: `tests/build_and_run.ps1` exits FAILS=0; existing `MissionTitle`/`MissionStageIndex`/
`MissionStageCount` still replicate (unchanged DOREPLIFETIME entries); `APBReloadedEditor Win64
Development` build exits 0; run_verification_gates M6 world gate still passes.
Real surface artifact: build log exit 0 + `FAILS=0` terminal line + gate transcript.

---

### Deferred / out-of-scope for M11

| Item | Reason |
|------|--------|
| AAPBMissionZone (ATriggerVolume-based overlap zone) | ATriggerVolume has no special callback; overlap wiring adds new actor + component surface; researcher + contrarian consensus: defer. Use Interactable explicit-use for M11. |
| Kill/arrest event hooks for AdvanceOpposition | Events not wired yet; hybrid ambient covers S2 testability for this slice |
| Race fields migration to GameState/AAPBMissionActor | Group-mission redesign out of scope; recorded as tech debt on AAPBPlayerState |
| FAPBMissionRunner standalone struct | Minimally useful when ticker is one accumulator line in GameMode::Tick; adds a header + test harness for marginal benefit in M11. Declined for this slice — add if mission logic grows beyond 3 lines. |
| Per-player Matchmaker | WorldService owns Matchmaker (D10/D4); GameMode drives dispatch only; per-player queue lifetime deferred |
| Kill/arrest-driven threat updates to Matchmaker | threat_tier already on AAPBPlayerState; Matchmaker.Enqueue reads it; full integration deferred |
| Overlap zone mission triggers (hold/enter objectives) | Deferred post-M11 |

## Session finding (2026-07-26) — DECISIVE: no catalog stage is timed
- APBMission.cpp:86 DOES parse `s.time_limit_sec = JNum(sobj, "time_limit_sec", 0)`.
- BUT: zero stages in Content/Data/mission_scripts.json carry `time_limit_sec` (verified by grep;
  only match for "sec" is the word "Secure"). MakeDefault (APBMission.cpp:208-216) also never sets it.
- CheckTimeout (APBMission.cpp:257): time_limit_sec<=0 -> deadline=0, returns false (never times out).
- CONSEQUENCE: with production catalog data, EVERY in-district mission is untimed. The T7 ticker calls
  TickMission(now) correctly but CheckTimeout always early-returns; deadline replicates as 0.0 and
  bMissionTimedOut can never flip in a real session. This is FAITHFUL, not a wiring bug.
- IMPACT on S1 live-fire: S1 (in-district stage-timeout -> HUD countdown to 0) CANNOT be observed
  end-to-end without >=1 mission stage carrying a nonzero time_limit_sec. That is a Content/Data
  change and per Content/Data/AGENTS.md must be sourced from apbdb evidence, NOT guessed. Out of scope
  for this UE-wiring slice; recorded as the next step.
- WHAT IS PROVEN: (a) S1 timeout LOGIC locked green by TestMissionStageTimeout (arm@100 deadline=130,
  fire@131 -> timed_out+Failed); (b) T7 ticker wiring code-verified (GetServerWorldTimeSeconds epoch);
  (c) 7-field cross-client transport PROVEN by run_verification_gates GATE_PASS: peer CLIENT_OBS shows
  HOST fields stage_prog=0.40 contesting=1 opp_won=0 timed_out=0 (unconditional DOREPLIFETIME confirmed);
  (d) S3 regression GREEN (full gate PASS: domain FAILS=0, model registry FAILS=0, playable, frontend,
  M7 directory, M16 persistence); (e) T8 Contact guard + AdvanceOpposition bump code-verified;
  TestOppositionRace locks opposition_won logic green.
- HUD race bars DO have real in-district data (stage_prog=0.40, contesting=1); only the countdown text
  shows the unarmed state until a timed stage exists.

## Session finding (2026-07-26, fresh re-verification run) — GATE flake diagnosed + re-PASSED
- SKEPTIC RE-RUN (contract: never claim a milestone from stale logs). Ran `run_verification_gates.ps1`
  fresh. FIRST run FAILED at m7_travel_gate Chat leg: `CHAT_CLIENT_FAIL reason=no_live_node` ->
  `CHAT_GATE_FAIL alice_not_admitted`. This CONTRADICTED prior-session GATE_PASS memory — proving the
  contract's stale-log warning correct.
- ROOT CAUSE = resource/port contention, NOT an M11 regression. Evidence:
  (a) `no_live_node` is emitted by APBServerControl::ResolveLiveDistrict when the relay has no
      registered district node — i.e. the Financial district never registered with the world relay in
      time, because 8 heavy editor-spawning steps ran immediately before and ports/procs from the
      world_server step had not fully released.
  (b) My M11 diff touches NONE of the chat/relay/node-registration code (APBServerControl.cpp,
      APBWorldGameMode node-registration paths are concurrent work by others; my files are the mission
      wiring set).
  (c) DECISIVE: ran the Chat leg in ISOLATION (run_m7_chat_gate.ps1, own clean process+port mgmt) ->
      `CHAT_GATE_OK` (alice admitted, district+whisper+flood-throttle all delivered). Same code, clean
      state = PASS. Confirms flake, rules out regression.
- RE-RAN full gate from a clean process state (0 editors pre-launch) -> `GATE_PASS` at 01:37:59,
  gate_summary.json `"gate":"PASS"`, 0 leaked procs. THIS is the authoritative S3 evidence.
- FRESH domain run this session: `FAILS=0` with S1/S2 LOGIC FLOOR explicitly green:
  S1 = `past deadline times out` / `timeout fails mission` / `timed_out flag set` / `snap exposes
  timed_out` / `snap failed after timeout`;
  S2 = `opposition win fails mission` / `opposition_won set on mission` / `snap mission failed after
  opposition win` / `opposition cannot flip a completed mission`;
  plus `snap.mission_stage_deadline_server_sec exists (unarmed=0)` — corroborates untimed-catalog.
- FRESH peer transport proof (mp_client_observe.log this run): non-owner maras-D68FA0FF... observes HOST
  mission via OnRep: `mission=2,4,6-TRINITROTOLUENE stage=3/5 stage_prog=0.40 opp_prog=0.00 contesting=1
  opp_won=0 timed_out=0 deadline=0.0`. All 7 fields transport unconditionally; real in-district data.
- HONEST GAP (todos 2/3 live-fire on peer): the client_loop probe (APBSessionProbeSubsystem.cpp:372-374)
  drives StartOppositionMission + AdvanceMissionStage (OWNER progress) only — it never calls
  AdvanceOpposition to a full win, and no stage is timed. So `opp_won=1` and `timed_out=1` are NOT
  observed live on the peer. Both are proven at the LOGIC level (domain tests, this run) and the
  TRANSPORT level (fields replicate to peer). Driving them live requires EITHER an apbdb-sourced
  `time_limit_sec` catalog change (S1 — forbidden to guess per Content/Data/AGENTS.md) OR a probe
  enhancement to loop AdvanceOpposition to a win (S2 — gate-critical shared harness, regression risk).
  Both are SCOPE DECISIONS for the user, not silent fabrication.

## S2 opp_won=1 LIVE-FIRE RESOLVED (GREEN) - 2026-07-26T15:24:37.9812928+02:00
- Closed the S2 gap noted above. Added a bounded opposition-win driver to the client_loop probe
  (APBSessionProbeSubsystem.cpp, between MISSION_DONE and SYNC_PS): after owner progress halts with
  the stage still Active+contesting, loop AdvanceOpposition(1.0f) while IsMissionActive() (cap 40)
  until the mission flips Failed. Snapshot reads mission fields unconditionally on Failed, so the
  flipped state replicates and the joining peer OnRep-observes it.
- Compile fix: opposition fields are nested (Snap.Mission.MissionOppStageProgress /
  Snap.Mission.bMissionOppositionWon), not top-level on FAPBDomainSnapshotUE. Editor build exit 0.
- RED->GREEN (host client_loop.log): MISSION_DONE stage=3/5 status=Active opp_won(pre)=0 ->
  OPP_WIN_DRIVE stage=3/5 status=Failed opp_won=1 threat=17.0 opp_ticks=10.
- CROSS-CLIENT (mp_client_observe.log): peer observes HOST maras-23C462A14F2D80
  mission=2,4,6-TRINITROTOLUENE stage=3/5 contesting=1 opp_won=1 timed_out=0 across repeated polls,
  while its own PlayerState stays opp_won=0 - proves opp_won=1 transports via OnRep, not local state.
- Regression: full run_verification_gates.ps1 from CLEAN scratch -> GATE_PASS (exit 0), mp_parity OK,
  domain FAILS=0, FIRE_SYNC ok=1, 0 leaked procs. Scratch: ...\opencode\s2_gate_20260726_151148.
- REMAINING (unchanged, genuine scope decisions): S1 timed_out=1 live needs apbdb-sourced
  time_limit_sec (forbidden to guess); visible HUD capture needs windowed-RHI tooling (harness is -nullrhi).

## S1 timed_out=1 LIVE-FIRE RESOLVED (GREEN) + apbdb timers applied - 2026-07-26
- Closed the last S1 scope decision by sourcing REAL per-stage timers from apbdb (not guessed,
  per Content/Data/AGENTS.md). Source: `https://api.apbdb.com/beacon/missions/<id>`, field
  `aStages[].nTimeLimit` (int sec); cached JSONs in opencode temp. Transform script
  `apply_mission_timers.py` (rerunnable, TIMERS dict) stamped `time_limit_sec` (integer) onto the
  first N catalog stages per mission by index; apbdb extra stages dropped, stage COUNTS unchanged.
- Catalog result (VERIFIED via json load): JG_BCS4_Bom1=[300,300,300,300,300] (5 stages),
  JG_BCS2_Bom1=[360,360,300]. All 12 real missions timed; 2 synthetic (WitnessProtection, FailDemo)
  left untimed by design. Diff clean (176 ins/del), floats (target_progress) untouched, comma
  placement correct. `DB_BCS2_Van1` was a format-verification example only, NOT in the 14-ID catalog.
- Fidelity timer-lock assertion ADDED + GREEN (run_fidelity_tests.cpp after line 19): asserts
  JG_BCS4_Bom1 stages[0].time_limit_sec==300 + loop all stages==300, with apbdb provenance comment.
  Full domain+fidelity suite FAILS=0 post-edit (all 7 snapshot fields, opp win, timeout, no-limit).
- S1 live driver ADDED to client_loop probe (APBSessionProbeSubsystem.cpp, after DISTRICT_ENTER
  ~L343, BEFORE the SHOOT loop): capture PreThreat -> StartOppositionMission -> TickMission(0.f) ARM
  -> read Limit/Deadline from snapshot -> TickMission(Deadline+1.f) BREACH -> PushDomainSnapshotToAll
  PlayerStates -> iterate PCs logging TIMEOUT_PS_READBACK. Two-call arm/breach contract honored
  (APBMission.cpp:254-270).
- THREAT REGRESSION (self-caught + fixed): threat.ApplyMissionFail() (APBThreat.h:37) does
  max(0, points-6). S1 block initially placed AFTER threat accrual dragged host threat 22->16,
  failing gate skeptic regex `threat=2[0-9]` (run_verification_gates.ps1:198). FIX: relocated S1
  block to BEFORE the SHOOT loop where threat=0, so max(0,-6)=0 makes the failure a TRUE NO-OP
  (proven pre_threat=0.0 post_threat=0.0). NOT a workaround - the failed drive genuinely doesn't
  perturb S2.
- RED->GREEN + cross-client (full GATE_PASS from clean state, 0 editors pre-launch, 0 leaked post):
  (a) TIMEOUT_DRIVE armed_timed_out=0 limit=300 deadline=300 breached=1 timed_out=1 status=Failed
      stage=0/5 pre_threat=0.0 post_threat=0.0
  (b) TIMEOUT_PS_READBACK timed_out=1 limit=0 deadline=300 mission=2,4,6-TRINITROTOLUENE stage=0/5
      (host PS observed timeout via same DOREPLIFETIME channel S2 uses - S1 target GREEN)
  (c) S2 unperturbed: SHOOT threat=8.0, MISSION_DONE stage=3/5 threat=23.0, OPP_WIN_DRIVE opp_won=1
      threat=17.0, FIRE_SYNC domain_final=22.0; MP host threat=22.0 (back in 2[0-9] band), opp_won=1,
      contesting=1, stage=3/5 cross-client.
- NOTED (not a blocker): post-breach snapshot reads `deadline` unconditionally (APBWorldService.cpp:647,
  stays 300) but `time_limit_sec` only inside `if(Current())` (L652, defaults 0 on Failed). HUD keys
  "TIMED OUT" off the timed_out flag not limit; RefreshCountdown early-returns deadline<=0. S1 pass
  condition (bMissionTimedOut==1) is independent. Arguably correct (failed stage no longer counts down).
- Editor build exit 0 (x2); probe compiles clean. clangd not installed -> compile is the type gate.
- OUT OF SCOPE (unchanged): visible HUD pixel capture needs windowed-RHI tooling (harness is -nullrhi).
- NOT MINE this session (leave in shared pile): tracked `notify.log`, `.gitignore` `.vercel` line -
  unrelated to M11, another session's in-progress work.
- STATUS: M11 slice functionally COMPLETE + gate-verified. S1 (timed_out=1) + S2 (opp_won=1) both
  live-fired cross-client/host-PS; S3 regression GREEN; apbdb timers applied + locked.

## HUD PIXEL CAPTURE — DEAD END (root cause proven; do NOT re-attempt via Python)
- User launched editor manually; PIE booted APBFrontendGameMode (login), NOT freeroam. Real mission
  HUD (APBFreeroamHUD::BeginPlay CreateWidget->AddToViewport) only auto-spawns under freeroam GameMode.
- Attempted synthetic overlay from Python. Hit 5 distinct walls, each root-caused:
  1. Occlusion — fullscreen APBFrontendWidget login art painted over the HUD even at z=30000.
  2. GC — hand-created widget (new_object) un-rooted by remove_from_parent -> reaped
     ("ObjectInstance is null"). Fix: never remove_from_parent; keep in viewport + globals().
  3. Frontend-PS access — APBFrontendPlayerController has NO get_player_state() bound; uncaught
     Python exception SILENTLY DISCARDS all stdout in this MCP bridge (looked like empty result).
     Correct route: pc.get_editor_property('player_state') -> APBPlayerState_0 (exists in frontend).
  4. Occlusion again — even after arm-first + no-GC, login art still on top; collapsed frontend_widget
     via pc.get_editor_property('frontend_widget').set_visibility(COLLAPSED) -> clean screen.
  5. ZERO-SIZE NON-COMPOSITE (terminal) — with login gone + widget confirmed inViewport=True, STILL
     paints nothing. desiredSize={0,0}. ROOT CAUSE: UUserWidget created via Python new_object never
     gets UUserWidget::Initialize() called (only engine CreateWidget / WidgetBlueprintLibrary::Create
     does that, and WidgetBlueprintLibrary is NOT bound in Python). No Initialize -> WidgetTree layout
     pass yields 0x0 -> nothing composites regardless of z-order/visibility.
- CONCLUSION: synthetic-overlay pixel capture is ARCHITECTURALLY IMPOSSIBLE from Python alone.
  A faithful pixel requires the widget to be born through the engine's CreateWidget path, i.e. PIE
  must actually run the freeroam GameMode (drive full login->char->district travel, needs world server),
  OR a windowed-RHI freeroam boot. Editor restored to clean login state; synthetic HUD removed; no GC leak.
- The PS->pixels last mile is already CODE-VERIFIED end-to-end (ApplyDomainSnapshot -> OnRep_Mission ->
  OnMissionUpdated.Broadcast -> widget OnMissionStateChanged -> RefreshMissionDisplay SetPercent/countdown).
  The widget being QA'd (APBFreeroamHUDWidget) is SHARED-PILE, not the M11 delta.

## HUD PIXEL CAPTURE - UPDATE (freeroam boot SOLVED; only capture-egress remains blocked)
- SUPERSEDES the "architecturally impossible" line above for the WIDGET-BIRTH problem: it is now SOLVED.
- FIX for login-screen boot: PIE ignores DefaultEngine.ini:7 map-prefix routing (that only applies on
  URL travel). PIE falls back to GlobalDefaultGameMode=APBFrontendGameMode (ini:6). Setting the editor
  world's WorldSettings.default_game_mode = unreal.APBFreeroamGameMode IN MEMORY (no umap save) makes PIE
  boot freeroam. Confirmed: GAMEMODE=APBFreeroamGameMode, HUD=APBFreeroamHUD, PC=APBPlayerController,
  PS=APBPlayerState_0. So APBFreeroamHUD::BeginPlay CreateWidget->AddToViewport(10) DID run -> the real,
  engine-Initialize()'d HUD widget exists (NOT the dead synthetic path). MUST restore default_game_mode
  to None at teardown (done this session).
- Real PS armed via set_editor_property on all 10 mission fields of APBPlayerState_0, then
  ps.get_editor_property('on_mission_updated').broadcast() SUCCEEDS (delegate APBMissionStateChangedDelegate)
  -> drives RefreshMissionDisplay, the SAME render path OnRep_Mission uses. TickMissionClock early-returns
  with no real mission active, so poked fields are not clobbered.
- REMAINING WALL is pure CAPTURE-EGRESS (getting the composited PIE frame to a PNG), 7 distinct approaches
  all root-caused to ONE cause: the UMG HUD exists only in the PIE viewport's final composited frame, which
  (a) no scene-capture API exposes, and (b) a hidden, engine-managed PIE DirectX window won't surface for a
  screen grab.
    1. HighResShot <res> -> scene-only PNG, strips Slate/UMG (confirmed: mesh+void, no HUD).
    2. Shot showui -> captured the EDITOR frame (1298x738), not the game viewport.
    3. GDI CopyFromScreen "largest visible window" -> grabbed editor chrome (PIE window is hidden).
    4. Synthetic widget new_object -> 0x0 non-composite (no Initialize()); DEAD, see above.
    5. PrintWindow PW_RENDERFULLCONTENT on the hidden 512x225 UnrealWindow -> black (DX swapchain).
    6. Surface hidden PIE window (ShowWindow/SetWindowPos 0,0 1280x720/foreground) + CopyFromScreen ->
       engine REFUSED the move (window stayed at L=1406 off-primary-monitor); grabbed desktop, not game.
       Artifact: work/logs/hud_contested_screen.png (desktop, NOT the HUD - negative result kept as evidence).
    7. unreal-mcp_vision capture_viewport -> "No editor world is open" during PIE; and its doc is "3D scene
       only" so it would strip UMG even on success.
- REMAINING VIABLE PATHS (need user decision - all are scope beyond the gate-verified M11 delta):
  A. Windowed standalone client (UnrealEditor.exe ... -game -windowed): a REAL top-level visible window that
     CopyFromScreen can grab with full composited UMG. Cost: lose the Python MCP arming bridge, so mission
     state must be driven through gameplay (session-probe drivers / real district travel), and freeroam needs
     its world path to resolve standalone.
  B. C++ FWidgetRenderer::DrawWidget -> RenderTarget capture wired into a dev-only console command + rebuild.
     Faithful widget pixels without the OS-window problem. Cost: C++ change + editor rebuild.
  C. Accept CODE-VERIFIED HUD binding as sufficient (PS->OnRep_Mission->Broadcast->RefreshMissionDisplay is
     verified end-to-end; widget is shared-pile, not the M11 delta) and close the slice.
- Editor restored to clean state this session: PIE stopped, default_game_mode=None, is_in_pie=false, no
  leaked game/server procs (lone CrashReportClientEditor is UE's normal idle watchdog sidecar).

## HUD PIXEL CAPTURE - SESSION 2: two more branches ELIMINATED with evidence; decision now B-vs-C only
- PIE-world screenshot route (approach #8): started freeroam PIE (WorldSettings trick), armed the REAL
  PIE-world APBPlayerState_0 with a CONTESTED state (title '2,4,6-TRINITROTOLUENE', stage 2/5, stageprog
  0.40, oppprog 0.25, contesting=1, deadline now+90s), on_mission_updated.broadcast()=ok. Then routed
  'shot showui' through get_game_world() console. RESULT: wrote ScreenShot00001.png but it is the EDITOR
  FRAME (1298x738: level viewport design-scene + Outliner + PIE toolbar), NOT the game viewport. Root
  cause CONFIRMED: PIE runs in "Play in New Window" mode -> game renders to a SEPARATE window, so every
  editor-side capture misses it. Cannot switch to "Selected Viewport" mode: LevelEditorPlaySettings /
  PlayModeType are NOT Python-exposed, and LevelEditorSubsystem offers only editor_request_begin/end_play
  (no mode selector). MCP start_pie inherits the editor's last-used mode.
- Visible-window GDI route (approaches #9,#10): the PIE game window DOES exist and is vis=True
  (h=0x760360, cls='UnrealWindow', empty title, 1280x720) BUT the engine PINS it at pos=(1406,175) and
  overrides BOTH SetWindowPos AND MoveWindow (stays at 1406, partly off primary monitor). PrintWindow
  PW_RENDERFULLCONTENT returned ok=True but the DX-swapchain body is BLACK (GDI cannot read the D3D
  backbuffer - only the Slate window-chrome maximize glyph rendered). CopyFromScreen at (1406,175) grabbed
  UNRELATED occluding desktop content (SetForegroundWindow/BringWindowToTop lost the z-order fight), NOT
  the HUD. Both misleading PNGs deleted. gdigrab/PrintWindow are GDI => same black-on-DX wall. Only
  Windows Graphics Capture (WGC) taps DWM composition to read DX content, but standing up WGC from
  PowerShell (WinRT + D3D interop) is too fragile to justify here.
- PYTHON WIDGET-RENDER PATH: PROVEN ABSENT. dir(unreal) has NO WidgetRenderer/DrawWidget, no
  KismetRenderingLibrary. RenderTarget* entries are exporters/factories only. So FWidgetRenderer requires
  C++. AutomationLibrary.take_screenshot exists but is the same PIE-viewport machinery (same wall).
- NET: 10 capture approaches, all root-caused to ONE wall (UMG lives only in a DX backbuffer that GDI
  can't read + engine won't surface the window on primary monitor + editor-side captures miss the New-
  Window game viewport + no scripting widget-render path). DECISION COLLAPSED TO:
    B. C++ FWidgetRenderer::DrawWidget -> UTextureRenderTarget2D -> PNG. Robust, faithful, no OS-window
       problem. COST: C++ into gate-complete M11 + editor build. Build needs the editor CLOSED (DLLs
       locked) OR fragile Live Coding (static console-command/CVar registration may not hot-patch).
    C. Accept CODE-VERIFIED binding (PS->OnRep_Mission->Broadcast->RefreshMissionDisplay verified E2E;
       widget is shared-pile, not the M11 delta). RECOMMENDED - cost/benefit favors C for optional polish.
- Editor left clean again this session: PIE stopped, default_game_mode=None, is_in_pie=false, no leaked
  game/server procs. Awaiting user go/no-go on B (C++ + rebuild) vs C (accept).

## HUD PIXEL CAPTURE - SESSION 3: OPTION B EXECUTED & SUCCEEDED (real-surface artifact captured)
- Path chosen: B via Live Coding (NO editor restart, NO Build.cs change). Added a .cpp-only file-scope
  FAutoConsoleCommandWithWorldAndArgs "apb.CaptureHUD [path]" to APBFreeroamHUD.cpp that renders the HUD
  UMG widget offscreen via FWidgetRenderer -> UTextureRenderTarget2D -> KismetRenderingLibrary::ExportRenderTarget.
  This sidesteps the entire DX-backbuffer/OS-window capture wall: UMG is drawn to an offscreen RT, never a
  swapchain/window.
- 4-step hypothesis-driven debug chain (each step root-caused from evidence, not shotgunned):
  1. LNK2019 x2: BeginCleanup + FlushRenderingCommands live in RenderCore (not in Build.cs). Fix WITHOUT a
     Build.cs change (which would force editor close + full rebuild): stack-allocate FWidgetRenderer (no
     BeginCleanup) and drop the explicit flush (ExportRenderTarget->ReadPixels already flushes). Link clean.
  2. Detached fresh CreateWidget -> pure black (22616 B). Root cause: this widget builds its ENTIRE tree in
     NativeConstruct (APBFreeroamHUDWidget.cpp:16-45), which never fires on a widget added to no viewport;
     even the static Hint text was absent. RebuildWidget cached an empty root.
  3. Live HudWidget (off AAPBFreeroamHUD::HudWidget, already constructed+ticked+bound) -> still black
     (byte-identical 22616 B). Root cause: a Slate widget can't live in two trees; it's already parented to
     the viewport overlay so FWidgetRenderer's SetContent reparent is refused -> paints nothing.
  4. FIX: W->RemoveFromParent() before TakeWidget (detach from viewport, keep constructed tree) + double-draw
     (warm layout/resource caches) + diagnostic dark-blue clear. -> SUCCESS. 56458 B, HUD rendered faithfully.
- VERIFIED PIXELS (D:\APBReloaded\work\logs\hud_capture_final.png, 1280x720) match armed PS EXACTLY:
  Line1 "maras-... | Criminal | Cash 10000 | G1C 5000"; Line2 "Threat 0 (bots 2) | Inv 1 | District Financial";
  MissionLine "Mission: 2,4,6-TRINITROTOLUENE  stage 2/5  [CONTESTED]  01:30" (title/stage 2of5/CONTESTED/
  90s countdown all correct); StageBar ~0.40 blue; OppBar ~0.25 orange; Hint controls line. This is the
  real-surface visual proof of the M11 HUD binding: PS fields -> OnMissionUpdated broadcast ->
  RefreshMissionDisplay/RefreshCountdown -> visible pixels.
- TEARDOWN: APBFreeroamHUD.cpp reverted to committed baseline (git diff empty); 2 intermediate black PNGs
  deleted (kept hud_capture_final.png); PIE stopped; default_game_mode=None; is_in_pie=false; final Live
  Coding compile (patch_3) so loaded code == on-disk source. CAVEAT: the static console command string
  lingers in THIS running editor session's console until next editor restart (Live Coding cannot un-register
  a static FAutoConsoleCommand from a live process) - on-disk source is clean so no restart/rebuild has it.
