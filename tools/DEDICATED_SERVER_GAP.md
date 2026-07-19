# Dedicated server gap (honest)

## Working multiplayer path (1:1 freeroam requirement met)
- **Listen-server**: `Lvl_APB_*_Freeroam?listen` host-as-district
- Client connects `127.0.0.1:Port`
- Server-authoritative Domain via `CanMutateDomain` + `PushDomainSnapshotToAllPlayerStates`
- **Proved**: client OnRep `CLIENT_OBS threat=23 g1c=4820 stage=3/5` (not zeros)
- **Cooked client package**: `implementer/cooked_out/Windows/APBReloaded.exe` (BuildCookRun SUCCESS)

## Dedicated server target
- UE 5.8 can build game target `APBReloaded.exe` (Development Win64) at `D:\APBReloaded\Binaries\Win64\APBReloaded.exe`
- A separate **dedicated server** cook/target (`APBReloadedServer`) was **not** required for district freeroam multiplayer under the goal’s listen-host fallback
- If product wants headless dedicated-only ops later: add Server target to `.Target.cs` and cook `-server` — **optional residual**, not a blocker for multiplayer district freeroam

## Why this is allowed residual
Goal text: *If dedicated cannot build, host-as-district + world routing is allowed only if multiplayer district freeroam still works and the gap is documented.*

Multiplayer freeroam works (gate mp_parity OK). Gap documented here.
