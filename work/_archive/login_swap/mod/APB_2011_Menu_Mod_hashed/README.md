# APB 2011 Main Menu Texture Mod (Hash-Prebuilt)

This mod replaces the retail APB Reloaded login/main menu textures with the original 2011 versions using **uMod** (Universal Modding Engine).

## What is included

- 29 replacement textures already renamed to their retail uMod hashes
- Classic 2048x2048 `Constant_BG.dds` (the original login background)
- Classic faction icons, avatars, loading screen art, flames, etc.

## How the hashes were computed

uMod/TexMod identifies textures by a CRC32 hash of the first
`BitsPerPixel * Width * Height / 8` bytes of the locked D3D9 surface.

This package was built by:
1. Exporting the retail `APBMenus_Art_GameFlowScenes.upk` textures with UE Viewer.
2. Computing the uMod CRC32 on the raw BGRA pixel data.
3. Renaming the 2011 replacement DDS files to `<hash>.dds`.

The hash function is:
```cpp
#define CRC32POLY 0xEDB88320u
unsigned int hash = 0xFFFFFFFF;
for each byte:
  for each bit:
    bit = (hash ^ byte) & 1;
    hash >>= 1;
    if (bit) hash ^= CRC32POLY;
    byte >>= 1;
```

## Prerequisites

- Windows 10/11
- DirectX End-User Runtime (legacy) — provides `D3DX9_43.dll`
- APB Reloaded (retail Steam client)
- uMod v2.0 Alpha r53 (included in `../uMod/`)

## Installation & usage

1. **Install uMod**
   - Extract `uMod_alpha_v2_r53.zip` from `../uMod/`.
   - Run `uMod.exe` as Administrator.

2. **Add APB to uMod**
   - In uMod: `Main` → `Add game`
   - Select: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries\APB.exe`

3. **Launch APB through Steam**
   - Start uMod first, then launch APB Reloaded from Steam normally.
   - A new tab for APB should appear in uMod.

4. **Load the mod**
   - In uMod, right-click in the mod list area and `Add texture/package`.
   - Select all `<hash>.dds` files in this folder.
   - Click `Update`.

## Files

- `<hash>.dds` — 2011 replacement textures, pre-hashed for uMod
- `hash_report.txt` — mapping of retail texture names to hashes
- `Launch_APB_with_uMod.bat` — helper to start uMod
- `README.md` — this file

## Risks

- uMod injects a DLL into the game process. While it only replaces textures, anti-cheat systems (GFAC/SARD) may detect it.
- Little Orbit's official policy states that modifying game files is bannable. Runtime tools are a gray area.
- **Use at your own risk. Consider testing on a throwaway Steam account first.**

## If the pre-hashed mod does not work

Some textures may use a different locked surface format at runtime. Use the included `capture_and_build.ps1` script:

```powershell
.\capture_and_build.ps1
```

This will:
1. Start uMod and launch APB.
2. Let you capture the exact runtime hashes.
3. Automatically build a new `APB_2011_Menu_Mod_live` folder with correctly renamed replacements.

## Troubleshooting

- **uMod shows "No Injection: DX"**: Make sure APB is using D3D9. Run uMod as Administrator.
- **Game crashes on launch**: Try a different injection method (copy `d3d9.dll` from uMod into `APB Reloaded\Binaries` instead of using `Add game`).
- **Textures do not replace**: The computed hash may differ from the runtime hash if the locked surface format is not BGRA. Use `capture_and_build.ps1` to capture the exact hashes.

## Verification status

- ✅ uMod CRC32 hash algorithm verified against standard CRC32 test vectors.
- ✅ 2011 APB client launches successfully with uMod's `d3d9.dll` injected.
- ⚠️ Retail client testing was not performed to avoid anti-cheat risk.

## Credits

- Textures extracted from the original 2011 APB client.
- uMod by the TexMod/uMod community.
