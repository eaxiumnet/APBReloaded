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

### M6 — Login/auth + world server  *(brief #5 — D6/D11)*  — PLAN (hyperplan 2026-07-20)

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
