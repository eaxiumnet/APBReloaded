# APB private-server / emulator research vs local rebuild

Research date: 2026-07-17  
Workspace: `D:\APBReloaded`  
Local Steam tree: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\ApbPrivateServer`  
2011 RTW client: `D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America`

This note compares **public emulator lineages** to the **local ApbPrivateServer** and maps multi-process roles onto the **UE5 Domain rebuild**.

---

## Sources (URLs)

| # | Project / thread | URL | Notes |
|---|------------------|-----|--------|
| 1 | **ivan-draga/rAPB** (current public GitHub) | https://github.com/ivan-draga/rAPB | C#/C++ emulator; claims client **1.19.4.775380** (modern Reloaded); README: *“You cannot enter districts yet”*; setup wiki link → fiki574 |
| 2 | **fiki574 / rebornAPB lineage** (Ryderizm mirror) | https://github.com/Ryderizm/rAPB | Emulator for **v1.4.1.555239**; founder credit **fiki574**; tools + client hooks; districts not enterable; historical **www.apbemu.com** / APBX |
| 3 | **fiki574 rAPB wiki** (setup help target) | https://github.com/fiki574/rAPB/wiki | Linked from ivan-draga README; wiki empty/unavailable when fetched (2026-07) |
| 4 | **RaGEZONE — APB Emulator 2013 Files** | https://forum.ragezone.com/threads/apb-emulator-2013-files.963326/ | Source + DB dump era; Lobby / World / **District** processes; ports 2101/2106/2108/1031; MySQL `apbdb`; download links often **login-gated or dead** |
| 5 | **RaGEZONE — rebornAPB development** | https://forum.ragezone.com/threads/development-rebornapb-all-points-bulletin-emulator-for-v1-4-1-55239.1091992/ | Open collab thread; login→world pass-through; district spin-up goals |
| 6 | **Steam group APBEMU / APBX** | https://steamcommunity.com/groups/apbemu | Community hub for “APBX” full emulator goal (historical) |
| 7 | **AllPrivateServer.com** (copyright header in local tree) | http://AllPrivateServer.com/ | Branded in `ApbPrivateServer` sources (2011 APS); site historical |

---

## Local ApbPrivateServer layout (what you already have)

Path: `...\APB Reloaded\ApbPrivateServer\`  
Solution: `ApbEmu.sln`  
Copyright: **APS / AllPrivateServer.com (2011)** — same family as RaGEZONE 2013 “French FrameWork.dll” stack.

| Process | Binary (Release) | Role |
|---------|------------------|------|
| **LobbyServer** | `LobbyServer.exe` | Login puzzle/salt/proof, character list/create/delete, world list, world enter, config file load/save |
| **WorldServer** | `WorldServer.exe` | World enter + large opcode surface (district reserve/enter, chat, clan, mail, marketplace stubs) |
| **CharacterServer** | `CharacterServer.exe` | Character DB RPC companion |
| **FileServer** | `FileServer.exe` | Client file / block config serving |

### Lobby opcodes (local `LobbyServer/TCP/Opodes.cs`)

Client→server: `ASK_LOGIN (0x3E8)`, `LOGIN_PROOF`, `ASK_CHARACTER_INFO`, `ASK_WORLD_LIST`, `ASK_CHARACTER_*`, `ASK_WORLD_ENTER`, `ASK_CONFIGFILE_*`  
Server→client: `LOGIN_PUZZLE`, `LOGIN_SALT`, `ANS_LOGIN_SUCCESS` / `ANS_LOGIN_FAILED`, `CHARACTER_LIST`, `WORLD_LIST`, `ANS_WORLD_ENTER`, …

### World opcodes (local `WorldServer/Tcp/Opcodes.cs`)

Includes full catalog: `ASK_DISTRICT_RESERVE/ENTER/EXIT`, queues, group/clan/mail/marketplace, population, GM — **opcode map is rich**; many handlers are stub/partial (typical of 2011–2013 dumps).

**Missing vs RaGEZONE “full” 2013 description:** dedicated **DistrictServer.exe** project is **not** present in this tree (only Lobby/World/Character/File). World has district *opcodes* but no separate UE3 district host process.

---

## Project comparisons

### 1) ivan-draga/rAPB

| Capability | rAPB (ivan-draga) | Local ApbPrivateServer | Verdict |
|------------|-------------------|------------------------|---------|
| Open source on GitHub | Yes (GPL-3) | Yes (local copy; APS headers) | — |
| Target client version | **Modern Reloaded 1.19.x** | Older / classic packet set (2011 APS) | **rAPB further for modern client** |
| Lobby / login | Emulator tree includes LobbyServer | Implemented (ASK_LOGIN path) | Similar role |
| World list / enter | Present in lineage | Present | Similar |
| **District enter** | README: **not yet** | No DistrictServer process; world district ops partial | **Neither fully ships district gameplay** |
| Tools / research dumps | Tools + Other dirs | Minimal | **rAPB further** (tooling) |
| DB | MySQL-style in lineage | MySQL configs present | Similar |

**Verdict vs local: FURTHER for modern Reloaded protocol + tooling; NOT further for playable district simulation (explicitly incomplete).**

### 2) Ryderizm/rAPB (fiki574 rebornAPB)

| Capability | rebornAPB | Local | Verdict |
|------------|-----------|-------|---------|
| Version focus | **v1.4.1.555239** classic-ish Reloaded | APS 2011-style | rebornAPB more documented community path |
| Districts | Cannot enter yet | Same gap | Tie on district |
| Client DLL / hooks | Emphasized in README | Not in local tree | **Further** (client injection / research kit) |
| Credits / continuity | fiki574 founder; APBEMU/APBX | AllPrivateServer APS | Different brand, same problem space |

**Verdict: FURTHER than local on client-side research tooling and documented setup intent; NOT a complete district grid.**

### 3) RaGEZONE 2013 APB emulator (Dyox / BlueType release thread)

| Capability | 2013 thread claims | Local ApbPrivateServer | Verdict |
|------------|--------------------|------------------------|---------|
| Lobby ~95% | Login, chars, world enter | Matches structure (LobbyServer packets) | Local is **this stack’s descendant/sibling** |
| World ~10% | Partial | Large opcode enum, limited logic | Similar maturity class |
| **DistrictServer** | Dedicated exe + player-hosted instances | **Absent** as project | **2013 design further in process split** |
| Working list (claimed) | Login, chars, world, district *list*, friends base, mail base, “Connecting to District…” | Local implements subset in C# | Thread **claims more productized packaging** (SQL, 3 exes) |
| Artifacts today | Mega/magnet **login-gated or dead** | **On-disk, buildable** | **Local further for availability** |

**Verdict: Conceptually further (DistrictServer + hosting); artifact-wise local tree is more usable if 2013 downloads are dead. Do not assume binary parity without building both.**

### 4) APBX / APBEMU (Steam group)

| Capability | APBX goal | Local | Verdict |
|------------|-----------|-------|---------|
| Full private grid | Project goal | Partial lobby/world | APBX **aspired further**; public code not as complete as marketing |
| Community | Steam group | Fork of APS | Historical pointer only |

**Verdict: NOT further as a consumable OSS tree today** (community pointer; use rAPB + local sources instead).

### 5) UE5 rebuild Domain (`D:\APBReloaded`) — offline private recreation

| Capability | Domain / frontend | Protocol emulators |
|------------|-------------------|--------------------|
| Login register/fail | `LoginService` + classic UMG login | Packet SRP/puzzle |
| World list / enter | `WorldDirectory` / `EnterWorld` | Lobby WORLD_LIST / WORLD_ENTER |
| District reserve/join | `DistrictRouter` + freeroam maps | World/District opcodes + UE3 DS |
| Catalog / combat / missions | Data-driven Domain services | Mostly unimplemented in open emus |
| Live Steam client compatibility | **Not** the goal | Primary goal of rAPB |

**Verdict: Further for playable offline nostalgia loop in UE5; not further for authentic live-client multiplayer protocol.**

---

## Server-role gap table (intended multi-process map)

| Role | Classic / emu design | Local ApbPrivateServer | UE5 rebuild today | Deferred |
|------|----------------------|------------------------|-------------------|----------|
| **Login / Lobby** | LobbyServer TCP; salt/puzzle/proof; char select | Yes — LobbyServer + ASK_LOGIN… | Domain `LoginService` + frontend Login stage | Live SRP6a / puzzle against Steam client |
| **World list / enter** | Lobby WORLD_LIST → WorldServer | Yes — WORLD_LIST, ASK_WORLD_ENTER | `WorldDirectory` + `EnterWorld("W1")` | Multi-world registration from remote hosts |
| **Character** | Char create/delete/info + CharacterServer | Yes — CharacterServer + lobby packets | Domain character create + customization | Full live wardrobe protocol |
| **File** | FileServer config/blocks | Yes — FileServer | Local JSON under `Content/Data` | Live client file protocol |
| **District** | DistrictServer (UE3 dedicated) | Opcodes on World only; **no DS project** | Freeroam listen maps + Domain district session | Full multi-process district grid, opposition AI parity, mission scripting host |
| **Voice / AC** | Vivox / PunkBuster era | Not required for private | N/A | Out of scope |

### Intended nostalgia flow (rebuild)

```
Splash → Login (theme + credentials)
  → Register/Login via Domain LoginService
  → Character select/create
  → District select
  → Load freeroam map (listen server path)
```

Classic emu flow (for comparison):

```
Launcher → Lobby (login/chars/world list)
  → WorldServer (social, reserve district)
  → DistrictServer (UE3 gameplay)
```

---

## “Further than local ApbPrivateServer?” — summary

| Project | Further than local? | Why |
|---------|---------------------|-----|
| **ivan-draga/rAPB** | **Yes (modern client + tools)** / **No (districts)** | Targets current Reloaded build; districts still blocked |
| **fiki574 / Ryderizm rAPB** | **Yes (client hooks / docs intent)** | Classic 1.4.x focus + research kit; districts blocked |
| **RaGEZONE 2013 full package** | **Yes (DistrictServer design)** / **No (downloads)** | Process model richer; artifacts often unavailable |
| **APBX Steam / AllPrivateServer site** | **No as code** | Historical; local tree *is* the APS-shaped source you can open |
| **This UE5 rebuild** | **Orthogonal further** | Offline Domain + classic login UX + freeroam; not a drop-in Steam protocol grid |

**Practical recommendation:** Treat **local ApbPrivateServer** as the on-disk APS/RaGEZONE-class lobby/world skeleton; use **ivan-draga/rAPB** as the best public modern-client research fork; use **this repo’s Domain + frontend** for RTW nostalgia play without depending on dead mega links. Full world+district grid remains deferred (see plan non-goals).

---

## 2011 RTW extract touchpoints (related)

Inventory/extract outputs: `D:\APBReloaded\Content\Extracted\2011_rtw\`

- Interface: `APBMenus_FrontEnd.upk`, `APBMenus_Art_GameFlowScenes.upk`, … (umodel TGAs under `UI/umodel/`)
- Movies: SplashScreen / IntroTitles / LoadingMovieV1 `.bik`
- Config: `APBCompat_APBLoginLevel.ini`, `APBUI.ini`, launcher/client configs
- Login theme: Wwise `APB_ThemePreMaster` (short_id `540780953`) — 2011 StreamedSFX LUT did not contain modern id; rebuild uses `Content/Audio/LoginTheme_APB_ThemePreMaster.wav` with provenance JSON

---

## Opcode crosswalk snippet (Lobby)

| Local enum | Value | Domain analogue |
|------------|-------|-----------------|
| ASK_LOGIN | 0x3E8 | `LoginService::Login` |
| ANS_LOGIN_SUCCESS / FAILED | 0x7D4 / 0x7D5 | login_ok / login_fail stages |
| ASK_WORLD_LIST / WORLD_LIST | 0x3EB / 0x7D8 | `WorldDirectory::ListOnline` |
| ASK_WORLD_ENTER | 0x3F0 | `EnterWorld` |
| ASK_CHARACTER_CREATE | 0x3EE | `CreateCharacter` |

World district (deferred full grid): `ASK_DISTRICT_RESERVE (0xBBB)` … `ANS_DISTRICT_ENTER (0xFAA)` ↔ Domain `ReserveDistrict` / `JoinDistrict`.
