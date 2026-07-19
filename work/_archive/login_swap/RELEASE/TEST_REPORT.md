# APB 2011 Main Menu — Test Report

## Test environment

- OS: Windows 11
- GPU: AMD Radeon RX 7900 XTX
- Retail APB Reloaded: Steam client, D3D9 confirmed
- 2011 APB client: `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe`
- uMod: v2.0 Alpha r53

## Tests performed

### 1. 2011 APB client launch with automatic popup handling

- Launcher: `Launch_2011_APB.py`
- Result: ✅ PASS
- Details:
  - Game starts.
  - Popup appears: "Ambiguous package name: Using ...\unpacked\APBStart.apb, not ...\APBStart.apb"
  - Launcher clicks OK automatically.
  - Main window title becomes "APB All Points Bulletin".
  - Classic 2011 main menu is reachable.

### 2. Retail client modding attempts

| Approach | Result |
|---|---|
| Direct `.apb` swap | Blocked — UE3 version mismatch (547/31 vs 564/33) |
| Package rebuild | Blocked — no APB editor available |
| UPKUtils texture injection | Blocked — rejects APB's 564/33 format |
| uMod runtime texture replacement | **Blocked by GFAC/SARD** — custom `d3d9.dll` refused to load |

### 3. uMod hash algorithm verification

- Source: `uMod_IDirect3DTexture9::GetHash()` in uMod source.
- Verified against standard CRC32 test vectors.
- Result: ✅ PASS

## Conclusion

The only working way to experience the 2011 APB main menu is to launch the original 2011 client. The retail client is protected by GFAC/SARD and cannot be safely modded.
