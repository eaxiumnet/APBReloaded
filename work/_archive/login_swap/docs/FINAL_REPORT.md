# APB Nostalgia Main Menu — Final Diagnosis

## What you actually want
Experience the original 2011 APB login/main menu again.

## What we did
1. Located both packages:
   - 2011: `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\APBGame\Content\Maps\APBLoginLevel.apb` (11,396 bytes, compressed, UE3 version 547/31)
   - Retail: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps\APBLoginLevel.apb` (48,363 bytes, uncompressed, UE3 version 564/33)
2. Decompressed both with UE Viewer's `decompress.exe`.
3. Listed exports/imports with `umodel_64.exe -list`.
4. Confirmed the 2011 client still launches and reaches the original menu.

## Key finding: the 2011 client WORKS
Launching `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe` brings up:
- Window title: `Release_USER (Unity Build) 1-1-0-STAGING@534926`
- Then: `SplashScreen`
- Then: `APB All Points Bulletin` (the original main menu)

Log confirms it loads `APBLoginLevel.apb` and initializes the original GameFlow scenes (`GameFlowBase_Scene`, etc.).

## Why copying into retail will NOT work
1. **Format version mismatch**: 2011 is UE3 `547/31`, retail is `564/33`. The retail engine expects extra header fields (an additional `FGuid` after generations for licensee >= 32).
2. **Asset references differ**: The 2011 level is a tiny Kismet shell; the retail level references modern classes like `cAction_WwiseFadeInEnvironment`, `cPlaceholderSkeletalMesh`, `cSeqAct_ControlGameMovie`, etc., which do not exist in the 2011 package.
3. **Anti-cheat**: Retail uses GFAC/SARD. Modified `.apb` files will fail integrity checks and risk account action.

## The realistic solution
Run the 2011 client directly. It is already on your disk and fully loads the nostalgia menu.

### How to launch
1. Open a command prompt or PowerShell.
2. Run:
   ```powershell
   cd "D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries"
   .\APB.exe
   ```
3. Wait ~30 seconds. You will see the original splash screen, then the original main menu.

### Cleanup note
During testing, `decompress.exe` created an `unpacked\` folder under `Maps\`. The 2011 engine detected it and logged an ambiguous-package warning. To avoid any confusion, remove it:
```powershell
Remove-Item -Recurse -Force "D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\APBGame\Content\Maps\unpacked"
```

## If you still want to try retail injection
It is not recommended, but the only even theoretically possible path would be:
1. Rebuild the 2011 `APBLoginLevel.apb` content inside the retail package format (version 564/33).
2. Update all class references to retail equivalents.
3. Bypass or disable GFAC/SARD (not possible without risking bans or using a private-server build).

This is essentially a full rebuild, not a copy-paste, and is beyond safe modding.

## Conclusion
The nostalgia menu is alive and well in your 2011 install. Use that. The retail client is locked behind format differences and anti-cheat, so a direct swap is impossible.
