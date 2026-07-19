# APB 2011 Main Menu Texture Mod

This mod replaces the retail APB Reloaded login/main menu textures with the original 2011 versions using **uMod** (Universal Modding Engine). uMod is a runtime texture replacement tool that does not modify game files.

## What is included

- All 2011 `APBMenus_Art_GameFlowScenes` textures converted to DDS
- Classic 2048x2048 `Constant_BG.dds` (the original login background)
- Classic faction icons, avatars, loading screen art, flames, etc.

## How it works

uMod intercepts DirectX 9 texture calls and replaces textures by matching their hash. Because texture hashes must be captured while the game is running, this package contains the replacement art and a workflow for you to bind it to the retail textures.

## Prerequisites

- Windows 10/11
- DirectX End-User Runtime (legacy) — provides `D3DX9_43.dll`
- APB Reloaded (retail Steam client)
- This mod folder

## Installation & usage

1. **Install uMod**
   - Extract `uMod_alpha_v2_r53.zip` (included in `../uMod/`).
   - Run `uMod.exe` as Administrator.

2. **Add APB to uMod**
   - In uMod: `Main` → `Add game`
   - Select: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries\APB.exe`

3. **Launch APB through Steam**
   - Start uMod first, then launch APB Reloaded from Steam normally.
   - A new tab for APB should appear in uMod.

4. **Capture retail menu texture hashes**
   - In uMod, go to the APB tab.
   - Enable `Save single texture` and set a capture folder.
   - In APB, navigate to the login/main menu.
   - Use your configured hotkeys to scroll through and save the textures you want to replace.
   - uMod saves them as `<HASH>.dds`.

5. **Map 2011 replacements to captured hashes**
   - For each captured retail texture, identify which 2011 texture should replace it.
   - Rename the matching 2011 DDS file to use the captured hash.
   - Example: if uMod captured the background as `A1B2C3D4.dds`, rename `Constant_BG.dds` to `A1B2C3D4.dds`.
   - A helper script `rename_by_hash.ps1` is provided to make this easier.

6. **Load the mod**
   - In uMod, right-click in the mod list area and `Add texture/package`.
   - Select the renamed DDS files.
   - Click `Update`.

## Helper script

`rename_by_hash.ps1` helps map captured hashes to 2011 textures interactively.

```powershell
.\rename_by_hash.ps1
```

Follow the prompts: enter the captured hash, then select the 2011 texture to rename.

## Risks

- uMod injects a DLL into the game process. While it only replaces textures, anti-cheat systems (GFAC/SARD) may detect it.
- Little Orbit's official policy states that modifying game files is bannable. Runtime tools are a gray area.
- **Use at your own risk. Consider testing on a throwaway Steam account first.**

## Troubleshooting

- **uMod shows "No Injection: DX"**: Make sure APB is using D3D9. Check that you launched uMod as Administrator.
- **Game crashes on launch**: Try a different injection method (copy `d3d9.dll` into `APB Reloaded\Binaries` instead of using `Add game`).
- **Textures do not replace**: The captured hash must exactly match the retail texture. Verify the renamed file uses the correct hash.

## Files

- `*.dds` — 2011 replacement textures
- `rename_by_hash.ps1` — interactive hash-to-texture mapper
- `README.md` — this file

## Credits

- Textures extracted from the original 2011 APB client.
- uMod by the TexMod/uMod community.
