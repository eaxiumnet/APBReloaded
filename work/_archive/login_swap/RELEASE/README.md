# APB 2011 Main Menu — Release Package

This package contains the only known working way to experience the original 2011 APB main menu: **launching the original 2011 APB client**.

## Important: retail client cannot be modded

After extensive testing, every path to use the 2011 menu inside the retail APB Reloaded client is blocked:

| Approach | Result |
|---|---|
| Direct `.apb` swap | Blocked — UE3 version mismatch (547/31 vs 564/33) |
| Package rebuild | Blocked — no APB editor available |
| UPKUtils texture injection | Blocked — rejects APB's 564/33 format |
| Runtime texture replacement (uMod) | **Blocked by GFAC/SARD anti-cheat** — custom `d3d9.dll` refused to load |

The retail Steam client is locked down. The only safe, working option is the original 2011 client.

## Files

| File | Purpose |
|---|---|
| `Launch_2011_APB.bat` | One-click launcher for the 2011 APB client |
| `Launch_2011_APB.py` | Python launcher with automatic OK-click on the startup popup |
| `APB_2011_Menu_Mod_hashed_v2.zip` | uMod texture-replacement attempt (best-effort, retail-blocked) |
| `uMod_alpha_v2_r53.zip` | uMod v2.0 Alpha r53 (D3D9 texture injector) |
| `MANUAL_CAPTURE_GUIDE.md` | Manual uMod hash capture guide |
| `TECHNICAL_NOTES.md` | Hash algorithm details and limitations |
| `TEST_REPORT.md` | Verification report |

## How to use the 2011 client launcher

1. Double-click `Launch_2011_APB.bat`.
2. The launcher starts the original 2011 APB client.
3. A harmless "Ambiguous package name" dialog appears; the launcher clicks OK automatically.
4. Wait for the classic 2011 main menu to appear.

## What if I still want to try retail modding?

You can try the uMod-based mod at your own risk, but be aware:

- GFAC/SARD will likely block uMod injection.
- **Use a throwaway Steam account.**
- See `MANUAL_CAPTURE_GUIDE.md` for the manual hash capture workflow.

## Verification

- ✅ 2011 APB client launches and reaches the main menu.
- ✅ Launcher automatically handles the startup popup.
- ⚠️ Retail client modding blocked by anti-cheat.

## Credits

- 2011 textures and client from the original APB release.
