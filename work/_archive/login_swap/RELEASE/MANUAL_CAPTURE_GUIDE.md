# Manual uMod Hash Capture Guide

If the pre-hashed mod does not replace textures, follow these steps to capture the exact runtime hashes from the 2011 APB client.

## Why this matters

uMod identifies textures by a CRC32 hash of the locked D3D9 surface bytes. The exact hash depends on the runtime texture format and pitch. If our pre-computed hashes do not match, you can capture the live hashes and build a perfectly matched mod.

## Steps

1. **Copy uMod's `d3d9.dll` to the 2011 APB Binaries folder**
   ```
   copy "D:\APBReloaded\work\login_swap\mod\uMod\uMod\d3d9.dll" "D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\d3d9.dll"
   ```

2. **Start uMod as Administrator**

3. **Launch the 2011 APB client**
   ```
   "D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe"
   ```

4. **In uMod, add the 2011 APB game**
   - `Main` → `Add game`
   - Select `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe`

5. **Switch to the APB tab in uMod**

6. **Set a save path**
   - In the `Capture textures` tab, set `Save path` to an empty folder.

7. **Enable "Save single texture"**
   - Check `Save single texture`.
   - Set hotkeys for `Back`, `Save`, and `Next` (e.g., F1, F2, F3).

8. **Capture a texture**
   - In APB, navigate to the main menu.
   - Press your `Save` hotkey while the texture you want is visible.
   - uMod saves it as `<hash>.dds` in the save path.

9. **Compare with pre-computed hashes**
   - Open `D:\APBReloaded\work\login_swap\mod\APB_2011_Menu_Mod_hashed_v2\hash_report.txt`.
   - Find the retail texture name and its pre-computed hash.
   - If the captured hash differs, the runtime format is different.

10. **Build a matched mod**
    - Use `capture_and_build.ps1` in the v2 folder to map captured hashes to 2011 replacements automatically.

## Expected result

If the captured hash matches one of the pre-computed hashes in the v2 mod, the replacement will work. If not, the fallback hashes or the `capture_and_build.ps1` script will create a correctly matched mod.
