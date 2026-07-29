# APBReloaded — Technical Architecture

v1 · 2026-07-19 · Companion to `work/_active.md` (the active plan). AGENTS.md remains the
single source of truth for project rules; this document is the system design those rules
execute against.

---

## 1. Design principles

1. **Domain is pure C++** — no UE headers, stdlib only, compiled standalone by
   `tests\build_and_run.ps1`. All game rules (login, economy, threat, missions, auction,
   clans, matchmaking) live here and are unit-testable without the engine.
2. **Server-authoritative** — clients never mutate Domain state directly. UE glue mutates
   only where `CanMutateDomain()` is true (standalone / listen / dedicated server).
3. **Replication is the only network path for gameplay** — replicated state on
   `AAPBPlayerState`, RPCs with `WithValidation`, GameMode-owned spawns. No polling.
4. **Port behavior, not binaries** — source installs are reference/data. Data comes from
   plain-readable sources first (INT tables, INI, apbdb.com), cooked packages second
   (umodel/WwiseExtract/ffmpeg).
5. **One concern per commit** — enforced now that the repo is git-initialized (M0).

## 2. Code module map

One runtime module `APBReloaded` (split deferred to M17, see §10). Strict subfolder
namespacing gives the modularity without destabilizing the working build.

| Folder | Responsibility | Key classes (existing → new) | Hard rules |
|---|---|---|---|
| `Source\APBReloaded\Domain\` | All game rules, engine-free | WorldService, Catalog, Threat, Mission, Combat, Inventory, Armas, Auction, Customization, Social, ModelRegistry, DistrictPlacement → **+ Persistence, ChatService, MatchmakingService, ClanService, GroupService, EconomyService, VehicleService** | No UE includes; JSON via existing hand-rolled parser; every service unit-tested in `tests\` |
| `Source\APBReloaded\Systems\Frontend\` | Boot/login/char-select/char-create/district-select UI | APBFrontendGameMode/PlayerController/HUD, APBFrontendWidget, APBFrontendLayoutMath, APBDebugMenuWidget, APBCharacterCreatePreviewActor | UMG in C++; layout math stays pure + tested |
| `Source\APBReloaded\Systems\District\` | Freeroam gameplay | APBDistrictGameMode, APBFreeroamGameMode/Character/HUD, APBDriveableVehicle, APBBotNPC, APBInteractable, APBDistrictStreamer, APBDistrictPlacementLoader | Map-prefix GameMode routing stays in `DefaultEngine.ini` |
| `Source\APBReloaded\Systems\Server\` (new) | Dedicated-server roles + control channel | APBServerControl (TCP/JSON), APBWorldServerBootstrap, APBDistrictServerBootstrap | Only code that may open sockets outside UE netdriver |
| `Source\APBReloaded\Systems\Economy\` (new) | Auction/mail/shop UMG | APBAuctionWidget, APBMailWidget | Widgets call GameInstanceSubsystem; never Domain directly from Blueprints |
| Root bridge | UE entry + replicated state | APBGameInstanceSubsystem (owns `apb::WorldService`), AAPBPlayerState | Replicated fields only on PlayerState; snapshot push pattern preserved |

Blueprint policy: C++ for logic; Blueprints only for cosmetic wiring (animations, cosmetic
events). No game rules in Blueprints.

## 3. Runtime topology (dedicated deployment)

```
┌────────────┐   UE netdriver (game port 17778)   ┌─────────────────────────────┐
│  Client A  ├────────────────────────────────────►│  WORLD SERVER (1 process)   │
│  Client B  ├────────────────────────────────────►│  APBReloadedServer          │
└────────────┘                                    │  -WorldServer -Port=17778   │
       │ ClientTravel(IP:port, ticket)            │  Hosts: login/auth, world   │
       ▼                                          │  directory, character DB,   │
┌─────────────────────────┐   TCP/JSON control    │  mail, auction, chat relay, │
│ DISTRICT SERVER(S)      │◄──────────────────────┤  matchmaking coordinator    │
│ -District=Waterfront    │   (port 17800)        └─────────────────────────────┘
│ -Port=17821             │                              owns Domain DB (JSON)
│ -WorldAddr=127.0.0.1    │
│ Hosts freeroam map,     │
│ gameplay replication    │
└─────────────────────────┘
```

- **One binary, role by CLI** (D6). `-WorldServer` hosts a headless lobby map; district
  role hosts `Lvl_APB_<District>_Freeroam`. Standalone/listen keeps the current in-process
  WorldService path — same Domain code, no fork.
- **Control channel** (TCP, JSON-lines, world listens on 17800): `district.register`,
  `district.heartbeat`, `district.directory` (world → clients via lobby), `char.handoff`,
  `ticket.validate`, `chat.relay`, `mail.notify`, `auction.sync`.
- **Login flow**: client → world (UE connect on 17778) → login RPC → WorldService
  `LoginAccount` (D5 store) → char select → district select requests reservation →
  world returns `{ip, port, one-time ticket}` → `ClientTravel`.
- **District join flow**: district receives PostLogin → `ticket.validate` to world →
  world pushes `char.handoff` (appearance/inventory/cash snapshot) → district applies via
  `SyncPlayerStateFromDomain`; on exit, district posts final snapshot back.
- **Auction flow**: kiosk RPC → district server → `auction.sync` → world Domain (owner of
  listings, fees, expiry) → persist → replicate result back. Read snapshots cached on
  district for browse latency.
- **Chat flow**: in-district chat = UE RPC on district; cross-district/whisper =
  `chat.relay` through world. ChatService in Domain enforces rate limits/blocks.
- **Anti-tamper note**: district never trusts client-supplied character data — only world
  `char.handoff` over the control channel.

### Relay and port contract

`Source\APBReloaded\Systems\APBPorts.h` is the allocation authority; `[APBServer]` in
`Config\DefaultGame.ini` mirrors it, and launch scripts parse the header rather than carrying
their own port defaults. `DistrictPort(numeric_id)` returns `17810 + numeric_id` for positive
stable catalog IDs and `0` for an invalid ID.

| Role | Port | Resolution |
|---|---:|---|
| World game / NetDriver (UDP) | 17778 | `apb::ports::World` |
| World-district relay (TCP) | 17800 | `apb::ports::Relay` |
| District game base (reserved) | 17810 | `apb::ports::DistrictBase` |

| District | numeric_id | Port |
|---|---:|---:|
| Financial | 1 | 17811 |
| FinancialChaos | 2 | 17812 |
| PGAsylum | 4 | 17814 |
| PGBeacon | 5 | 17815 |
| PGCrate | 6 | 17816 |
| Social | 9 | 17819 |
| Waterfront | 11 | 17821 |
| FinancialRiot | 12 | 17822 |

The relay is a versioned JSON-lines protocol: every newline-terminated frame requires
`version`, `request_id`, `sent_ms`, and `auth`. Frames are capped at 64 KiB; the request
timestamp window is 2 s; the receive queue is bounded at 256; reconnect backoff is
exponential from 250 ms to 5000 ms; a district is evicted after two missed 5 s heartbeats.

## 4. Data & persistence schemas (D5: JSON docs, `Saved\DomainDB\`)

`accounts.json`
```json
{ "accounts": [ { "id": "acc_...", "name": "...", "pass": "plaintext-offline-private",
  "banned": false, "created_utc": "...", "last_login_utc": "..." } ] }
```
`characters\<account>_<slot>.json`
```json
{ "name": "...", "faction": "Enforcer|Criminal", "appearance_blob": "...",
  "wardrobe": { "Top": "item_id", "...": "..." }, "inventory": [ { "item_id": "...",
  "count": 1 } ], "cash": 10000, "g1c": 500, "threat_points": 0, "roles": {},
  "clan_id": null, "tutorial_done": false }
```
`mail.json` — `{ "messages": [ { "id", "to_char", "from", "subject", "body",
"attachments": [ { "item_id", "count", "cash" } ], "read", "expires_utc" } ] }`
`auction.json` — `{ "listings": [ { "id", "seller_char", "item_id", "count",
"start_price", "buyout_price", "current_bid": { "bidder_char", "amount" }, "fee_paid",
"created_utc", "expires_utc", "state": "active|sold|expired|cancelled" } ] }`
`clans.json` — `{ "clans": [ { "id", "name", "tag", "leader_char", "ranks": {},
"members": [ { "char", "rank", "joined_utc" } ] } ] }`
`social.json` — friends/blocks per character.
`districts_live.json` — district registry (id, map, ip, port, population, state) written
by world heartbeats.
`tools\import_ledger.json` — per-asset import tracking:
`{ "asset_key": "upk_path#object", "source": "retail|2011", "dest": "/Game/Imported/...",
"status": "extracted|imported|bound|manual", "updated": "..." }` → reported to
`work\IMPORT_STATUS.md` by `tools\scripts\` (what's imported vs. what needs manual recreation).

Passwords are plaintext only because this is a private offline port; hash + salt lands with
the M16 hardening pass. Documented, not hidden.

## 5. Content folder & naming conventions

```
Content\
├── Maps\                Lvl_APB_Frontend, Lvl_APB_<District>_Freeroam (existing convention)
├── Imported\
│   ├── Districts\<District>\{Meshes,Materials,Textures}\   (per-block subfolders)
│   ├── Characters\{Contacts\<Name>, Wardrobe\<Category>\}
│   ├── Vehicles\<VehicleName>\    Weapons\<WeaponName>\
│   └── UI\{Menu2011, Retail}\
├── Audio\{UI, Music, Ambience}\      Movies\                Data\ (JSON catalogs)
└── Extracted\            reference trees + docs — never packaged, git-ignored if large
```
Naming: `<Source>_<Category>_<Name>[_LODn]`, e.g. `R_Wardrobe_Top_JacketLeather`,
`W_District_Financial_Block09_Wall_A`. `R_`=retail, `W_`=RTW 2011 prefix only when the
same asset exists in both and provenance matters.

## 6. Extraction pipeline (per asset class)

| Class | Source | Tool (all in `tools\`) | Intermediate | UE import |
|---|---|---|---|---|
| Meshes/textures | `.upk` (ver 547 & 564) | `UEViewer\umodel` | `.psk/.fbx`, `.png/.tga` | Editor Python scripted import |
| District levels | `.APB` streaming blocks | `scripts\export_apb_level_parallel.py`, `extract_streamed_actors.py`, `merge_district_blocks.py` | placement JSON + mesh set | `APBDistrictPlacementLoader` manifests |
| UI art | `APBMenus_*.upk` | `scripts\export_2011_ui*.py`, `extract_2011_login_assets.py` | png + layout notes | UMG textures |
| Audio | `.bnk/.pck` (Wwise) | `WwiseExtract\` | `.wem→.wav` | SoundWave/SoundCue |
| Video | `.bik` | ffmpeg/RAD | `.mp4` | MediaPlayer/Electra (plugins already on) |
| Strings/data | `.INT/.INI`, apbdb.com | `apbdb\sync_apbdb.py`, new INT parsers | `Content\Data\*.json` | Catalog loaders (existing) |
| Animations | `Anim\*.upk` | umodel → `.psa` | retarget to UE5 skeleton | M13/M17 scope |
| Relay control | `APBRelayProtocol.{h,cpp}` | versioned JSON-lines | 64 KiB frames, 2 s window, 256 queue | `Systems\Server\` TCP transport |

Every batch run updates `tools\import_ledger.json`; the bind report
(`build_placement_bind_report.py`) is the gap oracle: any manifest entry not resolving to a
real `.uasset` (never `/Engine/BasicShapes/Cube`) is the remaining-work list.

## 7. UE 5.8 system choices

| Feature | UE system |
|---|---|
| Menus / HUD | UMG in C++ (existing pattern; no CommonUI migration this phase) |
| Menu video | MediaPlayer + Electra (enabled); WmfMedia fallback |
| Char-create preview | SceneCapture2D studio (existing `APBCharacterCreatePreviewActor`) |
| Clothing color | Dynamic Material Instances driven by `palettes.json` |
| Gameplay replication | PlayerState/GameMode/validated RPCs (existing pattern) |
| World↔district control | `FSocket`/TCP JSON-lines v1 in `Systems\Server\`; `version`, `request_id`, `sent_ms`, and `auth` are mandatory |
| NPC bots | StateTree (plugin already enabled) |
| Streaming | Existing manifest chunk streamer; World Partition evaluation deferred to M17 |
| Persistence | JSON docs now (D5); SQLite/Postgres adapter path in §10 |
| UI SFX | SoundCue from WwiseExtract WAVs |

## 8. Replication & RPC rules

- Replicated on `AAPBPlayerState`: faction, threat, cash, G1C, mission state, inventory
  count, district session (existing) → add clan tag, group id.
- Mutations: server-only RPCs `WithValidation`; Domain mutation gate `CanMutateDomain()`.
- Economy/threat/inventory NEVER client-authoritative; kiosk/chat widgets send intent RPCs.
- Net tick rates: keep `NetServerMaxTickRate=30`, `MaxClientRate=100000` (existing config).

## 9. Anti-cheat considerations (design posture, not a product)

Server-authoritative Domain (§1.2), validated RPCs, world-issued join tickets (§3),
server-side combat resolution (`ResolveShot` runs server-side), no client-trusted economy.
M16 adds: speed/teleport heuristics on district server, stat-anomaly logging, password
hashing. Kernel-level AC and retail's GFAC/EAC/BattlEye are out of scope for a private port.

## 10. Deferred migration paths

- **Storage**: repository interfaces (M2) allow swapping JSON → SQLite/Postgres without
  touching services.
- **Module split**: `APBFrontend` / `APBDistrict` / `APBServer` modules at M17 once the
  roadmap is stable.
- **World Partition**: evaluate against the manifest streamer after both districts ship.
- **Module-level protocol compat** (real retail clients): permanently out of scope.
