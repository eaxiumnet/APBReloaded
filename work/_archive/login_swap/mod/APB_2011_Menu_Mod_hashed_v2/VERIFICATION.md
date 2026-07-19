# Verification Status

## What has been verified

1. **uMod hash algorithm correctness**
   - Source: `D:\APBReloaded\work\login_swap\tools\umod_src\uMod_DX9\uMod_TextureFunction.cpp`
   - Implementation: `D:\APBReloaded\work\login_swap\scripts\umod_hash.py`
   - Verified against standard CRC32 test vector "123456789":
     - uMod CRC32: `340BC6D9`
     - Standard CRC32: `CBF43926`
     - Relationship: `340BC6D9 == CBF43926 XOR 0xFFFFFFFF`
   - Result: ✅ Algorithm is correct.

2. **Retail client uses Direct3D 9**
   - Source: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Logs\Current.log`
   - Evidence:
     - `DriverName: atidx9loader64.dll`
     - `AllowD3D10=false`
   - Result: ✅ uMod can hook the retail client.

3. **2011 client launches with uMod injected**
   - uMod's `d3d9.dll` was copied to `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\`.
   - 2011 APB client launched successfully and reached the main menu.
   - Result: ✅ uMod injection works with APB's D3D9 renderer.

4. **Retail menu textures exported**
   - Source: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages\Interface\APBMenus_Art_GameFlowScenes.upk`
   - Tool: UE Viewer (`umodel_64.exe`)
   - Result: ✅ 41 exports including all menu textures.

## What has NOT been verified

1. **Live retail hash capture**
   - uMod's live texture capture requires manual GUI interaction.
   - Retail testing was not performed to avoid GFAC/SARD flagging the user's account.
   - Mitigation: The mod includes fallback hashes for multiple D3D9 layouts (BGRA, RGBA, BGR, RGB).

2. **Anti-cheat reaction**
   - uMod injects a DLL into the game process.
   - GFAC/SARD may detect this and take action.
   - Mitigation: User is instructed to test on a throwaway Steam account first.

## Known limitation

uMod computes its texture hash from the **locked D3D9 surface**. APB may store menu textures in DXT-compressed or other GPU-native formats. UE Viewer exports decompressed TGA images, but uMod hashes the raw locked bytes, which may be DXT blocks. Our pre-computed hashes assume decompressed BGRA/RGBA/RGB/BGR layouts and will not match if the runtime surface is DXT-compressed.

This is a fundamental limitation of pre-computing hashes from exported images. The v2 mod includes fallback hashes for multiple decompressed layouts, but the only reliable method is live capture with uMod.

## Conclusion

The mod is technically sound and ready to test. The pre-hashed files are a best-effort starting point; if they do not match, the user should follow `MANUAL_CAPTURE_GUIDE.md` to capture exact runtime hashes.
