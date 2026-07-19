# umodel vs 2011 package version 547 — M3 spike verdict

Date: 2026-07-19 · Tool: `tools\UEViewer\umodel_64.exe` (`-game=apb`)
Sources (read-only): `2011 apb\APB All Points Bulletin\APB North America\` (pkg ver **547/31**),
retail Steam build (564/33, known-good per `tools\ASSET_PIPELINE.md`).

## Verdict: WORKS

umodel reads and exports 2011's version-547/31 packages. No version-override flags needed.

### Evidence

1. **Pre-existing full Interface dump** (`Content\Extracted\2011\UI\umodel\export_manifest.json`):
   94 packages attempted, **94 ok / 0 fail**, 2,407 TGAs. Includes every M3 target:
   `APBMenus_Art`, `APBMenus_Art_Jonathan`, `APBMenus_Font`, `APBMenus_FrontEnd`,
   `APBMenus_Skins` — all `ok: true`. Earlier probes in
   `Content\Extracted\2011\EXTRACTABILITY_INVENTORY.md` (weapons/characters/vehicles at
   547/31) also exited 0.
2. **Fresh M3 probe (this spike)**:
   - `APBMenus_Art.upk` loads: `Ver: 547/31 Engine: 3908 Names: 608 Exports: 1900
     Imports: 153 Game: 80001D`. The `StrProperty: unknown UMaterial3 RWTGuid` /
     `ArrayProperty: unknown UMaterial3 Channels` / `ByteProperty: unknown UTexture2D
     LODCategory` lines are benign unmapped-property warnings, not read failures.
   - `APBMenus_Font.upk` headless export (`-export`, temp dir): **Exported 11/11 objects,
     exit 0**, 11 TGAs written.
3. **Batch result**: `tools\scripts\export_2011_menu_art.py` converted the full
   `APBMenus_Art*` + Font/FrontEnd/Skins set to PNG at
   `Content\Extracted\2011\MenuArt\` — **77 packages, 2,085 PNG, 0 failures**
   (`menuart_manifest.json`).

Attempts used: 1 of 2 (no retry needed). The screenshot-measure rebuild fallback is NOT
required for menu art (it remains the fallback of record for UIScene *layout*, which
umodel cannot express — see plan D2).

## Splash/login video verdict (M3 task 3 — recorded here per task spec)

- **ffmpeg: unavailable.** Not on PATH; not bundled under `tools\`; not in UE 5.8
  `Engine\Binaries\ThirdParty\` (only RAD `Bink2ForUnreal.exe` + Premiere plugin there).
- **UE 5.8 Bink plugin: present but incompatible with the 2011 files.**
  `D:\UE58\UE_5.8\Engine\Plugins\Media\BinkMedia\` (v2.0, disabled by default) is
  **Bink 2 only** — its factory registers `bk2;Bink 2 Movie File` and the editor filter
  is `Bink 2 files (*.bk2)`. The 2011 videos are **Bink 1** (magic `BIKi`,
  verified in file header) → BinkMedia **cannot play them directly**.
- **What was done instead**: bit-exact copies of `SplashScreen.bik` (15,963,092 B) and
  `IntroTitles.bik` (39,627,064 B) at `Content\Extracted\2011\Movies\`.
- **Conversion paths forward (M4 decision)**:
  1. Re-encode `.bik` → `.bk2` with the bundled RAD tool
     `D:\UE58\UE_5.8\Engine\Binaries\ThirdParty\Bink\Bink2ForUnreal.exe` (GUI), then
     BinkMedia plays them in-engine.
  2. Obtain ffmpeg (bink1 decoder) and produce mp4/Electra as originally planned.
  3. Note: the 2011 *login animated backgrounds* (TextureMovie set, different assets)
     were already converted to WebM/MKV in earlier work under
     `Content\Extracted\2011\LoginAnimatedBackground_*`, so menu background motion is
     not blocked by this gap.
