# M4 — 2011 Main Menu Fidelity Checklist (sign-off)

Status: **IN PROGRESS** · Owner: lead · Companion spec: `work/menu2011_spec.md` (authoritative).

Purpose: the M4 definition-of-done in `work/_active.md` requires a "side-by-side
screenshot fidelity checklist vs 2011 capture stored in `work/`, signed off." This is
that document. Each item cites the spec section it verifies. `[INFERRED]` marks items the
2011 UIScene editor-previews could not fully confirm (see spec §0) — acceptable per spec,
to be trued-up only if the optional manual 2011-client capture contradicts them.

## Ground-truth reference (spec §0)

- No live 2011 menu screenshots exist in-repo. Ground truth = the 2011 UIScene
  editor-preview textures under `Content\Extracted\2011\LiveCurrentScene\hero\*.png`
  (`Login_Scene_Preview`, `Lobby_Scene_Preview`, `WorldQueue_Scene_Preview`,
  `TOS_Scene_Preview`, etc.) — real layout, art, and strings.
- **Manual capture is OPTIONAL and NON-BLOCKING** (spec §0/§11.7): launching
  `...\APB North America\Binaries\APB.exe` windowed and screenshotting Login at 1920×1080
  is a ≤15-min human verification for the two `[INFERRED]` items only. It is NOT a gate and
  NOT scheduled as a blocking step. If captured, store the PNG next to this file.

## Palette / global tokens (spec §2)

- [ ] Monochrome greys + ONE amber accent `#FFC254` (`APB_AMBER`); faction red/blue reserved.
- [ ] NO cyan anywhere (old `APB_PANEL_EDGE 0.15,0.72,0.92` fully purged).
- [ ] Window panels use `MessageBox_BG` 9-slice look (`APB_PanelBrush`), not flat fills.
- [ ] Buttons use `Menu_Button_On/Off`; selected rows use amber `Menu_Button_Light`.
- [ ] Fonts: Arial Bold (`FCoreStyle` "Bold"), ALL-CAPS headers/buttons, sizes per §2.4.
- [ ] UI sfx fire on hover/click/back/error per the §7 slot table.

## Splash / boot flow (spec §6)

- [ ] Boot order Splash → Login (classic path, not character-first).
- [ ] Splash shows `LoadingScreen_APB` on black (Bink1 unplayable — documented fallback),
      auto-advances (~1.5s) or on key to Login.
- [ ] Menu bed videos play via existing MediaPlayer ladder (`Login_BG` etc.).
- [ ] Login theme music (`LoginTheme_APBTheme1`) starts on the login stage.

## Login screen (spec §3) — restaged in M4b (commit 1d821c3)

- [ ] Centered window over `Login_BG` bed; `MessageBox_BG` 9-slice panel; amber accents.
- [ ] Title reads "LOGIN" (compact) / "TERMS OF SERVICE" (`bFirstRunTOS` first-run).
- [ ] Brand key chip (amber) left of title; close (exit-to-desktop) affordance.
- [ ] Email + Password fields (`APB_BG_TextEntry` styling), labels from `ui_strings_2011`.
- [ ] Remember-Me checkbox (`Check_True/False`, amber hover/pressed states).
- [ ] Primary button "Login" / "Accept" (TOS state); footer strip present.
- [ ] `[INFERRED]` State B compact geometry (1006×480) — confirm vs optional 2011 capture.
- [ ] First-run TOS scroll (1006×898) shows license body, scrollable.
- [ ] Status line shows amber Contacting/LoggingIn/NoUserID/NoPassword messages.

## Character Select (spec §4) — restaged in M4c

- [ ] Header "CHARACTER SELECT" with amber brand chip.
- [ ] Left panel (`MessageBox_BG` chrome) with account block + CHARACTERS list.
- [ ] Populated character row highlighted amber (`Menu_Button_Light`); empty slot greyed.
- [ ] CREATE CHARACTER row (amber) → OnCreateCharOpen.
- [ ] Name plate: character name + faction; rating/threat badge (`BG_Button_Active_Ring`).
- [ ] Faction icon uses staged `Faction_Icon_*`/`TexFaction*` uassets (no runtime file import).
- [ ] Primary PLAY button → OnSelectExistingChar; Back/Settings preserved.

## District Select (spec §5) — restaged in M4d · `[INFERRED]` (no 2011 preview)

- [ ] Header "SELECT DISTRICT"; district rows built from `districts.json` parse (travel intact).
- [ ] Each row shows district photo (`*District_MainPhoto256x195`) + name (amber/white bold).
- [ ] JOIN / ENTER DISTRICT button per row/selection → OnEnterDistrict (travel wiring intact).
- [ ] `DistrictCombo` hidden-select + OnDistrictComboChanged preserved.
- [ ] No mojibake in row labels (clean ASCII separators).
- [ ] Back / Settings buttons preserved.

## Sign-off evidence (filled at M4 close)

- Editor build (`APBReloadedEditor` Win64 Development): **exit 0** (Result: Succeeded, commit af40e5e).
- Game build (`APBReloaded` Win64 Development): **exit 0** (Result: Succeeded, commit af40e5e).
- Layout-math suite: **FAILS=0** (20 PASS incl. login geometry, TOS, fit-scale, movie/asset staging).
- Domain + persistence suites (`build_and_run.ps1`): **FAILS=0** (all A/B/C/D groups PASS).
- Residual-cyan grep on `APBFrontendWidget.cpp`: **clean** — only a comment referencing the purged
  cyan identity; 0 cyan color literals. Non-ASCII scan: 0 U+FFFD, 0 mojibake, restage added 0 non-ASCII bytes.
- `-APBProbe=frontend_menu` (M4 gate): **PASS** — terminal `FRONTEND_MENU_OK` after the full 2011 menu
  sequence Splash→Login→(login_fail→login_ok)→CharacterSelect→CharacterCreate→CharCreateConfirm→
  DistrictSelect→`DISTRICTS count=8`→`DISTRICT_ENTER ok=1 district=Financial`→`TRAVEL_OPENLEVEL_CALLED`,
  ZERO stage failures (log: `frontend_menu.log`, 21 lines). Editor **self-exits in ~17s** via
  `RequestEngineExit` (the prior harness had no exit call and zombied indefinitely — now fixed).
- `-APBProbe=frontend_flow` (M9+M12 integration gate, NOT M4): identical menu sequence THEN post-travel
  freeroam playables; terminal `FRONTEND_FLOW_FAIL post_travel_playables` (`props=0` from `vehicles=0`,
  `walk=0` from no walkable geometry) is the **expected M9/M12 baseline**; also self-exits (~56s). Gate
  split ratified by Oracle: M4 depends only on the menu-scoped `FRONTEND_MENU_OK` token; the composite
  flow verdict is held for M9 (geometry) + M12 (vehicles). `tools/run_verification_gates.ps1` now
  hard-gates `FRONTEND_MENU_OK` and defers `frontend_flow` behind `-IntegrationGate`.
- Visual fidelity checkboxes above: pending the Reviewer Gate's hands-on QA verdict on whether rendered
  side-by-side screenshots are a hard blocker (spec §0 marks manual capture OPTIONAL/NON-BLOCKING; ground
  truth = in-repo 2011 editor-preview textures under `Content\Extracted\2011\LiveCurrentScene\hero\`).
- Optional 2011-client capture stored: __path or N/A (non-blocking)__
- Signed off by: __ · Date: __
