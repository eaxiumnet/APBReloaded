# M4a — 2011 RTW Main Menu Recreation Spec

Status: AUTHORITATIVE for M4b (UMG restage). Written 2026-07-19.
Scope: Login screen (primary), CharacterSelect + DistrictSelect (secondary), boot flow,
shared chrome, typography, colors, UI sfx, asset staging.

---

## 0. Ground truth statement

- **No live 2011 menu screenshots/recordings exist in the repo.** `work/_archive/login_swap/`
  contains only a uMod tool window capture (`mod/captures/umod_screenshot.png` — not menu
  footage). Its `docs/FINAL_REPORT.md` confirms the 2011 client **launches and reaches the
  original menu** from `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe`.
- **Ground truth used here = the 2011 UIScene editor-preview textures** exported from
  `APBMenus_GameFlowScenes.upk` / `APBMenus_Art_GameFlowScenes.upk` (UE3 design-time renders of
  the actual scenes, 256×256). They show real layout, real art, real strings:
  - `Content\Extracted\2011\LiveCurrentScene\hero\Login_Scene_Preview.png`
  - `...\Lobby_Scene_Preview.png` (this IS the 2011 character-select scene)
  - `...\WorldQueue_Scene_Preview.png`, `CreateCharacter_Scene_Preview.png`,
    `FactionSelect_Preview.png`, `GameFlowBase_Scene_Preview.png`, `TOS_Scene_Preview.png`
- **Fidelity-reference procedure (for M4b sign-off, ≤15 min):** launch the 2011 client
  (`...\Binaries\APB.exe`, windowed), wait ~30 s through SplashScreen → IntroTitles →
  Login_Scene, screenshot (Win+Shift+S) at 1920×1080, store captures in `work\` next to this
  spec. Use it to confirm the two inferred items flagged `[INFERRED]` below. Do not attempt
  packet/login-server work; the menu appears without any server.
- Rendering pipeline evidence: `Content\Extracted\2011\LOGIN_MENU_RENDERING_PIPELINE.md`
  (scene stack GameFlowBase_Scene → IntroMovieScene → Login_Scene; TextureMovie backgrounds
  Login_BG / Character_Select_BG / Faction_Select_*_BG / Generic_BG; no 3D UIDistrict in 2011).

## 1. Asset sources (all paths repo-relative)

| Class | Path | Notes |
|---|---|---|
| Scene previews (geometry ref) | `Content\Extracted\2011\LiveCurrentScene\hero\*.png` | 256² design-time renders |
| Chrome (full 2011 `APBMenus_Art`) | `Content\Extracted\2011\LiveCurrentScene\png\packages\APBMenus_Art\APBMenus_Art\Texture2D\` | **222 PNGs — fuller than the M3 MenuArt dump (which has only 8 for this package; use this tree)** |
| Skins / Font / FrontEnd | `Content\Extracted\2011\MenuArt\APBMenus_{Skins,Font,FrontEnd}\Texture2D\` | MessageBox_BG, Check_*, font atlas, message-box previews |
| GameFlow art (icons, avatars, loading) | `Content\Extracted\2011\MenuArt\APBMenus_Art_GameFlowScenes\Texture2D\` | 38 PNGs |
| District photos / faction icons | `Content\Extracted\2011\MenuArt\APBMenus_Art_{DistrictPictures,FactionIcons,LargeFactionIcons}\Texture2D\` | |
| Strings | `Content\Data\ui_strings_2011.json` | sections `[APBLoginScreen]`, `[CharacterSelectScreen]`, `[DistrictSelect_Action]`, `[CommonGameFlowScenes]`(not present — see §9), `[WorldSelectScreen]` |
| UI sfx | `Content\Extracted\2011\UISfx\*.wav` (+`uisfx_manifest.json`) | 72 WAVs from Basic_Media/Main_Media |
| Menu music | `Content\Audio\LoginTheme_APBTheme1.wav` etc. | already imported |
| Animated backgrounds | `Content\Movies\Login\` | 5 stage beds, compat mp4 + 4k webm (AI-upscaled from the 2011 TextureMovies) |
| Splash videos | `Content\Extracted\2011\Movies\SplashScreen.bik`, `IntroTitles.bik` | **Bink 1 — unplayable in UE 5.8** (BinkMedia is Bink2-only); see §8 |

## 2. Global design tokens

Design resolution: **1920×1080** (matches `APBFrontendLayoutMath.h` kDesignW/kDesignH; the
2011 editor previews render at the scene's native ~4:3-safe area — fractions below are
normalized to full screen).

### 2.1 Window chrome (9-slice)

| Piece | Texture | Sampling |
|---|---|---|
| Window body | `MenuArt\APBMenus_Skins\Texture2D\MessageBox_BG.png` (512²) | dark grey panel, center **#4F4F4F @ a=255**, alpha fades to 0 at outer ~24 px (soft white glow edge). 9-slice margin ≈ 0.05 (26/512) |
| Tintable window frames | `LiveCurrentScene\...\APBMenus_Art\...\APB_Window_BG.png`, `APB_Window_BGC.png`, `Window_BG_04/05/05_a/06.png`, `Window_BGC_06.png`, `Window_GeneralBG.png` | white-on-alpha, tint per context |
| Title accent strip | `Window_Title_Accent_01.png` / `APB_Window_Title_Accent.png` | white-on-alpha |
| Dividers | `Window_Devider_01.png`, `Window_Devider_02.png` | mid-grey **#848684** |
| Drop shadow | `APB_DropShadow.png` | |
| Footer strip | `MenuArt\APBMenus_Art_GameFlowScenes\Texture2D\frontendFooter.png` (1024×128) | black grunge bar, rough edges |

### 2.2 Buttons

| State | Texture | Appearance |
|---|---|---|
| Standard menu button | `Menu_Button_On.png` / `Menu_Button_Off.png` (128×64) | dark charcoal gradient bar (**#000000 @ a≈43** center over panel ≈ #2A2A2A), angled notch cut on left edge, circular tab top-left. On = slightly lighter gradient than Off |
| Highlighted / selected row | `Menu_Button_Light.png` (16², tiled) | amber-gold **#FFC254** (edge #FFFB9C, bottom #F7BA5A) — the orange selection seen in the Lobby preview |
| Close button | `JKICON_close_default/over/pressed/Highlight.png` (PopUpMenu pkg) | |
| Generic small button | `APB_Button_Generic_SM_Active/_Active_2/_On.png`, `APB_Button_Global_SM_Off/_On/_Pressed.png` | |
| Checkbox | `MenuArt\APBMenus_Skins\Texture2D\Check_True.png` / `Check_False.png` | |
| Text entry field | `APB_BG_TextEntry.png` (32×16) | light field: top **#F7F7F7**, body near-transparent over panel, dark baseline **#292829** — renders as the "underline" inputs seen in the Login preview. Alt: `BG_TextEntry_01.png`, `BG_TextEntry_Round_01.png` |
| List row | `APB_List_Cell_NoBG_20.png` (+`_Active`,`_Pressed`), `APB_SmallListItem_Generic_01.png` | |
| Scrollbar | `Slider_Arrow_01_Up/Down.png` (+`_Active`), `Slider_BG_04.png`, `Slider_Marker_04.png` | |
| Tabs | `Tab_01_On/Off.png`, `Tab_02(_Enabled).png`, `Tab_03(_Focused).png` | |

### 2.3 Colors (sampled)

| Role | Hex | Source |
|---|---|---|
| Panel body | #4F4F4F (fades out at edge) | MessageBox_BG |
| Panel edge glow | #605E60 (alpha ramp) | MessageBox_BG bottom edge |
| Selection amber | #FFC254 | Menu_Button_Light |
| Selection amber hi | #FFFB9C | Menu_Button_Light top |
| Divider grey | #848684 | Window_Devider_01 |
| Text entry light | #F7F7F7 | APB_BG_TextEntry |
| Criminal icon | red/black crossed rifles in circle | CriminalFactionicon.png |
| Enforcer icon | blue-black star shield | EnforcerFactionicon.png |
| Background bed | light grey concrete + red (crim) / blue (enf) graffiti | Login_BG_AI_still.png |

2011 palette is deliberately monochrome (greys/white) with ONE amber accent for selection
and red/blue reserved for faction identity. **The current widget's cyan
(`APB_PANEL_EDGE 0.15,0.72,0.92`) is wrong for 2011** — replace with amber #FFC254 accents.

### 2.4 Typography

- 2011 font atlas: `MenuArt\APBMenus_Font\Texture2D\Texture2D_37.png` — heavy **bold grotesque
  sans** (double-storey a, straight terminals; "The quick brown fx jmp" sample glyphs).
  Rebuilding a Slate bitmap font from the atlas is possible but not worth it.
- **Decision (simpler path): use a Windows-shipped TTF — `Arial Bold` — for all 2011 menu
  text**, ALL-CAPS for headers/buttons, sentence case for body. Sizes at 1920×1080:
  scene title 28 bold caps; window title 20 bold caps; field labels 13 bold caps;
  body/list 15 regular; button labels 15 bold caps; small/footer 11 regular.
  Load via `FCompositeFont`/`FSlateFontInfo` with the OS font (matches existing
  `FCoreStyle::GetDefaultFontStyle("Bold", N)` usage — Arial Bold is visually close enough;
  swap-in of a truer clone (e.g. "Liberation Sans Bold") is a one-line change later).

## 3. LOGIN screen (primary) — `Login_Scene`

Verified against `Login_Scene_Preview.png` (window + TOS variant) and
`LOGIN_MENU_RENDERING_PIPELINE.md`. The preview shows the **first-run / full state**: a large
centered window over the animated Login_BG bed whose title bar reads "TERMS OF SERVICE", with
two underlined entry fields directly under the title, a scrollable text body (license
sections "1. License … 2. Restrictions …"), and a centered bottom button ("Accept").

### 3.1 Element tree + geometry (1920×1080)

```
LoginStage (full screen)
├── BgVideo            (0,0,1920,1080)  Login_BG loop  [Movies\Login\Login_BG_AI_compat.mp4]
├── FooterStrip        (0,1000,1920,80) frontendFooter.png, tiled-x       [INFERRED placement]
│   ├── VersionText    (24,1036,400,20)  small 11 grey — build/version string
│   └── FooterLinks    right-aligned row: "EXIT TO DESKTOP" · "ACCOUNT" · "REPLAY VIDEOS"
│                      small text buttons  (strings: ExitToDesktop / AccountManagement / ReplayVideos)
└── LoginWindow        (524,60,1006,898)  ← preview-measured: x .273–.797, y .056–.887
    ├── Panel          MessageBox_BG 9-slice (margin 0.05), dropshadow APB_DropShadow
    ├── TitleBar       (0,0,1006,48)
    │   ├── BrandChip  (16,8,32,32)  JKICON_login_header_key.png (white key glyph, tint amber)
    │   ├── TitleText  (56,10,700,28) 20 bold caps — "TERMS OF SERVICE" (TOS state) / "LOGIN" (compact)
    │   └── CloseX     (966,12,24,24)  JKICON_close_* 4-state
    ├── Divider        (16,52,974,2)   Window_Devider_01 tiled
    ├── EmailLabel     (32,72,300,18)  13 bold caps  string [APBLoginScreen]EmailAddress = "Email Address"
    ├── EmailEntry     (32,92,940,28)  APB_BG_TextEntry 9-slice
    ├── PassLabel      (32,132,300,18) 13 bold caps  [APBLoginScreen]Password = "Password"
    ├── PassEntry      (32,152,940,28)  APB_BG_TextEntry (password echo)
    ├── InstructionLn  (32,192,940,18) 13 reg grey  [APBLoginScreen]Instructions
    ├── TosScroll      (32,224,942,560) ← full/TOS state only; inset MessageBox_BG darkened,
    │   │                              MultiLineEditableText(read-only), scrollbar Slider_* right
    │   └── TosText    EULA body from APBUserInterface.int EULA section (plain text dump)
    ├── RememberRow    (32,800,400,24)
    │   ├── Check      (0,4,16,16)     Check_True/Check_False
    │   └── Label      (24,2,300,20)   [APBLoginScreen]RememberUserID = "Remember Me"
    ├── ActionButton   (393,840,220,40) Menu_Button_On/Off, label 15 bold caps centered
    │                                  TOS state: "Accept" · Compact state: [APBLoginScreen]Login = "Login"
    └── StatusLine     (32,860,942,20) 13 amber — ContactingLS/LoggingIn/NoUserID/NoPassword errors
```

### 3.2 States

- **State A — Full / first-run (preview-verified):** window 1006×898 as above; TOS scroll
  visible; ActionButton = Accept; email/password still present above the scroll.
- **State B — Compact / returning [INFERRED]:** same window chrome and field layout, window
  height collapses to ~480 (y 300–780), TosScroll hidden, ActionButton = "Login". Geometry
  of fields/title identical (they keep their window-relative offsets). Confirm against the
  2011 capture (§0); if the capture shows a different compact arrangement, M4b amends only
  window height + button label — no other elements move.
- **Compliment line:** strings `Compliment_00..05` ("Have you been working out?…") are a
  random greeting shown after successful login on transition to Lobby [INFERRED target:
  CharacterSelect status area]. Low priority; wire to Lobby StatusText.

### 3.3 Interactions → sfx / flow

| Event | SFX (§7) | Result |
|---|---|---|
| Hover any button/field | UI_Hover | Menu_Button_On lighten / amber edge |
| Click Login/Accept | UI_Click | StatusLine = `ContactingLS`; on success → `LoggingIn` → CharacterSelect (scene-open sting) |
| Empty user / pass | UI_Error | StatusLine = `NoUserID` / `NoPassword` |
| Click Exit to Desktop | UI_Back | quit |
| Checkbox toggle | UI_Click | Check_True/False swap |

## 4. CHARACTER SELECT — `Lobby_Scene` (secondary detail)

Verified against `Lobby_Scene_Preview.png`: header "CHARACTER SELECT" top-left with APB chip;
left column with account block, "CHARACTERS" list (one amber-highlighted row), two small
buttons bottom-left; center-right animated Character_Select_BG bed with the character render;
bottom-center dark name plate with a circular rating badge at its right; a wide action bar
under the plate; small faction icons right of it.

### 4.1 Element tree + geometry

```
CharSelectStage
├── BgVideo            (0,0,1920,1080)  Character_Select_BG loop
├── Header             (16,12,700,40)
│   ├── BrandChip      (16,12,32,32)    JKICON_login_header_key (amber)
│   └── TitleText      (56,16,600,30)   28 bold caps "CHARACTER SELECT"
│                                      [CharacterSelectScreen]CharacterSelect / SelectCharacter
├── LeftPanel          (8,60,470,560)   MessageBox_BG 9-slice
│   ├── AccountBlock   (24,76,438,110)
│   │   ├── EmailLine  15 reg white (account email)
│   │   ├── GametimeLn 13 reg grey  [CharacterSelectScreen]TotalPlaytime / PlayTimeLeft
│   │   └── AccountBtn (24,150,120,28) small button "ACCOUNT" ([APBLoginScreen]AccountManagement)
│   ├── Divider        (24,196,438,2)
│   ├── CharsHeader    (24,208,438,24)  CharacterSelectIcon 16px + "CHARACTERS" 15 bold caps
│   ├── CreateRow      (24,238,438,30)  Menu_Button_Light amber — "CREATE CHARACTER"
│   │                                 [CharacterSelectScreen]CreateCharacter → OnCreateCharOpen
│   ├── CharList       (24,274,438,320) rows h=30, APB_List_Cell_NoBG_20 (+_Active)
│   │   per row: NameText 15 reg (empty slot = [CharacterSelectScreen]EmptyCharacter "Empty",
│   │   grey) · faction icon right 24px (Faction_Icon_*_32px) · selected row amber tint
│   ├── QuitBtn        (24,604,110,32)  [CharacterSelectScreen]ExitGame "Quit"
│   └── LogoutBtn      (142,604,110,32) [CharacterSelectScreen]Logout "Logout"
├── CharPreview        (560,80,900,560) 3D SceneCapture studio (existing) restaged center-right
├── NamePlate          (580,680,580,80) Menu_Button_On brush, dark
│   ├── CharName       (600,690,380,30) 20 bold caps white
│   ├── RatingLabel    (600,724,200,20) 13 reg grey  [CharacterSelectScreen]Rating "Rating:"
│   │                                 + [CharacterSelectScreen]Threat "Threat:"
│   └── RatingBadge    (1080,692,56,56) circle (BG_Button_Active_Ring) + threat number 20 bold
├── PlayBar            (580,772,580,44) Menu_Button_On, "PLAY" 15 bold caps centered
│                                     [CharacterSelectScreen]Play → DistrictSelect
└── FactionMini        (1176,776,32,32)+(1216,776,32,32) newCriminalcon / newEnforcerIcon
```

Notes: delete-character flow uses `DeleteCharacter_Scene_Preview` (small centered confirm
dialog, [CharacterSelectScreen]DeleteCheck). `Lobby` strings: ExitCheck / LogoutCheck for
confirm popups (PopUpMenu chrome + UI_Popup sfx).

## 5. DISTRICT SELECT — secondary detail (no 2011 preview found)

`[INFERRED]` from `[DistrictSelect_Action]` strings + district chrome + retail-era layout.
Geometry is approximate; M4b may adjust ±10% without spec amendment.

```
DistrictSelectStage
├── BgVideo            (0,0,1920,1080)  Generic_BG loop
├── Header             (16,12,700,40)   chip + "DISTRICT SELECT" 28 bold caps
│                                     [DistrictSelect_Action]DistrictSelect
├── TabRow             (16,64,600,36)   Tab_01_On/Off — "ACTION DISTRICTS" ([DistrictSelect_Action]Title)
│                                     · "SOCIAL" (future)
├── DistrictList       (220,120,1480,760)  3 rows, h=240, gap 20
│   └── Row[i]         APB_Button_DistrictSelect_Background brush
│       ├── Photo      (24,24,256,195)  FinancialDistrict_MainPhoto256x195 /
│       │                             SocialDistrict_MainPhoto256x195 /
│       │                             WaterfrontDistrict_MainPhoto256x195
│       ├── NameText   (300,30,400,32)  20 bold caps (Financial / Waterfront / Social)
│       ├── PopBars    (300,80,500,24)  StatusBar_BG + StatusBar_Bar fill per faction pop
│       ├── InstanceDD (300,130,360,32) APB_Button_Dropdown_DistrictSelect (+_Active/_Pressed/_Targeted)
│       └── JoinBtn    (1100,160,220,48) Menu_Button_On — "JOIN DISTRICT"
│                                     [DistrictSelect_Action]JoinDistrict
├── BackBtn            (24,1000,160,40) "BACK"
└── WorldIcon          header-right worldselecticon 32px
```

`WorldQueue_Scene_Preview` governs the **Loading/queue** stage: centered window (~0.33 w),
"WORLD QUEUE STATUS" title, spinner (LoadingArrows_Ring + LoadingIcon_MAIN), "Position in
Queue", "Estimated Wait Time", Cancel button. Loading screen art set:
`LoadingScreen_APB.png` (2011 stencil "APB / ALL POINTS BULLETIN" logo, white-on-alpha),
`LoadingScreen_Flames*` (+_Alpha/_Mask/_RimGlow/_Emburs), `LoadingScreen_FactionIcon_Crim/Enf`
(+ masks), `LoadingScreen_Overlay`.

## 6. SPLASH & boot flow

2011 order (pipeline doc §7): `GameFlowBase_Scene` (Generic_BG base) → `IntroMovieScene`
(FFullScreenMovieBink: SplashScreen.bik, then IntroTitles.bik, skippable) → `Login_Scene`.

UE 5.8 port:
- **SplashScreen.bik / IntroTitles.bik are Bink 1 → unplayable** (UE 5.8 BinkMedia is
  Bink2/.bk2-only; ffmpeg re-encode deferred — ledger `group:2011/Movies=manual`).
  **Fallback (M4b):** Splash stage shows `LoadingScreen_APB.png` centered on black with the
  LoadingArrows spinner for ~2.5 s (or any key), then straight to Login. If a future task
  re-encodes the biks to mp4, drop them into `Content\Movies\Login\` and play via the
  existing MediaPlayer path before Login.
- **Menu beds already converted** (WebM/MKV/MP4 in `Content\Movies\Login\`): Login_BG,
  Character_Select_BG, Faction_Select_Criminal/Enforcer_BG, Generic_BG. Playback = existing
  `UMediaPlayer` + `FileMediaSource` (Electra is the UE 5.8 default player; plugins already
  enabled — `APBFrontendWidget::ApplyStageBackgroundVideo` already implements the
  compat-mp4 → 4k-webm ladder; keep it).

Transition map: Splash →(auto/key) Login →(login ok, UI_SceneOpen sting) CharacterSelect
→(Play) DistrictSelect →(Join) WorldQueue/Loading → InDistrict. Each stage swap = bed video
swap + chrome rebuild; no camera moves in 2011 (beds provide all motion).

## 7. UI sfx mapping (from the 72 extracted WAVs)

The Wwise event→WAV binding is not recoverable from the banks (runtime log shows event
`Play_UIDefaultControlFocused` id 2345472777 on scene open). These are **best-match
assignments** — pragmatic, swappable via one table in M4b:

| Slot | WAV | Rationale |
|---|---|---|
| UI_Hover (focus) | `TabSound_10.wav` | short neutral tick (tab/focus family) |
| UI_Click (confirm) | `ButtonPos.wav` | \UISounds\8BitUISounds\Buttons\ButtonPos — positive button blip |
| UI_Back (cancel) | `Button2.wav` | HUD ItemAppear family, softer |
| UI_Error | `Error.wav` | StandardHUDMessages\ErrorBeep |
| UI_Popup (dialog open) | `PopUp.wav` | StandardHUDMessages\StandardBeep |
| UI_SceneOpen (sting) | `Positive3.wav` | StandardHUDMessages\PoitiveBeep |
| UI_ListSelect | `Spark.wav` | StandardHUDMessages\ItemAppear |
| UI_CharConfirm | `Positive.wav` | 8BitUISounds\Positive |
| UI_SliderTick | `Button4_616844292.wav` | small click |
| UI_Loading (queue ping, optional) | `CSABeep2.wav` | short beep |

All import to `/Game/Audio/UI` (§10) and play fire-and-forget via `UGameplayStatics::PlaySound2D`.

## 8. Music

Login/Lobby music: existing `/Game/Audio/LoginTheme_APBTheme1` (already wired in
`StartLoginMusic`) — keep. Licensed 2011 Music.pck tracks are a standing non-goal.

## 9. Mapping table — 2011 element → current `APBFrontendWidget` → change

| 2011 element | Current location | Change for M4b |
|---|---|---|
| Login_BG bed | `ApplyStageBackgroundVideo(Login)` | none (already correct) |
| Window panel brush | `ApplyPanelChrome` flat `APB_PANEL` color | use MessageBox_BG 9-slice brush texture (imported `/Game/Imported/UI/Menu2011/Chrome/MessageBox_BG`) |
| Cyan accent bar | `APB_PANEL_EDGE` cyan, `PanelAccentBar` | recolor to amber #FFC254; ideally swap to Window_Title_Accent_01 texture |
| Title "LOGIN" | `TitleText` stage Login | keep text (string key APBLoginScreen.Login), Arial Bold 20 caps, add BrandChip image left |
| Email/Password fields | `MakeTextField` flat style | brush → APB_BG_TextEntry; labels from ui_strings_2011 (already matching text) |
| Remember Me checkbox | — (absent) | add Check_True/False pair + RememberUserID string |
| Cancel/Login buttons | `MakeAccentButton` flat colors | Menu_Button_On/Off brushes; amber hover; add UI_Click/UI_Hover sounds |
| New Account / Settings row | LinkRow buttons | move to footer strip (frontendFooter) + add "REPLAY VIDEOS" stub |
| TOS scroll + Accept | — (absent) | new collapsed-by-default block (State A) wired to a `bFirstRun` flag |
| Character list card | CharacterSelect flat `CharSelectCard` | rebuild as LeftPanel per §4.1; amber selection via Menu_Button_Light |
| Faction icons | `TexFactionCrim/Enf` runtime `FImageUtils::ImportFileAsTexture2D` from loose files | switch to imported uassets `/Game/Imported/UI/Menu2011/CharSelect/*` (kill runtime file import) |
| Name plate + rating badge | — (absent) | add per §4.1; threat value from PlayerState |
| Play button | `UseExisting` "CONTINUE" | rename label to "PLAY" (CharacterSelectScreen.Play), restyle |
| District rows | DistrictSelect text list + `DistrictCombo` | rebuild per §5 with photos + dropdown brushes |
| Loading stage | flat panel | LoadingScreen_APB + flames + LoadingArrows spinner per §5 |
| Splash stage | black + auto timer | LoadingScreen_APB on black (Bink1 unplayable) |
| Fonts | `FCoreStyle` default | keep (Arial Bold ≈ atlas); sizes per §2.4 |
| Sounds | none | add §7 table via PlaySound2D |

## 10. Staging (Deliverable 2) — results

Import script: `tools\scripts\import_menu2011_assets.py` (UE Python, `unreal.AssetImportTask`,
headless run via `-run=pythonscript -nullrhi -unattended -nop4`).

Destinations:
- `/Game/Imported/UI/Menu2011/{Login,Chrome,CharSelect,DistrictSelect,Loading,Reference}`
- `/Game/Audio/UI`

Results (run 2026-07-19, first attempt, 0 errors):
- **123/123 textures imported** → `Content\Imported\UI\Menu2011\`:
  Login 14 · Chrome 52 · CharSelect 20 · DistrictSelect 12 · Loading 19 · Reference 6
- **12/12 UI sounds imported** → `Content\Audio\UI\`
- Failures: none. Machine report: `tools\menu2011_import_report.json`
  (per-file source → imported asset paths).

## 11. M4b watch-outs

1. `APBMenus_Art` in the **M3 MenuArt tree is incomplete (8 PNGs)** — the full 222-PNG dump
   is `LiveCurrentScene\png\packages\APBMenus_Art\...`; the import script sources chrome from
   there. (Ledger note added.)
2. Kill `ImportUiTex` runtime file imports for anything now imported as uassets — use
   `ConstructorHelpers`/soft object paths to `/Game/Imported/UI/Menu2011/...`.
3. `Menu_Button_On/Off` carry their shape in **alpha over black**; use them as
   `ESlateBrushDrawType::Image` with no tint multipliers that crush the gradient.
4. White-on-alpha chrome (APB_Window_*, Window_Title_Accent) is meant to be **tinted**;
   amber #FFC254 is the 2011 accent — not the current cyan.
5. Layout-math tests: login panel design size changes if State A (TOS) becomes default —
   update `DesignPanelSize("Login")` + `tests\` expectations together.
6. The `[CommonGameFlowScenes]` section referenced in the M4 brief does not exist in
   `ui_strings_2011.json` (sections written: APBLoginScreen, CharacterSelectScreen,
   CharacterCreateScreen, CharacterCustomisationScreens, DistrictSelect_Action,
   WorldSelectScreen, Marketplace) — common-flow strings live in `[APBLoginScreen]`.
7. Do not schedule the 2011-client capture as a blocking step; it's a 15-min manual
   verification for sign-off only.
