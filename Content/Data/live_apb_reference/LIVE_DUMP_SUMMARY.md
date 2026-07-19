# Live APB dump summary 2026-07-16T19:43:31.0610981+02:00

## Process (live)
- PID 53868 APB.exe ~2.9GB WS
- Window: APB Reloaded (64-bit, PC-D3D-SM3 at launch)
- Child: APB_Catcher.exe (EAC-related)
- TCP: ESTABLISHED to 104.18.125.108:443 (Cloudflare / EOS/auth CDN style)
- Steam client init failed in this environment (log: APB_Steam Failed to initialise Steam Client)

## Minidump
- procdump -ma/-mm FAILED: 0x800707D1 driver invalid / process appears protected (EAC/SecureEngine)
- Modules via API: empty; tasklist /M: N/A; handles: none exposed

## Live logs copied
- APBGame\Logs\Current.log (~1.1MB) - StatWatch + UI district streaming
- session_launch.log - short launch
- Config: APBEngine.ini, APBGame.ini, APBInput.ini

## Useful runtime observations for UE5 recreation
1. Login UI loads multi-level streaming packages:
   uidistrict_crimescene, login01, nightclub, skatepark, beachscene, posedcharacters, districtselect (+ artprops)
2. WorldInfo.StreamingLevels.Length = 16 on login map
3. Net ports from APBEngine.ini: Port=7777, PeerPort=7778, ServerBeaconPort=8777, BeaconPort=9777
4. NetServerMaxTickRate=30 (Social=20)
5. Texture streaming enabled; large static map pools for server
6. Frame spikes ~10-56ms with _Strm streaming component present

## For stats/lore
- Use apbdb API (already synced) + UPK extract — not PE strings / protected process memory

## Re-dump
```
powershell -File D:\APBReloaded\tools\scripts\live_dump_apb.ps1 -OutDir C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\live_dump
```
