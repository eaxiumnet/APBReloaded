# ACTIVE PLAN — APB Reloaded 1:1 Recreation (Master Roadmap)

Started: 2026-07-19 · Revised: 2026-07-19 (expanded from vertical-slice plan to full-roadmap
per lead-dev brief) · Status: **ACTIVE**

> Single source of truth for this effort per AGENTS.md. Companion design doc:
> `work/ARCHITECTURE.md`. Completed/abandoned plans move to `work/_archive/`.
> Hyperplan output — execution handed to ulw-loop milestone by milestone; do not freestyle.

---

## 1. Goal

Recreate APB Reloaded **1:1 wherever possible** in UE 5.8, combining:

- **2011 RTW build** (`D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\`
  — actual resolved root; the "2011 rtw version" subfolder name in the brief does not exist
  on disk) for anything superior/unique to the original: main menu (layout, art, strings,
  music, splash video), original feel.
- **Current retail build** (`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded`)
  for modern features/content: character creation, item catalogs, district content.
- Full backend: login/auth, world server, district servers, matchmaking, chat, clans,
  friends, mail, auction house, character DB, inventory, economy, missions, groups,
  vehicles, customization, dedicated-server support, anti-cheat posture.
- Fully playable **Waterfront** and **Financial**, plus Social district and the PG arenas.

### M3R R7 gate order

Source registry -> catalog provenance -> strict canonical oracle -> semantic-class parity ->
verified-row promotion -> verified-allowlist build/static audit -> runtime rejection/no-substitute
probe -> `tools\check_strict_asset_provenance.ps1` -> `bind_report` -> existing runtime gates.
Semantic proof precedes verification, verified rows precede allowlist generation, and runtime/final
gates consume those artifacts.

## 2. Non-goals (standing)

- Protocol compatibility with real 2011/retail clients (generated opcode header stays unused).
- Redistribution of licensed soundtracks (2011 `Music.pck` 161 tracks / retail 113 mp3) —
  menu music uses the already-extracted login theme WAVs in `Content\Audio\` only.
- Kernel-level anti-cheat; console platforms.
- True Golem morph extraction beyond a 2-attempt spike (D4).

## 3. Ground truth (verified 2026-07-19 — do not re-discover)

Project already implements: Domain WorldService (login/worlds/districts/customization/
threat/missions/combat/inventory/Armas/auction-in-memory), full C++ UMG frontend flow,
8 district freeroam maps + frontend map, ~5k imported uassets, placement manifests +
chunk streaming, extraction tooling (umodel, WwiseExtract, apb level exporters, 2011 UI
extractors, apbdb sync), listen-server MP proven. 2011 menus = UE3 UIScene (not Scaleform);
art in `APBMenus_*.upk` (pkg ver 547), strings in `APBUserInterface.int`, videos `.bik`,
UI sfx in Wwise banks. Retail char-create data is mostly plain-readable (palettes
`Colours\*.ini`, camera `APBLCC.ini`, INT tables); morphs are cooked (spike-only).
Gaps: no persistence, auction memory-only, mail stubbed, dedicated server never cooked,
template leftovers in module, no git (fixed in M0).

## 4. Decisions

- **D1** Fidelity passes over the existing frontend/Domain spine — no rewrites.
- **D2** 2011 menu = UMG rebuild with extracted 2011 art/strings/audio (UIScene parsing
  rejected as unbounded R&D; screenshot-measure + art parity is visually identical).
- **D3** Boot = bik→mp4 splash (Electra) → 2011-styled login over the existing SceneCapture
  studio restaged to 2011 framing; true `APBLoginLevel` 3D backdrop extraction is a stretch
  task after district `.APB` export is proven on the 547 format.
- **D4** Retail char-create: 15 wardrobe categories + retail flow/palettes/camera; morph
  spike capped at 2 attempts, hand-built morph fallback ("port behavior" contract).
- **D5** Persistence = JSON-file repositories behind interfaces in Domain (SQLite deferred;
  keeps `cl`-compiled tests green). Schemas in ARCHITECTURE.md §4.
- **D6** One dedicated binary, role by CLI (`-WorldServer` / `-District=<id>`), TCP/JSON
  control channel; gameplay stays UE replication. (APB-protocol emulation rejected.)
- **D7** Auction = bid + buyout + expiry + fee in Domain, persistence via D5, delivery via
  a real minimal MailService, UMG kiosk on district terminals.
- **D8 (revised)** District content order: **Social → Financial → Waterfront** per the
  lead-dev brief (supersedes the earlier Waterfront-first note); Social is the smallest
  district and proves the `.APB` pipeline cheapest.
- **D9** Repo hygiene first: git init + UE .gitignore, baseline commit, delete template
  leftovers (own commit), archive `work/login_swap/`.
- **D10** Chat/matchmaking/clans/groups are Domain services behind the WorldService facade;
  in-district chat over UE RPCs, cross-district via world control relay. Matchmaking =
  threat-tier opposition pairing + group queue in Domain (APB-authentic), not a separate
  process.
- **D11** Login/auth are roles of the world server process, not separate binaries
  (retail's LS/WS split is deployable later via the same role flags).
- **D12** Import ledger (`tools\import_ledger.json` → `work\IMPORT_STATUS.md`) is the
  single tracking mechanism for "imported vs. needs manual recreation".
- **D13** Single runtime module with strict subfolders (`Domain/`, `Systems/Frontend/`,
  `Systems/District/`, `Systems/Server/`, `Systems/Economy/`); module split deferred to M17.
  (Opposing view — split now — rejected: destabilizes a working build for organizational
  gain; folder discipline carries the roadmap.)
- **D14** Missions: Domain `MissionScriptLibrary` extended with district-side triggers +
  opposition dispatch; strings/templates parsed from retail `MissionTemplates.INT` /
  `TaskObjectives.INT` into `Content\Data\`.
- **D15** Vehicles: catalog-driven stats from `vehicles.json` on the existing drivable pawn;
  deep customization (paint/parts/kits) is its own milestone using retail `Colours\*.ini`
  paint grids.
- **D16** Server architecture VALIDATED (Oracle verdict 2026-07-25, High confidence; matches
  original-APB topology + modern split-server norm). The current split is correct and stays:
  (a) **engine-free `Domain/`** owns all persistent/meta authority (auth, persistence, economy,
  social, matchmaking, tickets, handoff); (b) **thin headless world-server role** stays as-is —
  its UE dependency is shallow (AGameModeBase hooks + UObject GC + socket/thread HAL) and it
  already runs headless; a full de-UE'd standalone process is a valid LATER optimization, not a
  gameplay-milestone unblocker, so DEFERRED (no rework now); (c) **UE district sim STAYS in UE**
  — a custom authoritative simulator is REJECTED for a small team (rebuilding replication +
  `UCharacterMovementComponent` semantics + Chaos collision/hit-reg + map/asset ingestion +
  actor lifecycle = a multi-year platform diversion with worse fidelity and permanent
  dual-runtime debugging). District GameModes do admission + realtime sim ONLY; meta decisions
  stay in `Domain/`. Listen-server districts are DEV-ONLY (not capacity evidence). Source-engine
  `Server` target (R1 wall) is scheduled at the deploy/perf boundary, not earlier; it does NOT
  change the architecture. Optimize (replication frequency, relevancy, actor counts, collision,
  server-only asset loading) only AFTER profiling; dedicated-server role guards to skip
  cosmetics (lights/atmosphere/PPV/preview meshes) land WITH the source-engine Server target.
- **D16b enforcement (2026-07-25):** audited `Systems/District/` for D16 clause (c) violations
  — district code directly mutating replicated meta. Found + fixed 2 leaks:
  `APBFreeroamCharacter.cpp` (weapon-fire path) and `APBDistrictGameMode.cpp` (PostLogin
  admission) were writing `AAPBPlayerState::ThreatPoints`/`Cash` directly; both now route meta
  through `UAPBGameInstanceSubsystem::SyncPlayerStateFromDomain` (ticket faction stays
  authoritative via a post-sync `ApplyFactionAuthority` re-assert). `APBDistrictGameMode.cpp`
  ticket-issuance read (:610) was already clean (read-only). Evidence: `APBReloadedEditor` Win64
  Development `Result: Succeeded` (both files compiled + DLL linked); runtime
  `tools\run_m7_handoff_gate.ps1` → `HANDOFF_GATE_OK` exit 0 with `HANDOFF_DISTRICT_PARITY ok=1`
  (replicated meta == Domain snapshot post-sync), `DISTRICT_TICKET_ADMITTED faction=Enforcer`,
  tamper path still `CHAR_HANDOFF_REJECT reason=account_mismatch`, 0 orphaned processes.
- **D16b Site-1 combat weapon-fire proof (2026-07-25):** closed the prior caveat (Site-1 was
  compile+pattern-verified only). Added a runtime differential to `APBSessionProbeSubsystem.cpp`
  client_loop probe (`FIRE_SYNC` log line) + a fail-fast gate assertion in
  `tools\run_verification_gates.ps1` (`Require-Fresh ... "FIRE_SYNC ok=1"`). Differential:
  baseline PS==Domain → `ApplyHandoffProbeMutation()` diverges Domain-only (+5 threat, no PS
  write) → fire ONLY via `AAPBFreeroamCharacter::FireWeaponLocal()` → assert PS moved off the
  stale value AND reached Domain parity. Proves `FireWeaponLocal` syncs replicated meta solely
  through the `SyncPlayerStateFromDomain` bridge (`APBFreeroamCharacter.cpp`), never a direct
  `AAPBPlayerState` write. RED→GREEN (identical editor/map/probe invocation the gate's
  client_loop step uses, `-APBProbe=client_loop` on `Lvl_APB_Financial_Freeroam?listen`,
  `-nullrhi -unattended`): with the bridge sync disabled →
  `FIRE_SYNC ok=0 mutated=1 moved_off_stale=0 parity=0 ps_after=23.0 domain_final=28.0`
  (gate `ok=1` grep would Fail); with the bridge sync restored →
  `FIRE_SYNC ok=1 mutated=1 moved_off_stale=1 parity=1 ps_after=28.0 domain_final=28.0`.
  `ps_after` flips 23→28 (tracks Domain) iff the bridge sync is present. Build: `APBReloadedEditor`
  Win64 Development `Result: Succeeded` for both RED and GREEN. Logs: RED
  `fire_sync_red\client_loop.log`, GREEN `fire_sync_green_iso\client_loop.log` (under
   `%LOCALAPPDATA%\Temp\opencode\`).
 - **D16b Site-1 full-spine gate green + stale-exe trap fixed (2026-07-25):** the isolated
   RED→GREEN above proved the behavior; this closes it end-to-end inside the real gate. Root
   cause of the prior blocker: `run_verification_gates.ps1` guarded `model_registry` with a soft
   `if (Test-Path $model)` that silently ran whatever stale `APBModelRegistryTests.exe` existed
   (mtime 2026-07-17), whose hard-coded paths `Systems\APBFreeroamGameMode.cpp` +
   `Systems\APBDistrictPlacementLoader.cpp` predated their move to `Systems\District\`; the gate
   never rebuilt it. Fix (3 parts): (1) `tests\run_model_registry_tests.cpp` L123/L136 repointed
   to `Systems\District\`; (2) new durable recipe `tools\scripts\build_model_registry_tests.ps1`
   (mirrors `build_lobby_flow.ps1`; 11-file WorldService src set + probe, compile-only exit 0);
   (3) gate now has a mandatory `model_registry_build` step feeding `model_registry` (same
   build→run pattern as `domain_tests_build→domain_tests`), replacing the soft skip with a
   fail-fast `missing $model after build`. Standalone: rebuilt exe `FAILS=0` exit 0 (12 srcs, the
   two former-failing readability checks now PASS, no cascade). Full gate
   (`tools\run_verification_gates.ps1`): **`GATE_PASS`** ~11 min, all steps green
   (`model_registry_build → model_registry` FAILS=0 → `host_client_loop`); the gate's own
   `client_loop.log` shows `FIRE_SYNC ok=1 mutated=1 moved_off_stale=1 parity=1 ps_after=28.0
   domain_final=28.0` and `gate_summary.json` records `gate=PASS`, `site1_fire_sync=FIRE_SYNC
   ok=1` — the assertion is now enforced live in-spine, not skipped. 0 orphaned editors; scratch
   cleaned. The earlier "not fixable by a source edit alone — left untouched" note is superseded
   by this entry.

## 5. Milestone roadmap

Brief order honored (1→16) with two dependency injections called out explicitly: **M2
persistence** must precede login/economy servers, and **M3 pipeline completion** must
precede menu/district content. Detail level: M0–M7 fully specced; M8+ specced at
architecture level — each gets a spec-expansion task as its first step when it becomes the
active milestone (recorded in this file, keeping one plan per effort).

### M0 — Hygiene & baseline  *(brief #1)* — ✅ COMPLETE 2026-07-19
- Files: repo root, `Source\APBReloaded\Variant_*`, template maps/`APBReloadedCharacter*`,
  `work\`.
- Actions: git init + .gitignore (`2011 apb/`, `APB Reloaded/` (7.3 GB retail clone found at
  repo root), `Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`,
  `Content/Extracted/`, `.codegraph/`, `.omo/`) + baseline commit; deleted template leftovers
  (Variant_* source, template classes, `Content\ThirdPerson`, `Content\Variant_*` incl. the
  4 template maps); moved `work/login_swap/` → `work/_archive/login_swap/`.
- Fixes landed during verification: vswhere fallback in `tests\build_and_run.ps1`; removed
  stale `Engine/SkyAtmosphere.h` include (moved into `SkyAtmosphereComponent.h` in 5.8);
  `bUseUnity=false` in Build.cs (purge regrouped unity blobs → anonymous-namespace
  collisions in Domain helpers).
- Evidence: `tests\build_and_run.ps1` FAILS=0; `APBReloadedEditor` + `APBReloaded` Win64
  Development builds Succeeded; `git log` = 5 commits (baseline, vswhere fix, template
  purge, SkyAtmosphere fix, unity fix). Note: `tools\UEViewer` + 3 archived tool dirs are
  embedded git repos (gitlinked, not content-tracked) — fine for local-only use.

### M1 — Architecture & conventions  *(brief #1)* — ✅ COMPLETE 2026-07-19
- Files: `work\ARCHITECTURE.md` (done), `Source\APBReloaded\Systems\{Frontend,District,Server}\`,
  `tools\import_ledger.json`, `tools\scripts\build_import_status.py`.
- Actions: moved 34 Systems files into `Frontend/` (14) / `District/` (20), `APBWorldGameMode`
  → `Server/`; bridge files (GameInstanceSubsystem, PlayerState, SessionProbeSubsystem) stay
  at Systems root; `PrivateIncludePaths` added for subfolders (`Economy/` deferred to M12 —
  no code yet); created ledger (seeded from on-disk reality) + read-only status generator.
- Evidence: editor + game builds Succeeded post-move; `tests\build_and_run.ps1` FAILS=0;
  `work\IMPORT_STATUS.md` renders (4,376 imported uassets; honest block coverage: Financial
  1/~270, Waterfront 1/~268 manifests). Commits: `c70b939` restructure, ledger tooling commit.

### M2 — Persistence foundation  *(injected; blocks #5/#11/#13/#14 — D5)* — ✅ COMPLETE 2026-07-19
- Files: new `Domain\APBPersistence.h/.cpp` (`apb::JsonDomainStore`), `APBWorldService.*`
  (`InitPersistence`/`SaveAllNow`/`LogoutAccount`/`SendMail`/`MarkMailRead`/`MailInbox`),
  `APBSocial.h` (real `MailService`; friends/clan stay stubbed),
  `APBGameInstanceSubsystem` (`PersistDir=<ProjectSavedDir>/DomainDB`, save on Deinitialize),
  `tests\run_persistence_tests.cpp`, `tests\build_and_run.ps1` (two suites).
- Actions: JSON-file repos per ARCHITECTURE.md §4 adapted to actual Domain types
  (deviations documented in code: wardrobe as array with colors/decal; auction/mail gain
  `next_id`; bid/fee/expiry fields deferred to M12 with the AuctionHouse extension).
  Persistence is opt-in — no persist dir ⇒ exact previous in-memory behavior.
- Expected: register → restart process → login → character/cash/inventory intact. **Proven**
  by restart-parity suite (two instances sharing a store, both factions, auction+mail parity).
- Verify: domain suite FAILS=0 + persistence suite FAILS=0 (independently re-run);
  editor + game builds Succeeded. Commit `207756c` (+ `*.obj` untracking follow-up).

### M3 — Extraction pipeline completion  *(brief #2)* — ✅ COMPLETE 2026-07-19
- **umodel 547 spike: WORKS** (1st attempt, no overrides; verdict `work\umodel_547_spike.md`)
  — the project's biggest extraction risk is retired for both installs.
- Batches (commits `9894c94`, `39aa684`, `66fa765`):
  - 2011 menu art + fonts: **77 packages → 2,085 PNG** in `Content\Extracted\2011\MenuArt\`
    (+ manifest) via new `tools\scripts\export_2011_menu_art.py`.
  - 2011 UI sfx: **72 WAV** in `Content\Extracted\2011\UISfx\` via
    `tools\scripts\extract_2011_ui_sfx.py` (bank .txt sidecar parsing + vgmstream).
  - Videos: Bink 1 `.bik` preserved; ffmpeg unavailable and UE 5.8 BinkMedia is Bink-2-only
    → re-encode (RAD/ffmpeg) deferred to M4; **menu motion backgrounds already exist as
    WebM/MKV** from prior work, so M4 is not blocked. Ledger entry `group:2011/Movies=manual`.
  - `Content\Data\palettes.json`: 7 retail palettes, 1,134 colors (`tools\convert\parse_colours.py`).
  - `Content\Data\ui_strings_{2011,retail}.json`: 510/594 keys across 8 menu sections
    (`tools\convert\parse_int_tables.py`, parameterized; 2011 INT is ASCII, retail UTF-16LE —
    parser sniffs BOM).
- Verify: ledger updated (MenuArt/UISfx `extracted`, Movies `manual`, palettes/ui_strings
  `extracted`); `work\IMPORT_STATUS.md` regenerated; JSONs validated by parse.

### M4 — 2011 main menu  *(brief #3 — D2/D3)* — ✅ COMPLETE 2026-07-20
- Files: `Systems\Frontend\APBFrontendWidget.*`, `APBFrontendLayoutMath.h`,
  `Content\Imported\UI\Menu2011\`, `Content\Audio\UI\`, `Content\Movies\`.
- Actions: restage Login/CharSelect/DistrictSelect to 2011 layout using extracted chrome +
  `[APBLoginScreen]`/`[CharacterSelectScreen]` strings + UI sfx; boot flow splash video →
  login; 2011 menu music (existing extracted theme WAVs).
- Verify: updated layout-math tests green; `-APBProbe=frontend_menu` passes (terminal
  `FRONTEND_MENU_OK`, self-exits) — menu-scoped gate independent of M9 geometry / M12 vehicles;
  composite `frontend_flow` held as M9+M12 integration gate (`run_verification_gates.ps1 -IntegrationGate`);
  side-by-side screenshot fidelity checklist vs 2011 capture (`work\menu2011_fidelity_checklist.md`) signed off.
- Evidence (2026-07-20): CharSelect+DistrictSelect 2011 restage committed `af40e5e`.
  Menu-scoped `frontend_menu` probe mode added (terminal `FRONTEND_MENU_OK` after all UI
  stages + district select + OpenLevel dispatch, then `RequestEngineExit`); `frontend_flow`
  retained as the M9/M12 integration gate. Probe commits `123e23e` (mode split + terminal
  hygiene decls), `013353b` (single-terminal-verdict enforcement: no_freeroam fail-return,
  per-manager timer clears, bTerminal symmetry). Gate spine `605fce1`
  (`run_verification_gates.ps1`: frontend_menu hard-gated w/ Require-Fresh, frontend_flow
  behind `-IntegrationGate`). Docs `e91f934` (HOW_TO_PLAY both modes).
  Both targets build exit 0. Runtime: `frontend_menu` self-exits 19s exit 0, 21-line log
  ending `FRONTEND_MENU_OK`, zero FAIL, no zombie; `frontend_flow` self-exits 27s exit 0,
  30-line log, exactly one terminal `FRONTEND_FLOW_FAIL post_travel_playables` (expected
  M9/M12 baseline: vehicles=0/walk=0), no double-FAIL fall-through, no zombie. Visual
  fidelity checkboxes intentionally left for optional manual capture per spec §0.

### M5 — Retail character creation  *(brief #4 — D4)* — ✅ COMPLETE 2026-07-20
- Files: `Domain\APBTypes.h`, `Domain\APBCatalog.cpp`, `Domain\APBCustomization.*`,
  `Content\Data\{wardrobe_categories.json (new), clothing.json, palettes.json}`,
  `Systems\APBGameInstanceSubsystem.*`, `Systems\Frontend\APBCharacterCreatePreviewActor.*`,
  `Systems\Frontend\APBFrontendWidget.*`, `tests\run_fidelity_tests.cpp`, `tests\build_and_run.ps1`.
- Actions delivered: 7 → **15 retail wardrobe categories** (new `wardrobe_categories.json`
  maps each tab to a distinct domain equip-slot: 7 original + underwear/outerwear/dress/
  jewellery/belt/webbing/armour/bodyhair; all 2,051 `clothing.json` items tagged
  `wardrobe_tab`). Domain `SymbolLayer` + `symbols[]` with 3rd-pipe serialize/deserialize
  (legacy 1-pipe blobs still decode) and `Randomize(faction,seed)` deterministic fill.
  UE bridges (`GetClothingForTab`/`GetSlotForTab`/`EquipClothingColored`/`RandomizeAppearance`/
  `GetPaletteColors`/`AddSymbolLayer`/`GetSymbolLayerCount`/`GetCameraFrameForTab`). Preview
  `FrameCamera` per `APBLCC.ini` (retail default 280/95/95/55, per-tab framing from JSON).
  CharCreate widget rebuilt to 15-tab UI + 24-swatch palette grid + RANDOMIZE + symbol stub.
  Tattoos/symbols: data model + UI stub this milestone; full symbol-editor depth → M17.
- **Morph spike (D4): abandoned after 2 attempts** — Golem facial morph is cooked-only
  (UE3 `.upk`), 0/4,499 imported assets carry morph data, `APBLCC.ini` has only
  `GolemSpawnerActor`. Fallback = `ApplyBodyProfile` height/bulk + discrete SkinTone/
  FacePreset selectors in Domain (verified present). Note: `work\morph_spike.md`. Continuous
  morph reopens in M17 only with a hand-authored UE5 morph-target set (AGENTS.md rule 5 honored).
- Verify (evidence 2026-07-20): fidelity suite green — `tests\build_and_run.ps1` all 3 suites
  `FAILS=0` (M5 coverage: 15-tab table locked + cross-checked vs `wardrobe_categories.json`
  for drift, symbol round-trip + legacy compat, 15 independent slots survive restart,
  deterministic randomize hits every tab). `APBReloadedEditor` + `APBReloaded` Win64
  Development builds `Result: Succeeded` (server target unsupported by this engine
  distribution — cook scheduled M6, not an M5 regression). `frontend_menu` probe self-exits
  exit 0, terminal `FRONTEND_MENU_OK`, `CHAR_CREATE ok=1`, `APPEARANCE slots_equipped=7
  required=7` (no M4 regression from the widget rebuild). Commits: `e6379e8` data model +
  clothing tags, `0f929b4` domain symbols/randomize/15-slot + fidelity tests, `f0d43d3` UE
  bridges + camera framing, `5622047` 15-tab widget UI, `224bec2` morph spike note. Retail
  `[CharacterCustomisationScreens]` screenshot checklist left for optional manual capture
  (same posture as M4 visual sign-off).

### M6 — Login/auth + world server  *(brief #5 — D6/D11)*  — ✅ COMPLETE 2026-07-20

> Authored by the `plan` agent from a 5-member adversarial bundle (minimalist / architect /
> hardcore / maverick / deep-research; 3 cross-attack rounds). Decision-complete: execute with
> zero further interview. Ordered single-concern commits C1–C13 in parallel groups A–G.

**RESOLVED DECISIONS (honor these; they survived cross-attack):**
- **R1 build blocker = HARD WALL.** Installed `BaseEngine.ini` `InstalledPlatforms` list Editor+Game
  only (no `PlatformType="Server"`); UBT rejects `TargetType.Server` at `UEBuildTarget.cs`. This box
  can NEVER cook `APBReloadedServer` without a source engine. → Run the world-server **role** on the
  **Game target** headless (`-WorldServer -nullrhi -nosound -unattended`); role keyed off the CLI flag
  (NOT `IsRunningDedicatedServer()`). KEEP `APBReloadedServer.Target.cs` untouched for a future source
  cook. Document, don't fake. Marking M6 "blocked" is rejected.
- **R2 transport.** M6 client↔world login / char-select / district-list are **served by the world
  authority to the 2 connected clients over UE's own NetDriver** (Server RPCs + OnRep — the path the
  probe already exercises), replacing the in-process path for those flows. NO bespoke client-facing TCP
  socket in M6. D6's "TCP/JSON control channel" = world↔district relay, **deferred to M7**.
- **R3 auth.** PRIMARY defect = **authorization gating** (Domain mutations gated by authenticated
  session identity; two clients on one authority must not cross-contaminate) — ranks ABOVE password
  KDF. ALSO replace plaintext with a salted KDF that compiles in pure-C++17 Domain with NO new deps
  (PBKDF2-HMAC-SHA256, header-only). Ticket = HMAC-SHA256 envelope + `jti` + short expiry (~90s) +
  one-use replay cache + constant-time verify, M2-persistable. Ed25519/asymmetric = M7. TLS on
  localhost = DEFERRED to M16.
- **R4 scope kills.** Behavioral-oracle (running `APBprivate.exe`) OUT of M6 (32-bit native + .NET +
  MySQL; impractical; zero gate value) — note only. No protocol emulation (`APBPrivateServerOpcodes.h`
  stays unused). Deferred to M7: world↔district TCP/JSON relay, cross-district travel + ticket
  redemption, chat. M6 only ISSUES + stores + self-verifies tickets (no district-side consumption).

**Verify gate (milestone):** world-server role process + 2 client processes; both complete
login → char-select → district-list **served by the world authority**. Extend
`tools\run_verification_gates.ps1` (step 7) — 1 headless world-server + 2 client procs. Plus: 4 domain
suites `FAILS=0`; `APBReloaded` + `APBReloadedEditor` build exit 0; `APBReloadedServer` expected-fail
documented (not a regression).

**Parallel groups:** A = {C1, C2} · B = {C3} · C = {C4, C5} (both after C3) · D = {C6→C7→C8}
(after C4+C5) · E = {C9, C11} (after C8) · F = {C10 after C9; C12 after C11} · G = {C13, after all}.

- **C1 — `work/m6_server_target_limit.md`** (doc). Record the installed-engine Server-target wall
  (`InstalledBuild.txt` + `BaseEngine.ini` Editor/Game-only `InstalledPlatforms`; UBT throw site), the
  role-flag resolution, and the M16 source-engine path. *Verify:* file states root cause + decision.
- **C2 — `tests/run_auth_tests.cpp`** (RED first). 5 tests locking the API: salted-hash stored (not
  plaintext, 16-byte hex salt); login hashed OK/wrong/banned; legacy-plaintext migrate-on-login;
  ticket issue→verify field parity + tamper-fail; ticket replay blocked after `ConsumeJti`.
  *Verify:* `cl /std:c++17` compiles the suite (link may fail until C3–C5).
- **C3 — `Domain/APBCrypto.h`** (header-only, pure C++17, no platform headers). `sha256` (FIPS
  180-4), `hmac_sha256` (RFC 2104), `pbkdf2_hmac_sha256` (RFC 2898; iters=10000, dk=32),
  `hmac_sha256_hex`, `hex_encode/decode`, `random_hex` (`random_device`+`mt19937_64`; M16 →
  `BCryptGenRandom`). *Verify:* smoke TU compiles + links, exit 0.
- **C4 — `Domain/APBTicket.{h,cpp}`** (after C3). `TicketPayload{account,character,faction,district,
  jti,issued_utc,expiry_secs}`; `TicketService::Global()` singleton; `IssueTicket(...)`→`payload.sig`
  HMAC token; `VerifyTicket` (constant-time HMAC compare + expiry + jti-unseen); `ConsumeJti` one-use
  `unordered_set` (M7 → JsonDomainStore). Add to `build_and_run.ps1` sources. *Verify:* C2 ticket tests
  GREEN.
- **C5 — auth hardening** `Domain/APBSocial.h` + `Domain/APBPersistence.{h,cpp}` + `tests/build_and_run.ps1`
  (after C3). Add `AccountRecord.password_salt`; `Register` salts+PBKDF2-hashes; `Login` re-derives +
  constant-time compares, `salt==""` ⇒ legacy plaintext compare then migrate-on-success; persist/load
  `password_salt` (empty default = back-compat); add 4th test suite `APBAuthTests`. *Verify:* all 4
  suites `FAILS=0` (existing register→login domain/persistence tests still pass — API unchanged).
- **C6 — `Systems/Server/APBWorldGameMode.{h,cpp}`** (fill shell; after C4+C5). Per-connection
  `TMap<FString,TUniquePtr<apb::WorldService>> PlayerServices` (key = PC unique id) — **the
  multi-tenancy fix**: each client gets its OWN authenticated WorldService, no shared `session`.
  `BeginPlay` creates `ServerControl`; `PostLogin` inits a per-player service from data+persist dir;
  `Logout` saves + erases. *Verify:* `APBReloaded` + `APBReloadedEditor` build exit 0.
- **C7 — `Systems/Server/APBServerControl.{h,cpp}` + `Config/DefaultGame.ini`** (after C6). `UObject`
  owning role selection (`FParse::Param(-WorldServer)`) + the auth-gated serving surface:
  `LoginRequest`, `GetCharListJson`, `GetDistrictListJson`, `IssueTicketJson` (each resolves the
  caller's per-player service; ticket issue requires `IsLoggedIn`). World-role activates via
  `?game=/Script/APBReloaded.AAPBWorldGameMode` URL (no GameModeMapPrefixes clash). Add `[APBServer]`
  `WorldPort=17778`, `WorldPersistDir`. *Verify:* build exit 0; `-WorldServer` detected.
- **C8 — `Systems/APBPlayerState.{h,cpp}`** (after C7). 4 `Server,Reliable,WithValidation` RPCs
  (`Server_LoginRequest/GetCharList/GetDistrictList/IssueTicket`) dispatching to
  `GetAuthGameMode<AAPBWorldGameMode>()->ServerControl`; 4 `ReplicatedUsing` fields
  (`bWorldAuthOk`, `CharListJson`, `DistrictListJson`, `IssuedTicketJson`) with `OnRep_*` logging;
  register in `GetLifetimeReplicatedProps`; validate inputs (non-empty, len caps). *Verify:* build exit 0.
- **C9 — `Systems/APBGameInstanceSubsystem.{h,cpp}`** (after C8; parallel w/ C11). Add
  `bWorldServerMode` + `ConnectToWorldServer(host,port)` + `IsWorldServerConnected` +
  `GetIssuedTicket`. `Initialize` reads `-WorldServerHost`; `Login`/`GetDistrictList` route through the
  local `APBPlayerState` Server RPCs when `NM_Client && bWorldServerMode`, else existing in-process
  path UNCHANGED (no `frontend_menu` regression). *Verify:* build exit 0; standalone probe unaffected.
- **C10 — `Systems/Frontend/APBFrontendWidget.cpp`** (after C9). Async world-server UX: `OnLoginClicked`
  fires `Server_LoginRequest` + polls `bWorldAuthOk` (timer, 10s timeout) → advance to CharacterSelect
  (reuses existing `SetStage`); `OnEnterDistrict` appends `?APBTicket=<token>`. Standalone paths
  untouched. *Verify:* build exit 0; `frontend_menu` probe still `FRONTEND_MENU_OK` (same code path).
- **C11 — `Systems/APBSessionProbeSubsystem.{h,cpp}`** (after C8; parallel w/ C9). Two modes:
  `world_server` (authority: counts PlayerStates reaching auth/charlist/districtlist/ticket == 2, then
  writes `WORLD_SERVER_GATE_OK login=N ... ticket=N` + exits) and `world_server_client`
  (`-WSClientId=<id>`: drives login→charlist→districtlist→ticket via Server RPCs, writes
  `WORLD_CLIENT_OK ... id=<id>` + exits). *Verify:* build exit 0.
- **C12 — `tools/run_verification_gates.ps1`** (after C11). Insert step 7 `world_server_gate`: launch 1
  headless world-server (`?game=...AAPBWorldGameMode -WorldServer -Port=17778 -APBProbe=world_server`) +
  2 clients (`127.0.0.1:17778 -APBProbe=world_server_client -WSClientId=alice|bob`); wait ≤180s for
  `WORLD_SERVER_GATE_OK`; `Require-Fresh` asserts `login=2` and `ticket=2`; add to `gate_summary.json`.
  *Verify:* `pwsh` syntax-parses; step runs.
- **C13 — Final milestone verify** (no file changes). 4 domain suites `FAILS=0`; `APBReloaded` +
  `APBReloadedEditor` builds `Result: Succeeded`; `APBReloadedServer` expected-fail logged as
  `server_target_unsupported_by_distribution` (matches C1, not a regression); `run_verification_gates.ps1`
  all 7 steps `GATE_PASS`.

**C13 RESULT — ✅ M6 functionally PROVEN 2026-07-20** (evidence: `work/m6_world_gate_findings.md`
+ `work/m6_world_gate_debug.md`):
- 4 domain suites `FAILS=0` (incl. `APBAuthTests`); `APBReloaded` + `APBReloadedEditor` Win64
  Development `Result: Succeeded`; `APBReloadedServer` expected-fail (installed-engine wall, = C1).
- **`world_server_gate` GREEN**: isolated `tools/run_m6_world_gate.ps1` →
  `WORLD_SERVER_GATE_OK login=2 charlist=2 districtlist=2 ticket=2` / `M6_WORLD_GATE_PASS`; both
  clients (alice+bob) reached `WORLD_CLIENT_OK ... ticket=1`. End-to-end login→char-select→
  district-list→HMAC-ticket served by the world authority over UE NetDriver to 2 clients.
- Three root causes resolved: (1) runner GameMode class typo `AAPBWorldGameMode`→`APBWorldGameMode`
  (silent fallback to `APBFrontendGameMode` caused `login=0`); (2) authority register-or-login
  provisioning + ticket-on-`IsLoggedIn` in `APBWorldGameMode.cpp`; (3) probe harness ticket-overlap
  race fixed by client-linger in `APBSessionProbeSubsystem.cpp` (test code only — product untouched).
  Converged with parallel agent **Sisyphus** (who fixed the official gate URL + scoped the race).
- CAVEAT (honest): the C13 "all 7 steps GATE_PASS" clause on the FULL spine cannot be met yet —
  steps 0–6 (`bind_report`, `VEHICLE_DOMAIN`, threat parity, `PLAYABLE_WALK/DRIVE`) depend on M9/M12
  content not built. That is a cross-milestone dependency, **not** an M6 defect; M6's own deliverable
  (step 7) is proven. Fixes left uncommitted for a coordinated single-concern commit (shared worktree).


**Risks & mitigations.** (1) *KDF pulls a dep the `cl`-compiled harness can't take* → C3 is header-only
pure C++17 (no OpenSSL/bcrypt); if PBKDF2 self-test fails after 2 attempts, STOP + `work/` note
(AGENTS.md rule 5), fall back to a documented salted-SHA256×N (still not plaintext, flag M16). (2)
*Client connect flow / RPC timing flakiness* → generous probe timeouts + `ForceNetUpdate`; if the
two-client gate loops >2 attempts, STOP + note rather than retry-tune. (3) *Widget async login
regressing `frontend_menu`* → standalone path left byte-for-byte unchanged; world path gated behind
`bWorldServerMode`. **Assumptions made (unresolved by bundle):** ticket JSON is a compact
`payload.signature` string (not full JWT); per-connection WorldService (not one shared) is the
multi-tenancy fix the auth decision implies; `?game=` URL override is preferred over a
GameModeMapPrefixes edit to avoid clashing with the existing frontend GameMode mapping.

### M7 — District servers + travel + chat baseline  *(brief #6 — D6/D10)*
- Files: `APBDistrictGameMode`, `APBServerControl` (register/heartbeat/ticket/char.handoff/
  chat.relay), `Systems\Frontend` district-select travel, new chat widget + Domain ChatService.
- Actions: district role boots freeroam map, registers to world; client travel with ticket;
  in-district + cross-district chat.
- Verify: world + 1 district + 2 clients travel in, replicate (PlayerState OnRep real
  values), chat both scopes; `tools\DEDICATED_SERVER_GAP.md` updated (gap closed).
- **Progress (Qoder):** N1 (ports) + N5 (Domain `ChatService`) DONE (see `m7_spec.md`).
  **N4 Domain half DONE + tested** — pure-C++17 W↔D relay control-message codec
  (`Source/APBReloaded/Domain/APBRelayProtocol.{h,cpp}`; verbs
  register/register_ack/heartbeat/report_load/expect(ASK_DISTRICT_EXPECT)/expect_ack/
  chat.relay/join/leave; line-delimited JSON + `DecodeStream` de-framer for an `FSocket`
  recv loop); `tests/run_relay_tests.cpp` (see `work/m7_relay_protocol_note.md`).
  **All 11 domain suites green (`FAILS=0`).**
  **N7 gate DONE** — `tools\run_m7_gate.ps1` compositor (5 legs: travel/ticket/handoff/
  chat/relay) wired REQUIRED into `run_verification_gates.ps1` (L340–351, summary L366);
  real end-to-end run GREEN → `M7_TRAVEL_GATE_OK`, exit 0, LEAKED=0 (evidence
  `work\logs\m7_travel_gate_20260724-151619.terminal.log`; see
  `work\m7_travel_gate_findings.md`). REMAINING (server-side, Sisyphus): N2 PreLogin
  ticket redeem, N3 travel dispatch, N4 `FSocket` transport wiring, N6 chat RPC.

### M8 — Social district  *(brief #7 — D8)*
- Spec-expansion first step. Scope: `RWorldSocialDistrict` content gap-closure via
  pipeline + ledger; social-space fixtures (kiosks, terminals, music studio stubs);
  `Lvl_APB_Social_Freeroam` probe green. Verify: bind report clean for Social.

### M9 — Financial District  *(brief #8)*
- Spec-expansion first step. Scope: geometry/prop gap closure (270 blocks, manifests
  exist), lighting/collision pass, `-APBProbe=playable` on `Lvl_APB_Financial_Freeroam`.
  Verify: gate spine Financial variant exit 0; no cube placeholders in bind report.

### M10 — Waterfront  *(brief #9)*
- Same shape as M9 for `Lvl_APB_Waterfront_Freeroam` (268 blocks). Verify: both district
  gates green.

### M11 — Mission systems live  *(brief #10 — D14)*
- Spec-expansion first step. Scope: INT-table mission template import; district-side stage
  triggers on existing interactables/zones; opposition dispatch via Domain matchmaking
  (threat-tier pairing); mission HUD. Verify: scripted 2-client mission probe end-to-end.
- **Progress (Qoder):** Domain matchmaking brain DONE + tested — `Source/APBReloaded/Domain/APBMatchmaking.{h,cpp}`,
  `tests/run_matchmaking_tests.cpp` ($exe12, all 12 suites FAILS=0). `Matchmaker` does
  threat-tier cross-faction opposition pairing, group-as-atomic-unit queueing, and
  wait-widening tier tolerance (deterministic, caller-supplied now_ms). Ready for the UE
  district-side N-work: opposition dispatch + mission stage triggers on top of this.
  See `work/m11_matchmaking_note.md`.
- **Progress (Qoder) — opposed-mission score race in the WorldService mission loop:** the
  contested state was previously cosmetic (`opposition_contesting` flag + takeout-only fail).
  Added the real APB symmetric race: `MissionStageRuntime.opp_progress`, `MissionRun`'s
  `opposition_won` + `AdvanceOpposition(amount)` (opposition accrues on the current contested
  stage; reaching the objective first → mission `Failed`, `opposition_won=true`), and
  `WorldService::AdvanceOpposition` (scales by `OppositionPressure()`, applies `ApplyMissionFail`
  + logs `MISSION_OPPOSITION_WON`). `DomainSnapshot` now exposes
  `mission_opposition_contesting`/`mission_opposition_won`/`mission_stage_progress`/
  `mission_opp_stage_progress` (owner/opposition current-stage fractions) for HUD sync.
  `TestOppositionRace` added to `run_domain_tests.cpp` — all 17 domain suites FAILS=0 (owner can
  still win if faster; a completed mission can't be flipped). Additive/merge-friendly (new fields
  default-off; `Progress()` unchanged). Remaining UE-side: drive `AdvanceOpposition` from the
  district GameMode opposed-mission ticker + HUD race bar. See `work/m11_opposition_race_note.md`.
- **Progress (Qoder) — mission stage countdown timers (APB stage clocks):** `time_limit_sec` was
  parsed from mission JSON but never enforced (dead data). Added deterministic, caller-clocked
  stage timers: `MissionRun::CheckTimeout(now_sec)` arms the current stage's `time_limit_sec`
  lazily on first tick and fails the mission (`timed_out=true`) once the deadline passes;
  `WorldService::TickMission(now_sec)` orchestrates it (applies `ApplyMissionFail` + logs
  `MISSION_TIMEOUT`). `DomainSnapshot` exposes `mission_timed_out` +
  `mission_stage_time_limit_sec` for the HUD countdown. Zero changes to `Start`/`Progress`/
  `AdvanceMission` signatures (timer arms itself from the tick clock). `TestMissionStageTimeout`
  added — all 17 domain suites FAILS=0 (arm→within-window→expire fails; no-limit stage never
  times out). Remaining UE-side: feed `TickMission` from the district GameMode clock + bind the
  countdown to the mission HUD. See `work/m11_stage_timeout_note.md`.
- **Hyperplan synthesis RESOLVED (2026-07-25) — UE-side execution plan:**
  - **D1 Opposition drive:** HYBRID — 0.05/s ambient × OppositionPressure in GameMode Tick +
    Contact::Interact AdvanceOpposition(1.0) bump. S2 testable in this slice.
  - **D2 Stage trigger:** Extend existing `AAPBInteractable::Interact` Contact path. No new
    zone actor. `AAPBMissionZone` deferred.
  - **D3 HUD:** New `UAPBMissionHUDWidget` (2 UProgressBar + countdown). Bound via
    `OnRep_Mission` delegate. No NativeTick poll. Existing `UAPBFreeroamHUDWidget` unchanged.
  - **D4 Matchmaker owner:** `AAPBFreeroamGameMode` member. `FormMatches` in Tick ~5s cadence.
  - **G1:** Add 7th replicated field `mission_stage_deadline_server_sec` (absolute server time).
    Client countdown = `max(0, deadline - GameState->GetServerWorldTimeSeconds())`.
  - **G2:** All 7 fields on `AAPBPlayerState` (TECH DEBT: migrate race fields to
    `AAPBMissionActor`/GameState when group missions land).
  - **G4/RISK-2:** `HasAuthority()` guard added at top of Contact case in `APBInteractable.cpp`
    in Wave 1 — in scope.
  - **G5/RISK-5:** `FAPBDomainSnapshotUE` + `FAPBMissionSnapshotUE` struct + all 4 call sites
    (PostLogin/ApplyRelayHandoff/RunClientLoopProbe/FireWeaponLocal) updated atomically in
    Wave 1 single commit.
  - **Wave order:** W1 snapshot transport + PS replication → W2 GameMode ticker + authority
    guard → W3 HUD widget → W4 probe gate.
  - See `work/m11_ue_side_notepad.md` for full wave task list + verification contract.

### M12 — Marketplace & auction house  *(brief #11 — D7)*
- Files: `Domain\APBAuction.*`, MailService in `APBSocial.h`, new
  `Systems\Economy\APBAuctionWidget.*`, `AAPBInteractable` terminals, auction domain tests.
- Actions: bid/buyout/expiry/5% fee/cancel; world-owned listings with district routing
  (ARCHITECTURE.md §3); mail delivery of items/cash; kiosk UI on retail `[Marketplace]`
  strings.
- Verify: two accounts trade across a world restart; tests green.
- **Progress (Qoder):** Domain marketplace logic DONE + tested + restart-durable —
  `Domain/APBAuction.{h,cpp}` now does bid/buyout/expiry/**5% fee**/cancel with item+cash
  settlement via `MailService`; `APBPersistence.cpp` SaveAuction/LoadAuction extended to the
  full §4 schema (start_price/current_bid/bidder/fee_paid/created/expires/state).
  `tests/run_auction_tests.cpp` ($exe13, all 13 suites FAILS=0). Remaining: UE Economy
  widgets + kiosk, `auction.sync` world routing + expiry tick. See `work/m12_auction_note.md`.
- **Progress (Qoder) — server-side wiring DONE (2026-07-29):** world authority owns
  auction.json — `WorldService::InitAuctionPersistence` (dedicated `auction_store`,
  `auction.mail` wired) + `auction_write_enabled` single-writer guard on per-connection
  services; `AAPBWorldGameMode` loads/saves the store and drives `SettleExpired` on a 5 s
  Tick cadence (`AUCTION_SETTLED`); new `AuctionHouse::BuyoutViaMail` settles offline
  sellers via mailbox. Client surface: UGI `AuctionListItemAuth/Bid/Buyout/CancelListing`
  (SocialSvc + CharacterOwnerSvc resolution) + `AAPBPlayerState::Server_Auction` RPC riding
  the social dispatch/relay path (`auction.*` branches in `DispatchSocialOpDirect` and
  `HandleSocialRequest`). Editor build BUILD_EXIT=0; auction suite +
  `TestBuyoutViaMailSettlesOfflineSeller`, persistence suite + Instance L (authority
  ownership, write guard, restart round-trip), all suites FAILS=0. Remaining: Economy
  widgets/kiosk + district browse snapshot. See `work/m12_auction_note.md`.

### M13 — Vehicle systems  *(brief #12 — D15)*
- Spec-expansion first step. Scope: catalog stats/handling per `vehicles.json`; spawn
  kiosks; paint grids from retail `Colours\StandardPaint/PearlescentPaint.ini`; damage
  states. Verify: drive + customize + persist across district rejoin.
- **Progress (Qoder):** Domain layer landed. `Domain/APBVehicle.{h,cpp}` — `VehicleCatalog`
  (parses `vehicles.json`: class/faction/min_rating/max_speed/accel/health, no invented
  fields), `CanSpawnVehicle` kiosk gate (unknown_vehicle/wrong_faction/rating_too_low),
  `VehicleInstance` damage state machine (Pristine/Damaged/Critical/Destroyed at 60%/25%/0%
  + speed-factor handling degradation, ApplyDamage/Repair clamps). `AvailableTo(faction,
  rating)` for kiosk offer lists. Tests `tests/run_vehicle_tests.cpp` ($exe14, all 14 suites
  FAILS=0; real vehicles.json parsed). Remaining: UE spawn kiosks, paint grids from
  `Colours\*.ini`, physics/driving + damage-state VFX, persist across rejoin. See
  `work/m13_vehicle_note.md`.

### M14 — Clans, friends, groups, mail UI  *(brief #13 — D10)*
- Files: Domain `ClanService`/`GroupService`, `clans.json`/`social.json`, UMG social panel.
- Verify: clan create/invite/promote; friends add/see-presence (via world relay); group
  invite → shared mission queue (M11 hook). Domain tests + 2-client probe.
- **Progress (GPT-5.8):** Domain `GroupService` (party) DONE + tested — `Source/APBReloaded/Domain/APBGroup.{h,cpp}`,
  `tests/run_group_tests.cpp` (see `work/m14_group_service_note.md`). Domain `ClanService`
  (ranks/perms/invites/MOTD, single-faction, leader-can't-leave) DONE + tested —
  `Source/APBReloaded/Domain/APBClan.{h,cpp}`, `tests/run_clan_tests.cpp` (see
  `work/m14_clan_service_note.md`). Clan JSON persistence (`SaveJson`/`LoadJson`) DONE.
  Domain `FriendsService` (mutual friend requests, ignore/block, presence, JSON persistence)
  DONE + tested — `Source/APBReloaded/Domain/APBFriends.{h,cpp}`, `tests/run_friend_tests.cpp`
  (see `work/m14_friends_service_note.md`). File-backed persistence DONE — `SocialStore`
  (`Source/APBReloaded/Domain/APBSocialStore.{h,cpp}`) writes/reads `clans.json`/`friends.json`
  via the services' `SaveJson`/`LoadJson`; `tests/run_social_store_tests.cpp` (see
  `work/m14_social_store_note.md`). Mail attachment **claim ("Take All") + delete** with
  `claimed`-flag persistence added to `MailService` (`APBSocial.h`) + `JsonDomainStore` mail
  round-trip; `tests/run_mail_tests.cpp` (see `work/m14_mail_note.md`). **All 10 domain suites
  green (`FAILS=0`); the M14 social Domain (Group/Clan/Friends + on-disk store + mail verbs)
  is complete.**
- **Progress (Sisyphus) — UE-side waves 0-2 landed + verified.** Plan: `work/m14_ue_wiring_plan.md`.
  Evidence for every item below: `tests/build_and_run.ps1` 18/18 `FAILS=0` **and**
  `Build.bat APBReloadedEditor` `Result: Succeeded` exit 0, re-run independently after each wave.
  - **W0 social ownership + single authority.** `apb::WorldService` now owns `clans`/`friends_svc`/
    `groups`/`social_store` beside the existing `mail` (facade-respecting per `Domain/AGENTS.md`);
    the 4 social `.cpp` were added to `$srcs` in `tests/build_and_run.ps1` (safe: the `exe6`-`exe9`
    social suites use explicit file lists, so no duplicate symbols). Social persistence is
    **opt-in and isolated**: new `InitSocialPersistence(dir)` (appends `/social`, loads
    clans+friends) and `SaveSocialNow()`. THE cross-player authority is a single
    `apb::WorldService SocialAuthority` on `AAPBWorldGameMode` (public `Social()` accessors),
    initialised in `BeginPlay`, flushed in `EndPlay`.
  - **Defect found + fixed during W0:** social load/save was first placed inside
    `InitPersistence`/`SaveAllNow`, but `AAPBWorldGameMode` allocates one `apb::WorldService`
    **per connection** (`PlayerServices`, `PostLogin`) and calls `SaveAllNow` per `Logout` — so
    every connection would have loaded clans at login and written a stale snapshot back on
    logout (lost update / split-brain). The authority deliberately does **not** call
    `InitPersistence`, so it never becomes a second writer to `accounts.json`. Pinned by a new
    `run_persistence_tests.cpp` "Instance J" regression (asserts a player-service
    `InitPersistence` leaves `social_store` **inactive**, plus a restart round-trip).
  - **W1 read path + TDD baseline.** 4 `USTRUCT`s (`FAPBFriendEntryUE`, `FAPBClanInfoUE`,
    `FAPBMailMessageUE`, `FAPBGroupInfoUE`); 7 replicated `AAPBPlayerState` fields behind a
    shared `OnRep_Social` (`ClanId`, `ClanRole`, `GroupId`, `OnlineFriendCount`,
    `bHasPendingClanInvite`, `bHasPendingGroupInvite`, `bGroupAllReady`) — clients read social
    state by **replication**, never client-local Domain queries (`CanMutateDomain()` is false on
    `NM_Client`). New probe mode `-APBProbe=social_probe -SocialRole=alice|bob` landed as a
    deliberate **RED** baseline (`SOCIAL_PROBE_FAIL reason=not_implemented`, per-role log file).
  - **W2 bridge.** 39 `UAPBGameInstanceSubsystem` UFUNCTIONs (clan 11+1, friends 6+5, group 9+1,
    mail 6), all routed through a `SocialSvc()` helper that resolves the world GameMode's single
    authority — verified no bridge touches per-connection `PlayerServices`. Clan invite derives
    the invitee's faction **server-side** from the connected `AAPBPlayerState` and fails closed
    (`SOCIAL_CLAN_INVITE_FAIL reason=invitee_not_online`); the client never supplies faction.
    Mail mutations enforce `msg->to == character` before mutating and return typed
    `EAPBMailResult{Ok,NotOwner,NotFound,AlreadyClaimed,Unclaimed,GrantFailed}`; claim
    pre-checks so a non-grantable path is never marked claimed, and delete surfaces the retail
    "Take All first" refusal as `Unclaimed`.
- **BLOCKER (Sisyphus, open) — social across the world/district process split.** The M7 gate
  proves each district is a **separate `UnrealEditor.exe`** from the world server (`Start-District`
  uses its own `-Port` and phones home via `-RelayHost/-RelayPort`), so: (a) the planned chat seam
  (read `SocialAuthority` from `AAPBFreeroamGameMode` and call the district's
  `ChatService::SetClan/SetGroup`) is **not implementable as written** — the authority is in
  another process; and (b) `SocialSvc()`'s fallback means social calls made **inside a district
  process** would silently hit that process's own empty social state (false green / lost writes),
  which is exactly the path the planned in-district social widget would use. The only cross-process
  channel is the HMAC-signed `apb::RelayCodec` TCP protocol (`ChatRelay`, `PlayerJoined`,
  `PlayerLeft`, `Handoff`, `Return`, …). Architecture decision in flight before T13/T15 proceed;
  relay presence (`friends_svc.SetOnline` in `MarkRelayPlayerJoined/Left`) is world-side and
  believed unaffected. REMAINING: this decision, then Server RPCs + relay presence + chat seam,
  2-client probe GREEN, UMG social panel, `run_m14_social_gate.ps1` + spine wiring.
- **Progress (Qoder) — M14 gate GREEN (2026-07-29):** the blocker above was resolved via the
  relay protocol (Server RPCs + `SocialRequest`/`SocialResult` relay + chat seam + UMG panel,
  commit `3d432bb` era) and the 2-client gate now PASSES end-to-end:
  `tools\run_m14_social_gate.ps1` → **`M14_SOCIAL_GATE_PASS`** — alice (listen world host)
  echo-confirms all six ops (clan.create/invite, friend.request, group.create/invite,
  mail.send); bob (pure network client, replicated-state driven) logs
  `SOCIAL_CLAN_JOINED clan=ClanProbe`, `SOCIAL_FRIEND_CONFIRMED online=1`,
  `SOCIAL_GROUP_JOINED group=GRP-1`, `SOCIAL_MAIL_RECEIVED unread=1`. Fixes required:
  LoginPlayer identity binding + auto character creation (mail anti-spoof), MailUnreadCount
  replication, `Client_SocialResult` echo fields + direct-path gate markers, single-flight
  probe (domain AlreadyInvited guards forbid blind resends). Gate script must set
  APB_DEPLOYMENT_SECRET, purge `Saved\DomainDB\social`, and launch alice before bob.
  Regression: `M11_MISSION_GATE_OK` re-verified; domain suite FAILS=0. Evidence + remaining
  relay-path coverage gap: `work/m14_social_gate_findings.md`.

### M15 — Economy & progression  *(brief #14)*
- Scope: role progression from `roles.json` + retail `PlayerRoles.INT`/`ContactLevels.INT`;
  contact standing; unlocks; threat rewards per `threat_table.json` multipliers; cash sinks
  (auction fees already in). Verify: progression domain tests; apbdb-parity spot checks.
- **Progress (Qoder):** Domain layer landed. `Domain/APBProgression.{h,cpp}` —
  `ProgressionCatalog` (parses real `contacts_lore.json` + `roles.json`; district derived
  from contact-id prefix), `LevelLadder` (standing→contact level 0..15, tunable default
  thresholds), `CharacterProgress` (per-char contact_standing + role_xp maps, server-auth),
  `ComputeMissionReward` (cash+standing scaled by `ThreatSystem` reward_multiplier, role xp
  flat), contact-level-gated `IsUnlocked`/`UnlockedItems`, and `TrySpend` cash-sink primitive.
  Reuses existing `APBThreat.h` for tier multipliers (no re-derivation). Tests
  `tests/run_progression_tests.cpp` ($exe15, all 15 suites FAILS=0; real contacts/roles
  parsed). Remaining: real per-level standing thresholds (cooked SDD binary, not in the INT);
  contact/kiosk UMG. `PlayerRoles.INT` full role roster DONE + `ContactLevels.INT` real
  per-contact level counts DONE (see the two bullets below). See
  `work/m15_progression_note.md`.
- **Progress (Qoder) — CharacterProgress persistence:** `JsonDomainStore`
  (`Domain/APBPersistence.{h,cpp}`) now round-trips per-character M15 progression via a
  sidecar `characters/<account>_<slot>_progress.json` — `ProgressPath`, `HasProgress`,
  `SaveProgress`, `LoadProgress`. Additive only (no change to `SaveCharacter` signature →
  merge-friendly); both `contact_standing` + `role_xp` maps emitted sorted-by-key for
  deterministic output; `LoadProgress` tolerates a missing file (fresh char = empty progress,
  returns false) exactly like the accounts/characters/auction/mail loads. `APBProgression.cpp`
  added to `$srcs` in `tests/build_and_run.ps1` so it links into $exe/$exe2/$exe3/$exe4.
  Covered by `run_persistence_tests.cpp` "Instance E" round-trip (save→fresh-load parity,
  unknown-key defaults-0, tolerate-missing). All 17 suites FAILS=0. See
  `work/m15_progression_note.md`.
- **Progress (Qoder) — real retail contact level counts:** new extraction tool
  `tools/scripts/extract_contact_levels.ps1` parses the retail `ContactLevels.INT` (1339-line
  localization mirror of the SDD table `ContactLevel`) → emits `Content/Data/contact_levels.json`
  (73 contacts, top-level array matching the `contacts_lore.json` convention so the Domain
  `JsonSplitObjects` parser reads it). Real per-contact level counts recovered (e.g.
  CriminalDefault=10, Binky=3, Financial_C07=15, Financial_C11=20, seasonal/organisation=1) —
  replaces the one-size invented 0..15 ladder. `ProgressionCatalog` extended:
  `contact_max_level` map + `LoadContactLevels*`, `ContactMaxLevel`, `LadderForContact`
  (ladder sized to the contact's real max level), `LevelLadder::ContactLadderWithMaxLevel`, and
  `NormalizeContactId` (strips zero-padding so lore `Financial_C1` resolves to `Financial_C01`).
  `WorldService` gained a `progression` member loaded in `InitFromDataDir`; the snapshot's
  `active_contact_level` now uses the per-contact ladder. Test `TestContactLevelsFromRetail`
  in `run_domain_tests.cpp`. NOTE: numeric per-level standing *thresholds* are NOT in the INT
  (cooked SDD binary) — those ladder thresholds remain tunable recreation defaults, only the
  level *count* per contact is now real. All 17 suites FAILS=0. See
  `work/m15_contact_levels_note.md`.
- **Progress (Qoder) — full retail role roster:** new extraction tool
  `tools/scripts/extract_player_roles.ps1` parses the retail `PlayerRoles.INT` (494-line
  localization mirror of the SDD table `PlayerRoles`) → emits `Content/Data/player_roles.json`
  (243 roles, top-level array; `PlayerRoles_<id>_DisplayName`/`_Description` keys → `{id,name,
  description,source}`; the SDD in-text newline glyph U+21B5 + control chars collapsed to a
  single space). This is the COMPLETE shipped roster with canonical display names (incl. all 13
  `Role2_*` activity/weapon tracks, e.g. `Role2_CrimArson`→"Arsonist", `Role2_Crim_Hacking`→
  "Black-Hat") vs the ~20 partial apbdb-seeded `roles.json`. `RoleDef` gained a `description`
  field; `LoadRolesFromText` now parses it; `WorldService::InitFromDataDir` loads
  `player_roles.json` AFTER `roles.json` so the retail-canonical entries merge on top (merge-by-id,
  retail wins) and the INIT log gained a `roles=` token. Test `TestPlayerRolesFromRetail` in
  `run_domain_tests.cpp`. NOTE: numeric role-XP thresholds/rewards are cooked SDD binary (not in
  the INT) — only the roster + names/descriptions are real here. All 17 suites FAILS=0. See
  `work/m15_player_roles_note.md`.
- **Progress (Qoder) — canonical retail mission titles (D14):** new extraction tool
  `tools/scripts/extract_mission_templates.ps1` parses the retail `MissionTemplates.INT` (219-line
  mirror of the SDD table `MissionTemplate`) → emits `Content/Data/mission_templates.json`
  (213 titles, top-level array `{id,title,source}` keyed by template id, e.g. `DB_BCS4_Del1`→
  "PIMP MY CRIB", `AE_BCS0_Ter1_B`→"GANGLAND ANNEXATION"). The extractor decodes `ConvertTo-Json`'s
  `\uXXXX` escapes (except `"`/`\`) so apostrophe titles like "YOU'RE FIRED!" round-trip through the
  Domain's naive `JStr` parser. New Domain `MissionTitleCatalog` (`APBMission.{h,cpp}`:
  `titles` map + `LoadFromJson*`/`Find`/`TitleFor`/`Count`); `WorldService` gained a
  `mission_titles` member loaded in `InitFromDataDir` (INIT log `mission_titles=` token). Test
  `TestMissionTemplatesFromRetail` in `run_domain_tests.cpp`. NOTE: template-id↔mission-script-id
  matching (to apply canonical titles onto `MissionScriptDef`s) + `TaskObjectives.INT` objective
  strings remain open. All 17 suites FAILS=0. See `work/m15_mission_templates_note.md`.
- **Progress (Qoder) — template-id↔script-id matching (D14):** verified the MissionTemplate and
  mission-script id spaces are identical (all 40 `missions.json` script ids match a template id);
  `MissionTitleCatalog::ApplyTo(MissionScriptLibrary&)` now stamps canonical retail titles onto
  loaded `MissionScriptDef`s in `InitFromDataDir` (INIT log `titled_scripts=` token). Synthetic
  `APB_Script_*` demo scripts are exempt. All 17 suites FAILS=0.
- **Progress (Qoder) — per-stage mission briefs (D14):** new `tools/scripts/extract_task_objectives.ps1`
  parses the retail `TaskObjectives.INT` (1944-line mirror of SDD table `TaskObjective`) → emits
  `Content/Data/task_objectives.json` (907 stage-brief rows / 211 templates; flat array
  `{id,template_id,stage,owner_brief,dispatch_brief,source}`; `<Col: StageText>` markup verbatim,
  `\uXXXX`-decoded). New Domain `MissionBrief`/`MissionBriefCatalog` (`Find`/`ForTemplate` stage-ordered);
  `WorldService.mission_briefs` loaded in `InitFromDataDir` (INIT log `mission_briefs=` token). Test
  `TestTaskObjectivesFromRetail` (join integrity: all 211 brief templates resolve to a canonical title).
  NOTE: `TaskOperations.INT` UIDescriptions + attaching briefs onto live `MissionRun` stages remain open.
  All 17 suites FAILS=0. See `work/m15_task_objectives_note.md`.
- **Progress (Qoder) — per-operation UI labels (D14):** new `tools/scripts/extract_task_operations.ps1`
  parses the retail `TaskOperations.INT` (mirror of SDD table `TaskOperation`) →
  `Content/Data/task_operations.json` (318 ops / 35 distinct labels; flat array
  `{id,ui_description,source}`; placeholder ops skipped; `\uXXXX`-decoded). New Domain
  `MissionOperationCatalog` (`Find`/`LabelFor`); `WorldService.mission_ops` loaded in
  `InitFromDataDir` (INIT log `mission_ops=` token). Test `TestTaskOperationsFromRetail`
  (anchors "Graffiti Target"/"Checkpoint"/"Escape!", anti-mangling, WorldService end-to-end).
  NOTE: stage→op-id link (needed to attach labels onto live `MissionRun` stages) lives in the
  cooked SDD tables, not the INT mirrors — still open. All 17 suites FAILS=0.
  See `work/m15_task_operations_note.md`.
- **Progress (Qoder) — mission result-reason end-screen messages (D14):** new
  `tools/scripts/extract_mission_result_reasons.ps1` parses the retail
  `MissionResultReasons.INT` (mirror of SDD table `MissionResultReason`) →
  `Content/Data/mission_result_reasons.json` (17 reasons; flat array
  `{id,win_message,lose_message,draw_message,source}`; None/all-empty skipped;
  `\uXXXX`-decoded so apostrophes round-trip). New Domain `MissionResultReasonCatalog`
  (`Find`/`WinMessage`/`LoseMessage`/`DrawMessage`); `WorldService.mission_result_reasons`
  loaded in `InitFromDataDir` (INIT log `mission_result_reasons=` token). Test
  `TestMissionResultReasonsFromRetail` (anchors TimedOut/WonFinalObjective/CompletedUnopposed,
  apostrophe round-trip, anti-mangling, WorldService end-to-end). All 17 suites FAILS=0.
  See `work/m15_mission_result_reasons_note.md`.
- **Progress (Qoder) — matchmaking threat-rating tiers + district-join gating (D14):** new
  `tools/scripts/extract_threat_ratings.ps1` parses the retail `ThreatLevels.INT` (mirror of
  SDD table `ThreatLevel`) → `Content/Data/threat_ratings.json` (5 tiers: In Training/Green/
  Bronze/Silver/Gold; flat array `{id,displayed_name,allowed_district_threats,rank,source}`).
  New header-only Domain `ThreatRatingCatalog` in `APBThreat.h` (`Find`/`FindByDisplayedName`/
  `DisplayedName`/`AllowedDistrictThreats`/`CanJoinDistrictThreat` matchmaking gate);
  `WorldService.threat_ratings` loaded in `InitFromDataDir` (INIT log `threat_ratings=` token).
  Test `TestThreatRatingsFromRetail` (display anchors, district-join gate Bronze->Green ok /
  Silver->Green blocked / Gold->Silver ok, WorldService end-to-end). DISTINCT from `ThreatSystem`
  notoriety/prestige heat. All 17 suites FAILS=0. See `work/m15_threat_ratings_note.md`.
- **Progress (Qoder) — faction-selection screen content (display names + lore) (D14):** new
  `tools/scripts/extract_factions.ps1` parses the retail `Factions.INT` (mirror of SDD table
  `Faction`) -> `Content/Data/factions.json` (3 rows: None/General Info, Enforcer, Criminal;
  flat array `{id,display_name,info_title,info_description,rank,source}`; DNT `Both` skipped;
  U+21B5 paragraph markers preserved as `\n\n`). NEW header-only Domain `FactionInfoCatalog`
  in `APBFactionInfo.h` (`Find`/`ForFaction`/`GeneralInfo`/`Description`/`ParagraphCount`) —
  the first Domain catalog with a PROPER JSON string unescaper (`\n`/`\"`/`\uXXXX`) so the
  multi-paragraph lore round-trips 1:1; `WorldService.faction_info` loaded in `InitFromDataDir`
  (INIT log `factions=` token). Test `TestFactionInfoFromRetail` (display/title anchors, enum
  mapping, lore content anchors, apostrophe + paragraph-break round-trip, WorldService
  end-to-end). All 17 suites FAILS=0. See `work/m15_faction_info_note.md`.
- **Progress (Qoder) — notoriety/prestige heat-level HUD descriptions (D14):** the apbdb
  `/heat` descriptions in `threat_table.json` (verbatim mirror of retail `HeatLevels.INT`)
  are now threaded through the Domain: `ThreatTier` gains a `description` field parsed by
  `ParseTierArray` (was previously discarded), and `DomainSnapshot` gains
  `threat_level`/`threat_tier_name`/`threat_tier_description` populated in `CaptureSnapshot`
  from `threat.CurrentTier()` so the HUD heat widget can show the authentic notoriety
  (N0-N5) / prestige (P0-P5) blurb. No new extraction — threat_table.json already carried
  the text 1:1. Test `TestHeatLevelDescriptionsFromRetail` (N0/N5 + P0 anchors, every tier
  non-empty, snapshot end-to-end). All 17 suites FAILS=0. See
  `work/m15_heat_level_descriptions_note.md`.
- **Progress (Qoder) — organisation catalog (contact gangs / Joker vendors / Armas stores):**
  extracted retail `Organisations.INT` (mirror of SDD table `Organisation`) via
  `tools/scripts/extract_organisations.ps1` -> `Content/Data/organisations.json` (19 rows,
  empty `None` skipped). Names verbatim from the INT; `faction` (Criminal/Enforcer/None) and
  `kind` (gang/default/seasonal/vendor/store/tutorial) classified from canonical APB fact
  since the SDD `Organisation.Faction` column is cooked away. New header-only
  `APBOrganisations.h` `OrganisationCatalog` (Find/Name/ForFaction/OfKind/Count*, merge-by-id,
  rank-sorted), wired into `WorldService.organisations` with an `organisations=` INIT token.
  This is the authoritative list the Armas store filters + contact UI group by. Test
  `TestOrganisationsFromRetail` (names, faction/kind, 6+6+7 grouping, 8 gangs/3 stores, rank
  order, end-to-end). All 17 suites FAILS=0. See `work/m15_organisations_note.md`.
- **Progress (Qoder) — medal / award catalog (kill streaks, mission wins, dishonours):**
  extracted retail `Medals.INT` (mirror of SDD table `Medal`) via
  `tools/scripts/extract_medals.ps1` -> `Content/Data/medals.json` (82 rows, empty `None`
  skipped). Titles + human-readable unlock criteria verbatim from the INT; `category` derived
  from the id's first token (KillStreak/BigWin/Dishonour/Situational/TimeLimit/KillBehind),
  39 of which are negative "Demerit" dishonours (`IsDishonour()`). New header-only
  `APBMedals.h` `MedalCatalog` (Find/Title/Description/ForCategory/Categories/Count*/Dishonours,
  merge-by-id, order-sorted), wired into `WorldService.medals` with a `medals=` INIT token.
  This is the source the post-mission award popup + profile achievements page read from. Test
  `TestMedalsFromRetail` (count, apostrophe round-trip "Kill 'Em All", category classification,
  39 dishonours, 6 categories, order sort, end-to-end). All 17 suites FAILS=0. See
  `work/m15_medals_note.md`.
- **Progress (Qoder) — street-name catalog (world-map / minimap + mission waypoint labels):**
  extracted retail `StreetName.INT` (mirror of SDD table `StreetName`) via
  `tools/scripts/extract_street_names.ps1` -> `Content/Data/street_names.json` (191 rows).
  Names verbatim from the INT (accents + `&` join labels preserved, no stray `\u`); `district`
  (Financial 84 / Waterfront 107) and `kind` (street 78 / intersection 113, keys with `_X_`)
  derived from the key. Retail typo key `Financia_X_...` (missing trailing 'l') still classified
  Financial. New header-only `APBStreetNames.h` `StreetNameCatalog`
  (Find/Name/ForDistrict/OfKind/Districts/Count*, merge-by-id, order-sorted), wired into
  `WorldService.street_names` with a `street_names=` INIT token. This is the source the HUD
  minimap + mission waypoint callouts read from. Test `TestStreetNamesFromRetail` (count,
  ampersand + accent round-trip, kind/district classification, typo key, order sort,
  end-to-end). All 17 suites FAILS=0. See `work/m15_street_names_note.md`.
- **Progress (Qoder) — ammunition-category catalog (weapon ammo pools + HUD ammo counter):**
  extracted retail `AmmoCategories.INT` (mirror of SDD table `AmmoCategories`) via
  `tools/scripts/extract_ammo_categories.ps1` -> `Content/Data/ammo_categories.json` (24 rows;
  25th `Blowtorch_Fuel` dropped for empty name, `None` no-ammo sentinel kept). Name /
  NameAbbreviated / QuantityText / Description verbatim from the INT; the `<Num>` counter token
  is preserved (no stray `\u`). New header-only `APBAmmoCategories.h` `AmmoCategoryCatalog`
  (Find/Name/Abbreviated/QuantityText/Description/Count, merge-by-id, order-sorted) plus
  `FormatQuantity(id,n)` that does the live `<Num>`->count HUD substitution
  (`FormatQuantity("Rifle",30)`->"30 rounds"), wired into `WorldService.ammo_categories` with an
  `ammo_categories=` INIT token. This is the source the HUD ammo counter + mod screen read from.
  Test `TestAmmoCategoriesFromRetail` (count, verbatim fields, None sentinel, dropped empty row,
  FormatQuantity, `<Num>` preservation, order sort, end-to-end). All 17 suites FAILS=0. See
  `work/m15_ammo_categories_note.md`.
- **Progress (Qoder) — scoreboard-column tooltip catalog (post-mission + chaos scoreboard):**
  extracted retail `ScoreboardDescriptions.INT` (mirror of SDD table `ScoreboardDescription`) via
  `tools/scripts/extract_scoreboard_descriptions.ps1` -> `Content/Data/scoreboard_descriptions.json`
  (22 columns, no drops). Column id + tooltip verbatim from the INT (apostrophe round-trips, no
  stray `\u`). New header-only `APBScoreboardDescriptions.h` `ScoreboardDescriptionCatalog`
  (Find/DisplayText/Count, merge-by-id, order-sorted), wired into
  `WorldService.scoreboard_descriptions` with a `scoreboard_descriptions=` INIT token. This is the
  source the scoreboard UI reads column hover text from. Test `TestScoreboardDescriptionsFromRetail`
  (count, verbatim tooltips, apostrophe, Premium columns, order sort, end-to-end). All 17 suites
  FAILS=0. See `work/m15_scoreboard_descriptions_note.md`.
- **Progress (Qoder) — HUD combat score-feed catalog (floating kill/objective/medal toasts):**
  extracted retail `HUDCombatMessages.INT` (mirror of SDD table `HUDCombatMessage`) via
  `tools/scripts/extract_hud_combat_messages.ps1` -> `Content/Data/hud_combat_messages.json`
  (145 rows; 4 both-empty Easter placeholders dropped, one-empty-line entries kept). Two-line
  messages (`line0` token/label + `line2` message) verbatim from the INT; tokens
  `<CharacterNameA>`/`<MedalName>`/`<GameplayObject>`/`($<Score>)` preserved (no stray `\u`). New
  header-only `APBHUDCombatMessages.h` `HUDCombatMessageCatalog` (Find/Line0/Line2/Count,
  merge-by-id, order-sorted) plus `FormatLine0/FormatLine2(id,value)` single-token substitution,
  wired into `WorldService.hud_combat_messages` with a `hud_combat_messages=` INIT token. This is
  the source the combat score feed reads its toast text from. Test `TestHUDCombatMessagesFromRetail`
  (count, verbatim lines, tokens, `($<Score>)`, demerit, empty-line handling, dropped placeholder,
  FormatLine substitution, order sort, end-to-end). All 17 suites FAILS=0. See
  `work/m15_hud_combat_messages_note.md`.
- **Progress (Qoder) — Modification-effect tooltip catalog (character/vehicle/weapon/consumable mod
  descriptions):** extracted retail `ModifierEffects.INT` (mirror of SDD table `ModifierEffect`) via
  `tools/scripts/extract_modifier_effects.ps1` -> `Content/Data/modifier_effects.json` (163 rows;
  scattered `_Description`/`_Description_N` keys reassembled per mod, all-empty placeholder mods
  dropped). Lines verbatim including inline `<Color:R=.. G=.. B=..>` markup and retail quirks
  (malformed no-`>` tag, `</Col>` closers, swapped channels, leading-space tag) preserved (no stray
  `\u`). New header-only `APBModifierEffects.h` `ModifierEffectCatalog`
  (Find/Lines/LineCount/PlainLines/ForCategory/Categories/Count, merge-by-id, order-sorted) with a
  markup-aware `ParseSegments`/`PlainText` parser that turns a raw line into coloured text runs.
  Wired into `WorldService.modifier_effects` with a `modifier_effects=` INIT token. Closes the
  long-flagged #1 open INT gap. Test `TestModifierEffectsFromRetail` (count, 4 categories + per-cat
  counts, verbatim + multi-line reassembly, ParseSegments colour runs, PlainText strip, malformed
  no-`>` literal, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_modifier_effects_note.md`. Follow-up: bind Armas/inventory mod items to their
  `modifier_effects` id when the modification-screen UI is built.
- **Progress (Qoder) — Modification-item catalog (purchasable/equippable mods -> effect binding):**
  extracted retail `ModifierItemTypes.INT` (mirror of SDD table `ModifierItemType`) via
  `tools/scripts/extract_modifier_item_types.ps1` -> `Content/Data/modifier_item_types.json`
  (284 rows; value split on U+21B5 into `type_label` + `description`, trailing ` -` stripped, empty
  rows dropped, `Mod_None`/`Mod_Vacant` kept as `Special`). New header-only `APBModifierItemTypes.h`
  `ModifierItemTypeCatalog` (Find/TypeLabel/Description/Category/ForCategory/Categories/Count,
  merge-by-id, order-sorted) with a **`static EffectId(id)`** helper that maps an item id to its
  `modifier_effects` row (strip `FnMod_`/`FNMod_` prefix + trailing `_Tutorial`; 138/284 bind
  directly). Wired into `WorldService.modifier_item_types` with a `modifier_item_types=` INIT token.
  This closes the ModifierEffects follow-up: item card text + stat tooltip now join up. Test
  `TestModifierItemTypesFromRetail` (count, 4 categories + per-cat counts, label/desc split, verbatim
  flavour, apostrophe/no-`\u`, flavour-only items, EffectId + cross-catalog resolve, order sort,
  missing-id safety, end-to-end). All 17 suites FAILS=0. See `work/m15_modifier_item_types_note.md`.
  Follow-up: explicit item->effect alias map for the ~146 renamed/sub-effect items when the mod UI
  is wired.
- **Progress (Qoder) — Role-milestone catalog (per-rank role progression titles + reward mail):**
  extracted retail `RoleMilestones.INT` (mirror of SDD table `RoleMilestones`) via
`tools/scripts/extract_role_milestones.ps1` -> `Content/Data/role_milestones.json` (705 rows; the
  three keys `_Title`/`_RewardMailSubject`/`_RewardMailBody` grouped per milestone id, U+21B5
  line-breaks collapsed to spaces, all-empty rows dropped; 705 titles, 21 subjects, 100 bodies). New
  header-only `APBRoleMilestones.h` `RoleMilestoneCatalog` (Find/Title/RewardSubject/RewardBody/
  HasReward/ForRole/Count, merge-by-id, order-sorted) with **`static RoleId(id)`/`Rank(id)`** helpers
  that split the trailing `_<NN>` rank so a milestone binds back to a `player_roles` id (309/705 bind
  directly; `ForRole` groups + sorts a role's ranks). Wired into `WorldService.role_milestones` with a
  `role_milestones=` INIT token. Test `TestRoleMilestonesFromRetail` (count, verbatim titles, RoleId/
  Rank/RoleIdFor/RankFor, ForRole ascending, reward body verbatim + U+21B5 collapse, apostrophe/no-`\u`,
  order sort, missing-id safety, end-to-end incl. `progression.FindRole` binding). All 17 suites
  FAILS=0. See `work/m15_role_milestones_note.md`. Follow-up: surface milestone Title/reward text in
  the role-progression UI + tie milestone completion to the reward-mail system when that lands.
- **Progress (Qoder) — HUD-message catalog (on-screen notifications/prompts/error banners):**
  extracted retail `HUDMessages.INT` (mirror of SDD table `HUDMessage`) via
`tools/scripts/extract_hud_messages.ps1` -> `Content/Data/hud_messages.json` (882 rows; the two keys
  `_DisplayText`/`_ChatText` grouped per id, U+21B5 -> real newline, all-empty rows dropped; 867
  display texts, 168 chat texts, 240 with `<col:>` spans, 278 with `<Token>` placeholders). This is
  the broad HUD banner system (mission events, deliveries, contact-standing, error banners) — distinct
  from the combat score-feed (`hud_combat_messages`). New header-only `APBHUDMessages.h`
  `HUDMessageCatalog` (Find/DisplayText/ChatText/PlainDisplayText/FormatDisplay/Count, merge-by-id,
  order-sorted) with **`static StripColor(text)`** (drops `<col:...>`/`</col>` wrappers, keeps
  substitution tokens) + **`static Format(text,token,value)`** (fills a named `<Token>`; repeat for
  multi-token banners). Markup + tokens kept VERBATIM. Wired into `WorldService.hud_messages` with a
  `hud_messages=` INIT token. Test `TestHUDMessagesFromRetail` (count, verbatim colour-span + token
  banners, no-`\u`, StripColor removes wrappers/keeps tokens, FormatDisplay single + repeated-Format
  two-token, chat text, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_hud_messages_note.md`. Follow-up: resolve `<col:NAME>` names against the HUD colour
  palette + wire the banner queue in the UMG HUD when it lands.
- **Progress (Qoder) — reward-package display catalog (rewards UI / reward-mail body text):**
  extracted retail `RewardPackages.INT` (mirror of SDD table `RewardPackages`) via
`tools/scripts/extract_reward_packages.ps1` -> `Content/Data/reward_packages.json` (1661 rows; the
  two keys `_Description`/`_OutOfSeasonDescription` grouped per package id, all-empty rows dropped
  from 3281 -> 1661; OutOfSeasonDescription is empty for every id in the current retail build, kept
  for event rotation). This is the DISPLAY half of the reward system — the player-facing prose for
  the bundles achievements/role-milestones/missions/challenges/seasonal-events/Armas grant (the item
  payload lives in the cooked SDD, resolved separately). New header-only `APBRewardPackages.h`
  `RewardPackageCatalog` (Find/Description/OutOfSeasonDescription/HasDescription/DescriptionFor(id,
  outOfSeason)/ForCategory/Count, merge-by-id, order-sorted) with **`static Category(id)`** (family
  token before first `_`: 20 families Weapon/Symbol/Crim/Enf/Clothing/Title/Vehicle/...). Prose kept
  VERBATIM (apostrophes, quotes, `APB$` survive; no `\u`). Wired into `WorldService.reward_packages`
  with a `reward_packages=` INIT token. Test `TestRewardPackagesFromRetail` (count 1661, verbatim
  desc + apostrophe + `APB$`, no-`\u`, OOS empty + DescriptionFor fallback, HasDescription, Category
  family + no-underscore, ForCategory, None dropped, order sort, missing-id safety, end-to-end). All
  17 suites FAILS=0. See `work/m15_reward_packages_note.md`. Follow-up: port the item PAYLOAD table
  (WeightedRewards.INT / RedeemableRewards.INT) keyed by the same package id so each package gains a
  `contents` list; then tie milestone/mission completion -> reward mail using `Description`.
- **Progress (Qoder) — weighted-reward mail catalog (reward-mail subject/body):** extracted retail
  `WeightedRewards.INT` (mirror of SDD table `WeightedRewards`) via
`tools/scripts/extract_weighted_rewards.ps1` -> `Content/Data/weighted_rewards.json` (189 rows; the
  four keys `_RewardMailSubject`/`_RewardMailBody`/`_OutOfSeasonSubject`/`_OutOfSeasonBody` grouped per
  reward id, all-empty rows dropped from 1277 -> 189; the big E_*/C_* Enforcer/Criminal pools are empty
  placeholders; OOS pair empty in current retail, kept for event rotation; 189 subjects, 132 bodies).
  This is the MAIL-BODY half of the reward system (bio/organisation lore, weapon/consumable/deployable
  reward mails, minigame + legendary drops, seasonal grants) — counterpart to the reward-package
  DISPLAY descriptions. New header-only `APBWeightedRewards.h` `WeightedRewardCatalog` (Find/
  RewardSubject/RewardBody/OutOfSeasonSubject/OutOfSeasonBody/HasReward/MailSubjectFor(id,outOfSeason)/
  MailBodyFor(id,outOfSeason)/ForCategory/Count, merge-by-id, order-sorted) with **`static Category(id)`**
  (family token: Bio/Consumable/Legendary/Deployable/...). Prose kept VERBATIM (apostrophes survive; no
  `\u`); U+21B5 paragraph breaks -> real `\n`. Wired into `WorldService.weighted_rewards` with a
  `weighted_rewards=` INIT token. Test `TestWeightedRewardsFromRetail` (count 189, verbatim subject +
  body + apostrophe + newline, no-`\u`, OOS empty + MailSubjectFor/MailBodyFor fallback, HasReward,
  Category + ForCategory, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_weighted_rewards_note.md`. Follow-up: adjacent `RedeemableRewards.INT` (1474 player-choice
  reward mails) + `RewardPackageItemTypes.INT` (142) follow the same recipe; wire both reward catalogs
  into the mail path when it lands; item PAYLOAD (contents list) is still a separate SDD/apbdb increment.
- **Progress (Qoder) — redeemable-reward mail catalog (player-choice confirmation mails):** extracted
  retail `RedeemableRewards.INT` (mirror of SDD table `RedeemableRewards`) via
  `tools/scripts/extract_redeemable_rewards.ps1` -> `Content/Data/redeemable_rewards.json` (1471 rows;
  the two keys `_MailSubject`/`_MailBody` grouped per reward id, both-empty rows dropped from 1474 -> 1471;
  1471 subjects, 370 bodies; many Leased/preset ids are subject-only). This is the PLAYER-CHOICE half of
  the reward-mail system (Retail/Leased weapon presets, clothing, titles, weapon skins, vehicles, emotes,
  bundles) — completes the reward-mail text trio alongside `weighted_rewards` (standalone grants) +
  `reward_packages` (rewards-UI blurbs). New header-only `APBRedeemableRewards.h` `RedeemableRewardCatalog`
  (Find/MailSubject/MailBody/HasReward/HasBody/ForCategory/Count, merge-by-id, order-sorted) with
  **`static Category(id)`** (family token: Weapon(353)/Clothing(253)/Title(161)/WeaponSkin(116)/
  Vehicle(74)/Armas(67)/Emote(50)/...). Prose kept VERBATIM (90 bodies with apostrophes survive; no `\u`);
  U+21B5 paragraph breaks (72 bodies) -> real `\n`. Wired into `WorldService.redeemable_rewards` with a
  `redeemable_rewards=` INIT token. Test `TestRedeemableRewardsFromRetail` (count 1471, verbatim subject +
  body + newline, no-`\u`, subject-only HasBody split, HasReward, Category + ForCategory, order sort,
  missing-id safety, end-to-end). All 17 suites FAILS=0. See `work/m15_redeemable_rewards_note.md`.
  Follow-up: only `RewardPackageItemTypes.INT` (142 per-item component mails) remains of the reward-text
  INTs; item PAYLOAD (contents list) is still a separate SDD/apbdb increment.
- **Progress (Qoder) — reward-package item-type catalog (per-component descriptions + mails):** extracted
  retail `RewardPackageItemTypes.INT` (mirror of SDD table `RewardPackageItemTypes`) via
  `tools/scripts/extract_reward_item_types.ps1` -> `Content/Data/reward_item_types.json` (139 rows; the
  three keys `_Description`/`_MailSubject`/`_MailBody` grouped per item id, all-empty rows dropped from
  142 -> 139; 125 descriptions, 61 subjects, 43 bodies). Per-component entries of reward packages
  (vehicle customization kits, clothing/outfit/title/weapon-skin components) — the per-ITEM layer beneath
  `reward_packages` blurbs + the `weighted_rewards`/`redeemable_rewards` mail catalogs. **Completes the
  reward-text INT family** (all four reward-text tables now in Domain). New header-only
  `APBRewardItemTypes.h` `RewardItemTypeCatalog` (Find/Description/MailSubject/MailBody/HasDescription/
  HasMail/ForCategory/Count, merge-by-id, order-sorted) with **`static Category(id)`** returning the
  SECOND token (all ids share the `RewardPackage_` prefix: Clothing(32)/Title(21)/Christmas(19)/
  Outfit(12)/Components(11)/...). Prose kept VERBATIM (embedded double-quotes round-trip through JSON
  `\"`; no `\u`); U+21B5 paragraph breaks (33 rows) -> real `\n`. Wired into
  `WorldService.reward_item_types` with a `reward_item_types=` INIT token. Test
  `TestRewardItemTypesFromRetail` (count 139, verbatim description + quoted subject + body newline,
  no-`\u`, desc-only HasMail split, Category second-token + ForCategory, order sort, missing-id safety,
  end-to-end). All 17 suites FAILS=0. See `work/m15_reward_item_types_note.md`. Follow-up: the reward-text
  INTs are now fully ported; the only remaining reward work is the PAYLOAD increment (attach a `contents`
  list of item ids + counts + lease flags per reward id from cooked SDD / apbdb.com) so completion can
  grant real items using these text catalogs as the display/mail wrapper.
- **Data (Qoder) — master inventory item-type dictionary ported (InventoryItemTypes.INT):** extracted the
  largest retail INT (mirror of SDD table `InventoryItemTypes`, 26470 kv lines) via
  `tools/scripts/extract_inventory_item_types.ps1` -> `Content/Data/inventory_item_types.json` (12997 rows;
  13235 distinct ids, 238 placeholder None/Vacant ids with empty DisplayName dropped). Header-only
  `APBInventoryItemTypes.h` `InventoryItemTypeCatalog` (`Find/DisplayName/CreatorName/HasDisplayName/
  ForCategory/Count`, merge-by-id, order-sorted) backed by an internal `unordered_map<id,index>` for O(1)
  Find over ~13k rows; `static Category(id)` = first token (`Mod`/`Reward`/`Weapon`/...). Each row keeps the
  item's `CreatorName` (Reloaded Productions / Little Orbit / community creators) for author-credit fidelity.
  Prose VERBATIM (no `\u`); ids may contain double underscores (`Equipment__None`) — suffix anchor handles it.
  Wired into `WorldService.inventory_item_types` with an `inventory_item_types=` INIT token. Test
  `TestInventoryItemTypesFromRetail` (count 12997, verbatim display names + creator, no-`\u`, Category first-
  token + ForCategory, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_inventory_item_types_note.md`. This is the id->name resolver the reward PAYLOAD increment depends
  on (resolve reward `contents` ids to display names); item payload data itself still lives in cooked SDD.
- **Data (Qoder) — unlock item-type catalog ported (UnlockItemTypes.INT):** extracted the retail unlock
  catalog (mirror of SDD table `UnlockItemTypes`, 8655 kv lines) via
  `tools/scripts/extract_unlock_item_types.ps1` -> `Content/Data/unlock_item_types.json` (1972 rows; only
  ids with a non-empty description kept — 1889 real player-facing + 83 verbatim `DNT` dev-notes). Unlock
  items are tokens/entitlements (emotes, inventory-capacity unlocks for clothing/outfit/symbol/song slots,
  daily-activity tokens, ...). Header-only `APBUnlockItemTypes.h` `UnlockItemTypeCatalog`
  (`Find/Description/HasDescription/ForCategory/Count`, merge-by-id, order-sorted); `static Category(id)` =
  first token (`Unlock`). Prose VERBATIM — embedded double-quotes round-trip through JSON `\"` (quoted
  `/emote` slash-commands); no `\u`. Wired into `WorldService.unlock_item_types` with an
  `unlock_item_types=` INIT token. Test `TestUnlockItemTypesFromRetail` (count 1972, verbatim quoted
  descriptions, no-`\u`, Category + ForCategory, order sort, missing-id safety, end-to-end). All 17 suites
  FAILS=0. See `work/m15_unlock_item_types_note.md`. Pairs with `inventory_item_types` (id->name) for the
  unlock/store UI. NOTE: `InventoryItemPrices.INT` was profiled and is a STUB (all 5495 values empty) — do
  not port it; real prices are in the apbdb catalogs (`weapons_catalog`/`vehicles_catalog`/`apbdb_meta`).
- **Data (Qoder) — inventory item CATEGORY taxonomy ported (InventoryItemInfraCategories.INT):** extracted
  the retail category taxonomy (mirror of SDD table `InventoryItemInfraCategories`) via
  `tools/scripts/extract_inventory_infra_categories.ps1` -> `Content/Data/inventory_infra_categories.json`
  (149 rows; all-empty `None` placeholder dropped). These are the category buckets the inventory / Armas /
  store UI uses to GROUP + LABEL the 13k items (Marketplace Cash, Character, Clothing: Accessories,
  Clothing: Armor (Vests), Weapons, Mods, Vehicles, Symbols, ...); each carries a UI header (DisplayName),
  a plural label (Description) and a singular label (SingularName). Header-only
  `APBInventoryInfraCategories.h` `InventoryInfraCategoryCatalog`
  (`Find/DisplayName/Description/SingularName/Has/Count`, merge-by-id, order-sorted); labels VERBATIM, no
  `\u`. Wired into `WorldService.inventory_infra_categories` with an `inventory_infra_categories=` INIT
  token. Test `TestInventoryInfraCategoriesFromRetail` (count 149, three labels verbatim, no-`\u`, None
  dropped, missing-id safety, order sort, end-to-end). All 17 suites FAILS=0. See
  `work/m15_inventory_infra_categories_note.md`. This is the categorisation/label layer OVER
  `inventory_item_types` (id->name) — the inventory/store UI groups items by category then names them.
- **Data (Qoder) — weapon description catalog ported (WeaponItemTypes.INT):** extracted the retail weapon
  description table (mirror of SDD table `WeaponItemTypes`, 936 kv lines) via
  `tools/scripts/extract_weapon_item_types.ps1` -> `Content/Data/weapon_item_types.json` (839 rows; only
  ids with a non-empty description kept). This is the flavour + role blurb the Armas / weapon-select /
  inventory UI shows — the DESCRIPTION leg of the weapon-info triple: `weapons_catalog.json` (apbdb stats)
  + `weapon_display_names.json` (id->name from InventoryItemTypes) + this (id->description). Header-only
  `APBWeaponItemTypes.h` `WeaponItemTypeCatalog` (`Find/Description/HasDescription/ForCategory/ForClass/
  Count`, merge-by-id, order-sorted); `static Category(id)` = first token (`Weapon`), `static Class(id)` =
  SECOND token (the discriminating weapon class: `SniperRifle`/`AssaultRifle`/...). Prose VERBATIM, no
  `\u`. Wired into `WorldService.weapon_item_types` with a `weapon_item_types=` INIT token. Test
  `TestWeaponItemTypesFromRetail` (count 839, verbatim descriptions, no-`\u`, Category + Class + ForClass,
  order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_weapon_item_types_note.md`. SIBLING TABLES STILL TO PORT (same single-`_Description` schema):
  `VehicleItemTypes.INT` (569 rows) and `ClothingItemTypes.INT` (1836 rows) — one increment each.
- **Data (Qoder) — vehicle description catalog ported (VehicleItemTypes.INT):** extracted the retail vehicle
  description table (mirror of SDD table `VehicleItemTypes`, 580 kv lines) via
  `tools/scripts/extract_vehicle_item_types.ps1` -> `Content/Data/vehicle_item_types.json` (569 rows; only
  ids with a non-empty description kept). This is the flavour + role blurb the Armas / vehicle-select /
  inventory UI shows — the DESCRIPTION leg of the vehicle-info pairing: `vehicles_catalog.json` (apbdb stats)
  + this (id->description). Header-only `APBVehicleItemTypes.h` `VehicleItemTypeCatalog`
  (`Find/Description/HasDescription/ForCategory/ForClass/Count`, merge-by-id, order-sorted); `static
  Category(id)` = first token (`Vehicle`), `static Class(id)` = SECOND token (the discriminating vehicle
  class: `Car` 338 / `Truck` 153 / `Van` 52 / `Armas` / `Ambient` / `Rally`). Prose VERBATIM, no `\u`. Wired
  into `WorldService.vehicle_item_types` with a `vehicle_item_types=` INIT token. Test
  `TestVehicleItemTypesFromRetail` (count 569, verbatim descriptions, no-`\u`, Category + Class + ForClass,
  order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_vehicle_item_types_note.md`. SIBLING TABLE STILL TO PORT (same single-`_Description` schema):
  `ClothingItemTypes.INT` (1836 rows) — one increment, the largest and core to APB customization.
- **Data (Qoder) — clothing description catalog ported (ClothingItemTypes.INT):** extracted the retail
  clothing description table (mirror of SDD table `ClothingItemTypes`, 2058 kv lines) via
  `tools/scripts/extract_clothing_item_types.ps1` -> `Content/Data/clothing_item_types.json` (1836 rows;
  only ids with a non-empty description kept). This is the flavour + role blurb the Armas /
  character-customization / inventory UI shows — the DESCRIPTION leg of the clothing-info pairing (apbdb
  clothing catalogs = slot/faction/unlock metadata + this = id->description); the LARGEST ItemTypes
  description table. Header-only `APBClothingItemTypes.h` `ClothingItemTypeCatalog`
  (`Find/Description/HasDescription/ForCategory/ForClass/Count`, merge-by-id, order-sorted); `static
  Category(id)` = first token (`Clothing`), `static Class(id)` = SECOND token (`Preset` 1013 / `M` 412 /
  `F` 411). Retail data quirk preserved verbatim: 2 rows carry a `Cloting_` typo prefix. Prose VERBATIM, no
  `\u`. Wired into `WorldService.clothing_item_types` with a `clothing_item_types=` INIT token. Test
  `TestClothingItemTypesFromRetail` (count 1836, verbatim descriptions, no-`\u`, Category + Class + ForClass,
  order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_clothing_item_types_note.md`. **The single-`_Description` ItemTypes family is now COMPLETE**
  (Weapon 839 + Vehicle 569 + Clothing 1836); no more sibling `_Description` INT tables remain.
- **Data (Qoder) — authoritative contact catalog ported (Contacts.INT):** extracted the retail
  `Contacts` table (mission-giver NPCs that drive all progression — Double-B, Veronika Lee, Britney
  Bloodrose, Grissom) via `tools/scripts/extract_contacts_catalog.ps1` ->
  `Content/Data/contacts_catalog.json` (**97 rows**; `None` DNT placeholder dropped). Two keys per id:
  `_Title` (display name) + `_Description` (lore bio). This is the AUTHORITATIVE, UNTRUNCATED source —
  bios run to thousands of chars (Double-B 2621, Grissom 7307) vs the apbdb-scraped `contacts_lore.json`
  (bios truncated ~500 chars). NEW separate catalog, merge-safe: does NOT touch `contacts_lore.json` or
  the `ProgressionCatalog` that consumes it — the two coexist. Header-only `APBContactsCatalog.h`
  `ContactCatalog` (`Find/Title/Description/HasDescription/ForDistrict/Count`, merge-by-id, order-sorted);
  `static District(id)` = first token (`Financial`/`Waterfront`/`Social`). Prose VERBATIM, no `\u`. Wired
  into `WorldService.contacts_catalog` with a `contacts_catalog=` INIT token. Test
  `TestContactsCatalogFromRetail` (count 97, `Title("Financial_C1")=="Double-B"`, HasDescription, no-`\u`,
  `None` dropped, District/ForDistrict, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_contacts_catalog_note.md`.
- **Data (Qoder) — in-game tutorial / City Guide catalog ported (Tutorials.INT):** extracted the retail
  `Tutorials` table (the new-player onboarding help book — "Welcome to San Paro", "Basic", "Movement &
  Actions") via `tools/scripts/extract_tutorials.ps1` -> `Content/Data/tutorials.json` (**120 rows**).
  Three keys per id: `_Title` (heading) + `_SubTitle` (tagline) + `_Body` (multi-paragraph HTML). The
  Body's lightweight HTML markup (`<br>`, `<b>`, `<img>`) is kept VERBATIM so the UE5 tutorial-book UI
  renders 1:1 (108/120 bodies carry markup). Header-only `APBTutorials.h` `TutorialCatalog`
  (`Find/Title/SubTitle/Body/HasBody/Count`, merge-by-id, order-sorted). Prose VERBATIM, no `\u`. Wired
  into `WorldService.tutorials` with a `tutorials=` INIT token. Test `TestTutorialsFromRetail` (count 120,
  Title/SubTitle verbatim, literal `&` proves `\u`-restore, `<br>` kept in body, no-`\u`, order sort,
  missing-id safety, end-to-end). All 17 suites FAILS=0. See `work/m15_tutorials_note.md`.
- **Data (Qoder) — loading-screen tip catalog ported (LoadingMovieTips.INT):** extracted the retail
  `LoadingMovieTips` table (the gameplay hints shown over the loading movie during district streaming) via
  `tools/scripts/extract_loading_tips.ps1` -> `Content/Data/loading_tips.json` (**134 rows**; `None` +
  empty-message rows dropped). Single `_Message` per id. Header-only `APBLoadingTips.h` `LoadingTipCatalog`
  (`Find/Message/ForCategory/Count`, merge-by-id, order-sorted); `static Category(id)` = first token (GP 61
  / SL 15 / OW 9 / UI 7 / ED / EG). Prose VERBATIM, no `\u`. Wired into `WorldService.loading_tips` with a
  `loading_tips=` INIT token. Test `TestLoadingTipsFromRetail` (count 134, Message verbatim, no-`\u`,
  Category + ForCategory, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_loading_tips_note.md`.
- **Data (Qoder) — display-point catalog ported (DisplayPoint.INT):** extracted the retail
  `DisplayPoint` table (progression/collectible entries surfaced in the UI, e.g. the "Graffiti - Empire
  Slipway, South" spray-tag collectibles around each district) via
  `tools/scripts/extract_display_points.ps1` -> `Content/Data/display_points.json` (**277 rows**; the
  `None` DNT row + empty-title rows dropped). Four keys per id (`Title/ShortTitle/Description/ObtainedBy`).
  Header-only `APBDisplayPoints.h` `DisplayPointCatalog`
  (`Find/Title/ShortTitle/Description/ObtainedBy/ForDistrict/Count`, merge-by-id, order-sorted);
  `static District(id)` = first token. RETAIL QUIRK preserved 1:1: many `ShortTitle` fields carry the
  literal `DNT - DO NOT TRANSLATE` placeholder (kept verbatim). Prose VERBATIM, no `\u`. Wired into
  `WorldService.display_points` with a `display_points=` INIT token. Test `TestDisplayPointsFromRetail`
  (count 277, Title/ObtainedBy verbatim, DNT ShortTitle preserved, no-`\u`, District + ForDistrict, order
  sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See `work/m15_display_points_note.md`.
  (Build hazard noted: repo-root `.obj` collisions between concurrent agent builds surface as transient
  `C1083 Permission denied` — retry the build.)
- **Data (Qoder) — popup-dialog catalog ported (PopupDialogs.INT):** extracted the retail `PopupDialogs`
  table (in-game advisory/help popups shown during play: ammo-low advice, arrest rules, vehicle controls,
  group/mission notices) via `tools/scripts/extract_popup_dialogs.ps1` ->
  `Content/Data/popup_dialogs.json` (**165 rows**; `None` DNT + empty-body rows dropped). Single
  `_PopupBody` per id. Header-only `APBPopupDialogs.h` `PopupDialogCatalog`
  (`Find/Body/ForCategory/Count`, merge-by-id, order-sorted); `static Category(id)` = first token (AD 70 /
  TD 29 / SD 29 / RO 14 / HUDO 13 / GUI 10). 22 bodies keep `<Key:...>` markup VERBATIM. Prose VERBATIM,
  no `\u`. Wired into `WorldService.popup_dialogs` with a `popup_dialogs=` INIT token. Test
  `TestPopupDialogsFromRetail` (count 165, Body verbatim, no-`\u`, Category + ForCategory, order sort,
  missing-id safety, end-to-end). All 17 suites FAILS=0. See `work/m15_popup_dialogs_note.md`.
- **Data (Qoder) — HUD marker text catalog ported (HUDMarkerVisualText.INT):** extracted the retail
  `HUDMarkerVisualText` table (the text the HUD paints on world markers — mission objectives, spawns,
  items — with up to six role-dependent label variants: OwnerAttack/OwnerDefend/OppositionAttack/
  OppositionDefend/Neutral/Misc) via `tools/scripts/extract_hud_marker_text.ps1` ->
  `Content/Data/hud_marker_text.json` (**112 rows**; `None` + all-empty rows dropped). Header-only
  `APBHUDMarkerText.h` `HUDMarkerTextCatalog` (`Find/Label(id, MarkerRole)/Count`, merge-by-id,
  order-sorted) + `enum class MarkerRole`; `static Family(id)` = first token. `<Color:R=g G=g B=g>` markup
  kept VERBATIM, U+21B5 -> newline, no `\u`. Wired into `WorldService.hud_marker_text` with a
  `hud_marker_text=` INIT token. Test `TestHUDMarkerTextFromRetail` (count 112, role labels verbatim,
  markup preserved, Family, order sort, missing-id safety, end-to-end). All 17 suites FAILS=0. See
  `work/m15_hud_marker_text_note.md`.
- **Data (Cline) — chat-channel catalog ported (ChatMessageCategories.INT):** extracted the retail
  `ChatMessageCategory` table (slash commands + tags + descriptions + syntax examples for all chat
  channels) via `tools/scripts/extract_chat_message_categories.ps1` ->
  `Content/Data/chat_message_categories.json` (**21 rows**; `None` DNT placeholder dropped). 12
  player-usable channels (Clan /c, District /d, Group /g, Officer /o, Reply /r, Say /s, Team /t,
  Whisper /w, Yell /y, Trade /tr) + 11 system-only (Broadcast/Combat/Mission/...). Header-only
  `APBChatMessageCategories.h` `ChatMessageCategoryCatalog` (`Find/Tag/SlashCommand/
  SecondarySlashCommand/Description/SyntaxExample/Count/PlayerChannelCount`, merge-by-id,
  order-sorted) with **`FindBySlashCommand(cmd)`** (resolves primary + secondary, case-insensitive)
  and `HasSlashCommand()` DNT discrimination. KEY RETAIL FINDINGS: Trade uses `/tr` not `/trade`;
  `/t` is Team not Whisper (Domain maps `/t` to Whisper — wrong); Officer (`/o`), Reply (`/r`),
  Yell (`/y`) channels are missing from the Domain ChatChannel enum. Wired into
  `WorldService.chat_message_categories` with a `chat_message_categories=` INIT token. Test
  `TestChatMessageCategoriesFromRetail` (count 21, 10 player channels, slash anchors, tag anchors,
  DNT system channels, FindBySlashCommand primary+secondary+case-insensitive, order sort,
  missing-id safety, no-`\u`, end-to-end). All 17 suites FAILS=0. See
  `work/m15_chat_message_categories_note.md`.
- **Data (Cline) — emote-command catalog ported (EmoteCommands.INT):** extracted the retail
  `EmoteCommand` table (slash commands + display names for the emote wheel UI) via
  `tools/scripts/extract_emote_commands.ps1` -> `Content/Data/emote_commands.json` (**50 rows**;
  no placeholder rows). 15 dance variants (Dance, Dance 80s, ..., Dance Urban) + 35 general emotes
  (Wave, Bow, Surrender, Taunt, ...). Ids may contain spaces (Body Pop, Strike A Pose 1).
  Header-only `APBEmoteCommands.h` `EmoteCommandCatalog` (`Find/SlashCommand/DisplayName/Count/
  DanceCount`, merge-by-id, order-sorted) with **`FindBySlashCommand(cmd)`** (case-insensitive)
  and `IsDance()` sub-classification. Wired into `WorldService.emote_commands` with an
  `emote_commands=` INIT token. Test `TestEmoteCommandsFromRetail` (count 50, 15 dances, slash
  anchors, display names with spaces, FindBySlashCommand, dance classification, order sort,
  missing-id safety, no-`\u`, end-to-end). All 17 suites FAILS=0. See
  `work/m15_emote_commands_note.md`.
- **Data (Cline) — ceremony-message catalog ported (HUDCeremonyMsg.INT):** extracted the retail
  `HUDCeremonyMsg` table (big on-screen celebration popup titles: KILL STREAK!, MEDAL EARNED!,
  NOTORIETY LEVEL UP, BOUNTY CLAIMED, etc.) via `tools/scripts/extract_hud_ceremony_msgs.ps1` ->
  `Content/Data/hud_ceremony_msgs.json` (**93 rows**; empty/DNT dropped). 11 categories: AM (54
  achievement-manager events), Minigame (15), ProvingGrounds (10), Reward (4), etc. Header-only
  `APBCeremonyMsgs.h` `CeremonyMsgCatalog` (`Find/Title/Category/ForCategory/Categories/Count`,
  merge-by-id, order-sorted) with `HasPlaceholder()` for `<WeaponName>` token detection. Covers
  combat streaks, fame/progression level-ups, heat (notoriety/prestige) changes, bounty events,
  rank ups, and all 20+ reward-unlock types. Wired into `WorldService.ceremony_msgs` with a
  `ceremony_msgs=` INIT token. Test `TestCeremonyMsgsFromRetail` (count 93, iconic title anchors,
  bounty faction encoding, reward unlocks, category grouping, placeholder detection, order sort,
  missing-id safety, no-`\u`, end-to-end). All 17 suites FAILS=0. See
  `work/m15_ceremony_msgs_note.md`.
- **Data (Cline) — task-target-type catalog ported (TaskTargetTypes.INT):** extracted the retail
  `TaskTargetType` table (mission objective display names: ATM, Graffiti Point, Checkpoint,
  Pedestrian, etc.) via `tools/scripts/extract_task_target_types.ps1` ->
  `Content/Data/task_target_types.json` (**118 rows**; empty/DNT dropped). 20+ Checkpoint variants
  (race/territory/dropoff/epidemic/musical), 30+ props (ATM/bench/hydrant/mailbox/...), 7 NPC
  targets (Pedestrian/Drug Mule/Mr Bunny/Mr Chicken), vehicles, dropoffs (Fence/Secure Lockup),
  Riot targets, seasonal items. Header-only `APBTaskTargetTypes.h` `TaskTargetTypeCatalog`
  (`Find/DisplayName/ByDisplayName/DistinctDisplayNames/Count`, merge-by-id, order-sorted) with
  `IsNPCTarget()`/`IsCheckpoint()` sub-classification. Wired into `WorldService.task_target_types`
  with a `task_target_types=` INIT token. Test `TestTaskTargetTypesFromRetail` (count 118, display
  anchors, checkpoint variant grouping, NPC classification, ByDisplayName, order sort, missing-id
  safety, no-`\u`, end-to-end). All 17 suites FAILS=0. See `work/m15_task_target_types_note.md`.
- **Test (Cline) — EquipmentTypes test + note added:** the extractor (`extract_equipment_types.ps1`),
  JSON (`equipment_types.json`, 60 rows), and catalog header (`APBEquipmentTypes.h`) were created
  by a prior agent but had no test or note. Verified the data is correct (15 families × 4 mk tiers,
  `base`/`mk` field derivation working). Added `TestEquipmentTypesFromRetail` (count 60, base count
  15, BatteringRam/BrassKnuckles/Handcuffs/SprayCan description anchors, mk tier parsing, ForBase
  grouping, FindBase from upgraded id, IsUpgrade classification, DNT row drops, order sort, missing-
  id safety, no-`\u`, end-to-end). Wrote `work/m15_equipment_types_note.md`. All suites FAILS=0.
- **Data (Cline) — gameplay-object catalog ported (GameplayObjects.INT):** extracted the retail
  `GameplayObject` table (context-sensitive HUD interaction labels: Bench, Taxi, Criminal,
  Pedestrian, etc.) via `tools/scripts/extract_gameplay_objects.ps1` ->
  `Content/Data/gameplay_objects.json` (**44 rows**; empty/DNT dropped). 8 categories: Prop (16),
  Vehicle (13), TaskItem (7), DisplayPoint (3), PlayerCharacter (2), Checkpoint/Graffiti/Pedestrian
  (1 each). Includes Halloween seasonal props (3 pumpkin variants), 9 ambient AI vehicle types,
  player vehicles, task items, and faction player characters. Header-only `APBGameplayObjects.h`
  `GameplayObjectCatalog` (`Find/Description/Category/ForCategory/Categories/Count`, merge-by-id,
  order-sorted) with `IsProp/IsVehicle/IsAmbientVehicle/IsPlayerCharacter` sub-classification. Wired
  into `WorldService.gameplay_objects` with a `gameplay_objects=` INIT token. Test
  `TestGameplayObjectsFromRetail` (count 44, prop/vehicle/player-char/ambient-vehicle labels, category
  grouping, sub-classification, empty-desc/DNT drops, order sort, missing-id safety, no-`\u`,
  end-to-end). All 17 suites FAILS=0. See `work/m15_gameplay_objects_note.md`.
- **Data (Qoder) — reward-package item-type text catalog ported (RewardPackageItemTypes.INT):**
  extracted the retail `RewardPackageItemTypes` table (per-package `Description` / `MailSubject` /
  `MailBody` — the prose shown when a reward package is described in the store/inventory or delivered
  by in-game mail) via `tools/scripts/extract_reward_package_item_types.ps1` ->
  `Content/Data/reward_package_item_types.json` (**139 rows**; 142 ids, kept if any of the 3 fields has
  text, 3 all-empty dropped). Field density 125 desc / 61 subject / 43 body. MailSubject strings keep
  literal embedded double-quotes (`Joker Distribution: "C.S.A. Sting" Outfit!`) round-tripped through
  JSON `\"`; `<col:>` markup + U+21B5->`\n` preserved. Header-only `APBRewardPackageItemTypes.h`
  `RewardPackageItemTypeCatalog` (`Find/Description/MailSubject/MailBody/ForCategory/Count`, merge-by-id,
  order-sorted; `static Category(id)` = SECOND token since all ids are `RewardPackage_` prefixed ->
  Outfit/Components/Weapon/...). This is the TEXT companion to `reward_packages` (id->contents). Wired
  into `WorldService.reward_package_item_types` with a `reward_package_item_types=` INIT token. Test
  `TestRewardPackageItemTypesFromRetail` (count 139, description/mail anchors, embedded-quote round-trip,
  Category second-token, ForCategory, order sort, missing-id safety, no-`\u`, end-to-end). All 17 suites
  FAILS=0. See `work/m15_reward_package_item_types_note.md`.
- **Data (Qoder) — daily-activity contact catalog ported (DailyActivityContacts.INT):** extracted the
  retail `DailyActivityContacts` table (the "do X today" objectives a player gets from a contact each day:
  Title / HUDDescription / LongDescription, e.g. "Blow up 3 enemy vehicles") via
  `tools/scripts/extract_daily_activity_contacts.ps1` -> `Content/Data/daily_activity_contacts.json`. **First
  variant-bearing catalog:** many activities ship randomised flavour VARIANTS (`_2/_3/_4` keys; variant 1 =
  unnumbered), so the extractor FLATTENS to one row per (id, variant) — 85 activities -> **133 rows** (44 with
  a 2nd variant, 3 a 3rd, 1 a 4th). `<col:>` markup + apostrophes preserved verbatim, U+21B5->`\n`. Header-only
  `APBDailyActivityContacts.h` `DailyActivityContactCatalog` (`Find(id,variant)`, `Variants(id)`,
  `VariantCount`, `Title/HUDDescription/LongDescription(id,variant)`, `Count`=133, `ActivityCount`=85;
  merge keyed by (id,variant), order-sorted). Wired into `WorldService.daily_activity_contacts` with a
  `daily_activity_contacts=` INIT token. Test `TestDailyActivityContactsFromRetail` (count 133, 85 activities,
  variant-1/2/4 anchors, Variants sort, markup verbatim, apostrophe round-trip, order sort, missing/out-of-range
  safety, no-`\u`, end-to-end). All 17 suites FAILS=0. Sets the numbered-variant precedent (flatten + variant
  field + (id,variant) key). See `work/m15_daily_activity_contacts_note.md`.
- **Data (Qoder) - task-operation UI-profile tracked-value labels ported (TaskOperationUIProfile.INT):**
  extracted the retail `TaskOperationUIProfile` table - the per-tracked-value HUD sub-labels each mission
  OPERATION shows beside its counters (slot 0 primary "Cover Graffiti:" / "Guard Targets:", slots 1-3 for
  multi-counter ops like BombDisposal "Bombs Armed:" / "Bombs Disarmed:") - via
  `tools/scripts/extract_task_operation_ui_profiles.ps1` -> `Content/Data/task_operation_ui_profiles.json`.
  New indexed-array `[0..3]` schema FLATTENED to one row per profile with a fixed `desc0..desc3` quartet;
  193 raw profiles -> 178 rows (15 all-empty placeholders incl. `None`/`Simple` dropped). Header-only
  `APBTaskOperationUIProfiles.h` `TaskOperationUIProfileCatalog` (`Find(id)`, `TrackedValueDescription(id,idx)`,
  `PrimaryDescription(id)`, `TrackedValueCount(id)`, `Descriptions(id)`, merge-by-id, order-sorted). This is the
  per-tracked-value companion to the already-ported `task_operations` catalog (`MissionOperationCatalog`); ids
  share the TaskOperation id space 1:1. Wired into `WorldService.task_operation_ui_profiles` with a
  `task_operation_ui_profiles=` INIT token. Test `TestTaskOperationUIProfilesFromRetail` (count 178, slot-0 +
  multi-slot BombDisposal anchors, TrackedValueCount, Descriptions slot-order, out-of-range/negative/missing
  safety, no-`\u`, end-to-end). All 17 suites FAILS=0. See `work/m15_task_operation_ui_profiles_note.md`.
- **Data (Qoder) - frontend/menu UI hover tooltips ported (Tooltips.INT):** extracted the retail `Tooltips`
  table - the short hover-hint text for frontend/menu widgets (e.g. `Login_Scene`/`UILabelButton_TOS` ->
  "Create a new APB Account.", `Lobby_Scene`/`UILabelButton_Logout` -> "Log out of this account.") - via
  `tools/scripts/extract_tooltips.ps1` -> `Content/Data/tooltips.json`. New SECTION-SCOPED schema: each
  `[<Scene>]` header groups keys `<Scene>@<Widget>=<text>`; FLATTENED to one row per (scene, widget). 412 keys
  across 54 scenes -> 409 rows (3 empty placeholder widgets dropped; 53 scenes retain a tooltip). Header-only
  `APBTooltips.h` `TooltipCatalog` (`Find(scene,widget)`, `TooltipFor(scene,widget)`, `ForScene(scene)`,
  `SceneTooltipCount(scene)`, `SceneCount()`=53, `Count()`=409; merge-by-(scene,widget), order-sorted).
  Lookups are scene-scoped (same widget id under a different scene is a different tooltip). Wired into
  `WorldService.tooltips` with a `tooltips=` INIT token. Test `TestTooltipsFromRetail` (count 409, 53 scenes,
  Login/Lobby anchors, scene-scoped miss, ForScene, SymbolEditor scene count, missing-scene safety, no-`\u`,
  end-to-end). All 17 suites FAILS=0. Sets the section-scoped precedent. See `work/m15_tooltips_note.md`.
- **Progress (Qoder) — voice-line subtitles catalog (M15 orphan closure):** closed the orphaned
  `Content/Data/subtitles.json` (8844 rows, extracted from retail `Subtitles_MASC.int` — the UTF-16LE
  mirror of the cooked SDD subtitle table) + `tools/scripts/extract_subtitles.ps1`. New header-only
  `APBSubtitles.h` `SubtitleCatalog` (`Find(id)`, `Text(id,def)`, `ForCategory(cat)`, `Count()`,
  static `Category(id)`=first `_`-token; merge-by-id keeps last, order-sorted). Masc/fem are byte-identical
  in this build (SHA256-verified), so one flat id→text catalog is ported from the MASC file. 20 empty-value
  rows dropped by the keep-if-non-empty rule (8864 kv lines → 8844 rows); one id (`ORL_Dispatch_Bounty_1`)
  appears twice with *different* text — merge-by-id keeps the last (order 7007, "lowlifes"), so the catalog
  holds 8843 distinct ids. Category prefixes: CHA (greetings), WLD (world/vendor), PRF/PRM/PTF/PTM
  (prestige/notoriety enforcer/criminal), GKF/GKM/GRI (gang), BRF/BRM (bounty), SUJ, VER, TER, ORL (dispatch),
  etc. Wired into `WorldService.subtitles` with a `subtitles=` INIT token. Test `TestSubtitlesFromRetail`
  (count 8843, CHA_Greeting_Known_1/WLD_Greeting_1 anchors, duplicate-id-last-wins, 121 CHA captions grouped,
  missing-id safety, no-`None`/no-`\u`, WorldService end-to-end). All 17 suites FAILS=0.
  See `work/m15_subtitles_note.md`.

- **Progress (Qoder) — WorldService progress lifecycle wiring:** `WorldService` now owns a
  `CharacterProgress progress` member and round-trips it automatically: `PersistCharacter`
  also calls `store.SaveProgress`, `TryLoadPersistedCharacter` resets + `store.LoadProgress`
  (tolerate-missing), and `CreateCharacter`/`LogoutAccount` reset it — so contact standing /
  role XP now survives a live login→logout→login restart exactly like cash/inventory/threat,
  not just via direct store calls. Proven by extending `run_persistence_tests.cpp` Instance A
  (mutate `progress` before `SaveAllNow`) + Instance B (assert restored on re-login); Instance
  E moved to an isolated `persist_erin` account so it stays independent. All 17 suites FAILS=0.
- **Progress (Qoder) — mission-completion rewards wired into the gameplay loop:**
  `WorldService::AdvanceMission` now, on `MissionStatus::Completed`, calls a new
  `ApplyMissionCompletionReward()` that computes `ComputeMissionReward(base_cash, base_standing,
  0, threat.CurrentTier().reward_multiplier)` and applies cash to `character->cash` + contact
  standing via `progress.AddContactStanding(mission->contact_id, …)`, then persists. Base payout
  is a tunable recreation default scaled by mission stage count (no per-mission reward table is
  parsed from the catalog yet); role/weapon XP stays per-action (kills), matching APB. Previously
  mission complete only bumped threat — progression never changed during play. Proven in
  `run_domain_tests.cpp` (cash increases + contact standing > 0 after a multi-stage mission
  completes). All 17 suites FAILS=0.
- **Progress (Qoder) — progression exposed through `DomainSnapshot`:** `CaptureSnapshot()` now
  surfaces per-character progression so the client HUD / `AAPBPlayerState` can reflect the
  standing/role XP that mission-completion now awards (previously invisible to the single
  Domain→UI bridge). New fields: `contact_standings` + `role_xp` (`vector<SnapshotProgressEntry
  {id,value}>`, entries `>0`, id-sorted for deterministic sync) plus convenience
  `active_contact_id`/`active_contact_standing`/`active_contact_level` from the current mission's
  contact (level via `LevelLadder::DefaultContactLadder()`). Pure/deterministic (`<algorithm>`
  sort only). Proven in `run_domain_tests.cpp` `TestDomainSnapshotParity` (vectors populated,
  id-sorted, value-correct, active-contact matches mission). All 17 suites FAILS=0. UE side still
  maps the two vectors to `TArray<F...>` on `FAPBDomainSnapshotUE` + replicates.

### M16 — Dedicated server hardening + anti-cheat posture  *(brief #15)*
- Scope: server role docs + launch scripts (`tools\scripts\start_world.ps1` /
  `start_district.ps1`), crash recovery + heartbeats → directory eviction, password
  hashing, speed/stat heuristics per ARCHITECTURE.md §9. Verify: kill -9 district → world
  directory reflects exit ≤ 2 heartbeats; fresh-server bootstrap from clean `Saved\`.
- **Progress (Qoder):** server-authoritative anti-cheat *heuristic* Domain layer landed:
  `Source/APBReloaded/Domain/APBAntiCheat.h`/`.cpp`. `MovementValidator` (speed/teleport
  from planar `MoveSample` + max_speed, tolerance 1.25 / teleport 5000), `FireRateValidator`
  (catalog-RPM min interval, tol 0.85, rejected shots don't advance timer), `ShotAnomalyCheck`
  (reported damage/range vs `ItemDef` limits, damage checked first), `AnomalyLog` (weighted
  violations → Warn/Kick/Ban 3/6/12). Deterministic caller-supplied clocks (no wall-clock).
  Tests `tests/run_anticheat_tests.cpp` ($exe16, all 16 suites FAILS=0). Remaining (ops, not
  heuristic logic): launch scripts, heartbeat→directory eviction, fresh-server bootstrap;
  password hashing already lives in `APBCrypto`/`APBTicket`; UE district-GameMode wiring +
  sanction enforcement. See `work/m16_anticheat_note.md`.
- **Progress (Qoder):** world-side **district directory + heartbeat eviction** logic landed
  (the core behind the "kill -9 district → world directory reflects exit ≤ 2 heartbeats"
  gate): `Source/APBReloaded/Domain/APBDistrictDirectory.{h,cpp}`. `DistrictDirectory`
  consumes the M7 relay verbs (`Register`/`Heartbeat`/`ReportLoad`/`PlayerJoined`/`PlayerLeft`
  via `Apply`), tracks `DistrictNode` liveness on a caller-supplied clock, `PruneStale`
  evicts nodes silent > `heartbeat_interval_ms*eviction_multiple` (default 5000×2=10000ms,
  strict `>`), and `LeastLoaded(district)` picks the join target. Distinct from
  `APBSocial.h`'s `WorldDirectory` (world list) / `DistrictRouter` (player pop). Tests
  `tests/run_directory_tests.cpp` ($exe17, all 17 suites FAILS=0). Remaining (needs compiled
  `APBReloadedServer`): FSocket relay loop feeding `Apply` + world-tick `PruneStale`, launch
  scripts `start_world.ps1`/`start_district.ps1`, clean-`Saved\` bootstrap, kill -9 e2e gate.
  See `work/m16_server_directory_note.md`.
- **Progress (Qoder):** M16 **launch scripts** landed — `tools\scripts\start_world.ps1`
  and `start_district.ps1`. Grounded 1:1 in the real conventions: role via the `-WorldServer`
  param (`APBServerControl.cpp`), ports from `APBPorts.h`/`[APBServer]` (World=17778,
  Relay=17800, district=17810+numeric_id), maps + numeric_id + max_players resolved from
  `Content/Data/districts.json` (district selectable by id or numeric_id), district hosts
  `<map>?listen` (no `-WorldServer`) per `DEDICATED_SERVER_GAP.md`. Both support `-DryRun`
  (prints exact command line, validates without the binary) and are overridable via params.
  Verified: PS-AST parse clean; DryRun resolves Financial→17811, Waterfront(11)→17821,
  world→`-WorldServer -Port=17778`; unknown district errors non-zero with a known-list.
  Remaining M16 (needs compiled `APBReloadedServer`): real launch + relay socket loop feeding
  `DistrictDirectory.Apply`/`PruneStale`, kill -9 e2e gate.
- **Progress (Qoder):** M16 **fresh-server bootstrap** landed — `tools\scripts\bootstrap_server.ps1`
  satisfies the "fresh-server bootstrap from clean `Saved\`" gate on the ops side. It (1) validates
  the read-only catalog (`districts.json` REQUIRED → exit 1 if missing; `contacts_lore`/`roles`/
  `threat_table`/`vehicles` recommended → warn only), (2) inspects the persist dir and reports
  CLEAN vs CARRIES-state, (3) provisions the JsonDomainStore layout (`APBPersistence.h` §Layout:
  persist dir + `characters\`) without fabricating state files — the server writes
  accounts/auction/mail/character JSON on first save since all loads tolerate missing files, and
  (4) offers a guarded `-Clean` (scoped to a `Saved\` path, report-only unless `-Force`). Supports
  `-DryRun`. Verified: PS-AST parse clean; DryRun against real `Content/Data` → PREFLIGHT OK exit 0;
  empty DataDir → PREFLIGHT FAIL exit 1; `-Clean -DryRun` on seeded temp reports "would remove" and
  deletes nothing (post-check: all files intact). Remaining M16 (needs compiled `APBReloadedServer`):
  real launch + relay socket loop feeding `DistrictDirectory.Apply`/`PruneStale`, kill -9 e2e gate.
- **Progress (Qoder, 2026-07-21) — M16 ops scripts re-verified end-to-end + progress-sidecar
  coverage confirmed:** re-ran all three `tools\scripts\` ops scripts this pass. `bootstrap_server.ps1`
  full lifecycle: fresh→`CLEAN`/exit 0/dirs made; seeded state incl. an M15
  `characters\<acct>_<slot>_progress.json` sidecar → `CARRIES 2 state file(s)`; `-Clean` report-only
  preserves files; `-Clean -Force` deletes them **including the progress sidecar** (the
  `characters\*.json` glob covers it — no bootstrap change needed for M15). `start_world.ps1 -DryRun`
  → exit 0; `start_district.ps1 -DryRun` resolves Financial(1)→17811 by both id and numeric_id, unknown
  district → exit 1 with known-list. Confirmed all ports are **1:1 with `APBPorts.h`** (World=17778,
  Relay=17800, `DistrictPort=17810+numeric_id`). Corrected the stale "NOT done" launch/bootstrap
  entries in `work/m16_server_directory_note.md`. Remaining M16: UE relay transport + kill -9 e2e gate.
  liveness+population view the district-select screen renders (one row per district, not per
  instance: `{district, instances, total_players}`, sorted by name for deterministic UI). APB
  pools multiple instances of one district; the player picks the district and `LeastLoaded()`
  routes to a node. This is the pure-Domain data `AAPBWorldGameMode::GetDistrictListJson` should
  emit (server-up + pop bar) instead of the raw catalog list. Tests extended in
   `tests/run_directory_tests.cpp` (`TestAggregateByDistrict`: pooling, sum, sort, eviction updates
   aggregate); all 17 suites FAILS=0. Wiring `GetDistrictListJson` to merge this with the catalog
   still needs the compiled `APBReloadedServer` (directory fed by the relay loop).
- **Progress (Sisyphus, 2026-07-24) - district population over-count FIXED + directory gate wired
  into master spine, e2e-PROVEN:** districts reported `population=2..3` for a single real player.
  Two causes: (1) `APBServerControl::HandleMessage` both queued `PlayerJoined`/`PlayerLeft` to
  `InboundMessages` **and** called `Directory.Apply` (relative +/-1), mixing with absolute
  `ReportLoad` - removed the `Apply` from those two handlers (`APBServerControl.cpp` L510/L525 keep
  only `push_back`; the `Apply` at L593 is the separate Register/ReportLoad ingest, untouched);
  (2) **dominant** - districts run as **listen servers** (installed engine can't cook the dedicated
  Server target, R1), so `SetDistrictPopulation(GetNumPlayers())` counted the local listen-host,
  inflating +1 per instance. Added `AAPBDistrictGameMode::GetRemotePlayerCount()` (`.h` L62, `.cpp`
  L252 - iterates controllers, counts `!IsLocalController()`) and replaced all three report sites
  (BeginPlay L146, PostLogin L320, Logout L649). Population = pure sum of node `player_count` via
  `DistrictDirectory::AggregateByDistrict()`. Wired the directory gate as REQUIRED **step-9** of the
  master spine `tools\run_verification_gates.ps1` (L355-365, mirroring the step-8 precedent; summary
  key `m7_directory_gate` L381). *Verify (real e2e run, listen-host world + 2 Financial nodes + travel
  probes):* `powershell -File tools\run_m7_directory_gate.ps1` -> exit 0,
  `DIRECTORY_HAPPY_1_OK instanceCount=2 population=1 expectedPopulation=1` (**bug gone**: 2 hosts, 1
  real player), `DIRECTORY_HAPPY_2_OK` (least-loaded -> port 17812), `DIRECTORY_FAILURE_EVICT_OK
  stale=1` (kill -> eviction), `DIRECTORY_REGRESSION_TRAVEL_OK survivorPort=17811` (survivor
  `instanceCount:1 population:1`), `LEAKED=0`, `M7_DIRECTORY_GATE_OK`. Evidence:
  `work\logs\m7_directory_gate_20260724-225622.terminal.log`. 18 domain suites FAILS=0; editor target
  rebuild `Result: Succeeded`. Temporary `DIRECTORY_POP_DIAG` logging removed (0 residual in shipped
  source).


### M17 — Optimization & polish  *(brief #16)*
- Scope: perf capture on both districts (stat unit), texture/mesh streaming budgets,
  module split evaluation (D13), World Partition evaluation, symbol-editor depth,
  tattoo/decal rendering pass, final fidelity sweep vs both reference installs.
- Verify: gates + full test suite green; 60 fps on target hardware in both districts.
- **Progress (Sisyphus, 2026-07-25) — 64-client load-gate per-district workload correction
  live-verified:** the load gate (`tools\run_64_client_gate.ps1`) previously told EVERY district's
  synthetic clients to run the full `movement,combat,vehicle` taxonomy, so a Social run could never
  satisfy its workload receipts — Social (Breakwater Marina) is a non-combat safe zone with no drawn
  weapons and no drivable vehicles, so combat/vehicle receipts are physically unreachable and the gate
  would false-fail `missing_workload_receipts`. Fix resolves a per-district `$districtWorkload` after
  district resolution (`run_64_client_gate.ps1` L66/L316-317): `$socialDistrictIds` → `movement` only,
  every combat-capable district → full taxonomy. `$workload` stays the canonical 3-entry taxonomy in
  `load_contract.workload` (telemetry schema requires ≥3); `$districtWorkload` is what clients are told
  to run (L388-389) and what receipts validate against (L437). *Verify (fresh live `-ValidateOnly`
  runs, 2026-07-25T20:16):* `pwsh -File tools\run_64_client_gate.ps1 -Map Social -Clients 2
  -ValidateOnly` → `LOAD_ASSIGNMENT ... workload=movement` (both clients) +
  `LOAD_GATE_WORKLOAD district=Social required=movement`, exit 0; same for `-Map Financial` →
  `workload=movement,combat,vehicle` + `LOAD_GATE_WORKLOAD district=Financial
  required=movement,combat,vehicle`, exit 0. Evidence:
  `work\task17-loadgate\verify_social.log` / `verify_financial.log`. The prior on-disk evidence
  (`work\load64_20260725_181102.log`) predates the fix (shows Social with full taxonomy, no
  `LOAD_GATE_WORKLOAD` line) and is superseded. Remaining M17: real launched (non-validate) gate run
  via `run_task17_loadgate.ps1` once a district listen-server is up; the editor/hardware-bound scope
  (stat-unit perf capture, streaming budgets, World Partition, symbol-editor depth, tattoo/decal, 60fps
  target, final fidelity sweep) is unchanged.

## 6. Parallel groups

- **G1:** M2 (Domain persistence) ∥ M3 (extraction batches) — different trees.
- **G2:** M4 (menu UI) ∥ M5 data prep (palettes/INT imports) — after G1.
- **G3:** M6/M7 server work ∥ M8 Social content — after M5.
- **G4:** M9 ∥ M12-Domain (auction logic/tests) — different files.
- M10 follows M9 (pipeline reuse); M11 needs M7 + M10 geography; M13–M15 sequential after
  M12; M16/M17 terminal.

## 7. Risks & mitigations

- **umodel vs 2011 pkg ver 547**: M3 spike capped at 2 attempts; fallback = screenshot +
  retail-art rebuild of menu chrome (strings/audio still 1:1 from plain files).
- **Morph extraction fails** → hand-built morph set (D4 fallback).
- **Dedicated cook issues**: never cooked before; timebox, listen-server fallback stays
  documented; AGENTS.md rule 5 — >2 failed attempts ⇒ debugging note in `work\`, no loops.
- **Scope creep** (protocol emulation, licensed music, kernel AC): standing non-goals;
  changing them requires amending this file.
- **Copyright**: extracted assets are local port/reference material only.

## 8. Definition of done (whole effort)

Boot → 2011 splash → 2011-styled login → retail-styled char create → world server
directory → travel into dedicated Waterfront/Financial/Social districts with a second
client, missions dispatching, auction trades surviving restarts, clans/friends/mail live —
with `tests\build_and_run.ps1` and `tools\run_verification_gates.ps1` green and all three
targets (game/editor/server) compiling clean on UE 5.8. Then this file → `work\_archive\`.
