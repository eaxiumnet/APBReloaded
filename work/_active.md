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

### M3 — Extraction pipeline completion  *(brief #2)*
- Files: `tools\scripts\export_2011_ui*.py`, `extract_2011_login_assets.py`,
  `export_apb_level_parallel.py`, `tools\UEViewer\`, `tools\WwiseExtract\`;
  new `tools\convert\parse_int_tables.py`.
- Actions: verify umodel on **547** (2011) vs 564 (retail) packages (spike, cap 2 attempts);
  batch: 2011 menu art/fonts, UI sfx from `Basic_Media.bnk`/`Main_Media.bnk`,
  `SplashScreen.bik`/`IntroTitles.bik` → mp4, retail `Colours\*.ini` → `palettes.json`,
  retail INT tables → catalog JSON; every batch updates the ledger.
- Expected: all M4/M5 inputs on disk under `Content\Extracted\` + `Content\Data\`.
- Verify: ledger shows `extracted` for every M4/M5 dependency; docs updated.

### M4 — 2011 main menu  *(brief #3 — D2/D3)*
- Files: `Systems\Frontend\APBFrontendWidget.*`, `APBFrontendLayoutMath.h`,
  `Content\Imported\UI\Menu2011\`, `Content\Audio\UI\`, `Content\Movies\`.
- Actions: restage Login/CharSelect/DistrictSelect to 2011 layout using extracted chrome +
  `[APBLoginScreen]`/`[CharacterSelectScreen]` strings + UI sfx; boot flow splash video →
  login; 2011 menu music (existing extracted theme WAVs).
- Verify: updated layout-math tests green; `-APBProbe=frontend_flow` passes; side-by-side
  screenshot fidelity checklist vs 2011 capture stored in `work\` signed off.

### M5 — Retail character creation  *(brief #4 — D4)*
- Files: `Domain\APBCustomization.*`, `Content\Data\{clothing.json, palettes.json}`,
  CharCreate stage of frontend widget, `APBCharacterCreatePreviewActor`,
  `tests\run_fidelity_tests.cpp`.
- Actions: 7 → 15 retail categories; palette grids; randomize; camera per `APBLCC.ini`;
  morph spike (cap 2). Tattoos/symbols/layers: data model + UI stubs this milestone; full
  symbol-editor depth tracked as M17 polish item.
- Verify: fidelity tests (15 categories equip/serialize/restart) green; retail
  `[CharacterCustomisationScreens]` screenshot checklist signed off.

### M6 — Login/auth + world server  *(brief #5 — D6/D11)*
- Files: `APBReloadedServer.Target.cs`, `AAPBWorldGameMode` (fill shell), new
  `Systems\Server\APBServerControl.*`, `APBGameInstanceSubsystem` (connect flow), Config.
- Actions: cook `APBReloadedServer`; world role = login/auth/directory/character DB via
  WorldService + M2 stores; ticket issuing.
- Verify: world server + 2 clients: login → char select → district list served from world.

### M7 — District servers + travel + chat baseline  *(brief #6 — D6/D10)*
- Files: `APBDistrictGameMode`, `APBServerControl` (register/heartbeat/ticket/char.handoff/
  chat.relay), `Systems\Frontend` district-select travel, new chat widget + Domain ChatService.
- Actions: district role boots freeroam map, registers to world; client travel with ticket;
  in-district + cross-district chat.
- Verify: world + 1 district + 2 clients travel in, replicate (PlayerState OnRep real
  values), chat both scopes; `tools\DEDICATED_SERVER_GAP.md` updated (gap closed).

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

### M12 — Marketplace & auction house  *(brief #11 — D7)*
- Files: `Domain\APBAuction.*`, MailService in `APBSocial.h`, new
  `Systems\Economy\APBAuctionWidget.*`, `AAPBInteractable` terminals, auction domain tests.
- Actions: bid/buyout/expiry/5% fee/cancel; world-owned listings with district routing
  (ARCHITECTURE.md §3); mail delivery of items/cash; kiosk UI on retail `[Marketplace]`
  strings.
- Verify: two accounts trade across a world restart; tests green.

### M13 — Vehicle systems  *(brief #12 — D15)*
- Spec-expansion first step. Scope: catalog stats/handling per `vehicles.json`; spawn
  kiosks; paint grids from retail `Colours\StandardPaint/PearlescentPaint.ini`; damage
  states. Verify: drive + customize + persist across district rejoin.

### M14 — Clans, friends, groups, mail UI  *(brief #13 — D10)*
- Files: Domain `ClanService`/`GroupService`, `clans.json`/`social.json`, UMG social panel.
- Verify: clan create/invite/promote; friends add/see-presence (via world relay); group
  invite → shared mission queue (M11 hook). Domain tests + 2-client probe.

### M15 — Economy & progression  *(brief #14)*
- Scope: role progression from `roles.json` + retail `PlayerRoles.INT`/`ContactLevels.INT`;
  contact standing; unlocks; threat rewards per `threat_table.json` multipliers; cash sinks
  (auction fees already in). Verify: progression domain tests; apbdb-parity spot checks.

### M16 — Dedicated server hardening + anti-cheat posture  *(brief #15)*
- Scope: server role docs + launch scripts (`tools\scripts\start_world.ps1` /
  `start_district.ps1`), crash recovery + heartbeats → directory eviction, password
  hashing, speed/stat heuristics per ARCHITECTURE.md §9. Verify: kill -9 district → world
  directory reflects exit ≤ 2 heartbeats; fresh-server bootstrap from clean `Saved\`.

### M17 — Optimization & polish  *(brief #16)*
- Scope: perf capture on both districts (stat unit), texture/mesh streaming budgets,
  module split evaluation (D13), World Partition evaluation, symbol-editor depth,
  tattoo/decal rendering pass, final fidelity sweep vs both reference installs.
- Verify: gates + full test suite green; 60 fps on target hardware in both districts.

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
