# How to play APB Reloaded (offline private client)

## Launcher (easiest)

Double-click:

- `D:\APBReloaded\Launch_APB.bat` — **Editor -game** (real window + splash UI)
- `D:\APBReloaded\Create_Desktop_Shortcut.bat` — Desktop shortcut

**Do not use** bare `Binaries\Win64\APBReloaded.exe` alone — that Development target often runs headless (no window, ~50–130 MB). The launcher defaults to **Editor -game** instead.

### If you only see a black screen
1. Kill stuck processes: `Get-Process APBReloaded,UnrealEditor -EA SilentlyContinue | Stop-Process -Force`
2. Launch: `.\Launch_APB.ps1` (Editor mode)
3. Wait for window title like `APBReloaded (64-bit Development...)`
4. You should see a **dark blue full-screen menu** (not pure black), yellow/green stage text, and **CONTINUE** / login fields.
5. If still black, open `Saved\Logs\APBReloaded.log` and search for `APBFrontend` — you want `ShowFrontendUI widget=1 inViewport=1` and `UI_STAGE=Login`.

```bat
Launch_APB.bat              rem auto = editor -game (recommended)
Launch_APB.bat editor       rem UnrealEditor -game frontend UI
Launch_APB.bat staged       rem cooked package under Saved\StagedBuilds (has a window)
Launch_APB.bat game         rem raw Dev exe — NOT recommended (often no window)
Launch_APB.bat fullscreen
Launch_APB.bat editor 1280 720
```

PowerShell: `.\Launch_APB.ps1` or `.\Launch_APB.ps1 -Mode editor`

## Recommended launch (Editor game, full UI)

```powershell
& "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "D:\APBReloaded\APBReloaded.uproject" -game -windowed -ResX=1920 -ResY=1080 -log
```

Default map is **`Lvl_APB_Frontend`** with splash → login → character → district UI.

## Packaged / Development binary

```powershell
# After building APBReloaded Win64 Development:
& "D:\APBReloaded\Binaries\Win64\APBReloaded.exe" -windowed -log
```

If the window is black for a few seconds, wait for shaders/content load. Logs go to  
`D:\APBReloaded\Saved\Logs\APBReloaded.log`.

### Why a bare double-click might “do nothing”

1. Old packaged builds still used **Financial freeroam with no menus** and could sit on a huge city load.
2. Missing cooked content when launching a Staging stub without `Content` beside it.
3. Always launch from `D:\APBReloaded\Binaries\Win64\` after a Development build, **or** use the Editor `-game` line above.

## Classic login UI
- Dark navy centered panel (viewport-scaled; scrolls on short screens)
- Splash -> **Login** (username/password) -> Character select/create -> District
- Menu theme: `Content/Audio/LoginTheme_APB_ThemePreMaster.wav` — real APB login theme from Wwise `Play_NewThemeMusic` / `APB_ThemePreMaster` in `StreamedSFX.pck` (not DefaultMusicLibrary radio tracks)
- Moving classic San Paro backdrop (extracted scene_background TGA layers + parallax motion)
- Full Steam audio dump: `Content/Extracted/Audio/` (banks + StreamedSFX WEMs/WAVs); re-run `tools/WwiseExtract/dump_all_audio.py`
- **Audio world map** (where sounds play): `Content/Extracted/Audio/Catalog/AUDIO_WORLD_MAP.md` — regenerate with `python tools/WwiseExtract/audio_world_map.py`
- Classic nonlinear login stems (layer together): `Content/Extracted/Audio/InteractiveMusic/AmbientCharacterTheme/` (Strings, StringsLow, MnM, Guitar, Synth → `Play_ThemeMusicNonlinear`)
- **3D model viewer** (textured solid PSK/PSKX + package `Texture2D/Diffuse.tga`):  
  `tools/model_viewer/Launch_ModelViewer.bat` or `python tools/model_viewer/view_models.py`  
  Controls: **N/P** next/prev, **arrows** orbit, **+/-** zoom, **T** toggle texture, **W** wireframe (debug), **R** auto-spin
- **Steam → UE conversion spine**: `tools/convert/CONVERT.md` — `python tools/convert/steam_inventory.py`; PrivateServer opcodes `python tools/convert/parse_privateserver.py`

## Screen order

1. **Splash** — APB Reloaded intro (auto-advances ~2s or Continue)
2. **Login** — Register / Login (offline Domain account; default `player1` / `password`)
3. **Character** — Create new (full multi-slot editor) or continue with active
4. **Character Create** — Name, Enforcer checkbox, body height/build, **7 clothing slots** from catalog
5. **District Select** — Financial, Waterfront, Asylum, Beacon, Crate, Social, …
6. **Enter District** — loads freeroam map with HUD, meshes, bots, vehicles, mailbox/ammo/resupply

## Freeroam controls

| Input | Action |
|-------|--------|
| WASD | Move |
| Mouse | Look |
| LMB | Fire catalog weapon |
| E | Enter nearest vehicle |
| F | Interact (mailbox / ammo / resupply / contact) |
| Space | Jump |

## Freeroam HUD

Shows faction, cash/G1C, threat, inventory slots, mission stage.

## Automated proof (cold-start Frontend map — required)

Must launch the **default Frontend map** (no Financial freeroam override). Two gates share the same menu sequence (splash -> login -> character -> body -> district -> travel dispatch):

- `frontend_menu` (**M4 gate**): validates every menu stage through the district-select travel dispatch, then stops. Independent of world content, so it passes today. Both gates now call `RequestEngineExit` after their terminal verdict, so the editor self-exits instead of hanging.
- `frontend_flow` (**M9 geometry + M12 vehicles integration gate**): runs the menu sequence, then `OpenLevel` to freeroam and asserts world props + walk/shoot/interact/vehicle. Its `FRONTEND_FLOW_OK` verdict needs walkable geometry and vehicles, so it stays red until those milestones land.

```powershell
$env:APB_SCRATCH = "C:\Users\Support\AppData\Local\Temp\grok-goal-5b882b537032\implementer"
# M4 menu gate (default GameDefaultMap Lvl_APB_Frontend — no map argument):
& "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" `
  "D:\APBReloaded\APBReloaded.uproject" -game `
  -APBProbe=frontend_menu -APBScratch=$env:APB_SCRATCH -nullrhi -nosplash -nosound -unattended -log

# Full integration gate (swap the mode; needs M9 geometry + M12 vehicles to pass):
# ... -APBProbe=frontend_flow ...
```

Expect in `{SCRATCH}/frontend_menu.log` (M4):
- `COLD_START_OK frontend_map=1` / `MAP_COLD map=...Frontend...`
- `login_fail ...` then `login_ok stage=CharacterSelect`
- `BODY height=... bulk=...` · `APPEARANCE slots_equipped=7`
- `DISTRICTS count=8` · `DISTRICT_ENTER ok=1 district=Financial`
- `TRAVEL_OPENLEVEL_CALLED` then `FRONTEND_MENU_OK` (editor exits)

Expect additionally in `{SCRATCH}/frontend_flow.log` (M9+M12):
- `MAP_AFTER_TRAVEL map=...Financial...`
- `WORLD_PROPS bots>0 mailbox>0 ammo>0 resupply>0 vehicles>0`
- `WALK_OK=1` · `SHOOT` · `INTERACT` · `VEHICLE enter=1`
- `FRONTEND_FLOW_OK`

The full spine (`tools/run_verification_gates.ps1`) hard-gates `frontend_menu` and only runs `frontend_flow` under `-IntegrationGate`.

