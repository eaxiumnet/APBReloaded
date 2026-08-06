# APB Reloaded — UE 5.8 Recreation + Asset Studio

A from-scratch recreation of **APB: All Points Bulletin (Reloaded)** built on **Unreal
Engine 5.8**, with a web-based **asset studio** that browses, previews, and animates
every mesh extracted from the retail build.

```
APBReloaded/
|- Source/APBReloaded/
|  |- Domain/                 # engine-free authoritative gameplay/backend rules
|  `- Systems/                # UE bridge, replication, frontend, district, server, economy
|- Content/
|  |- Data/                   # apbdb/reference-derived JSON catalogs
|  |- Imported/               # UE assets tracked by the import ledger
|  `- Extracted/              # reference output (local only, never packaged)
|- Config/                    # maps, GameModes, networking, ports
|- tests/                     # standalone C++17 domain test suites
|- tools/content-studio/      # web asset viewer (React + FastAPI + three.js)
`- work/                      # plans, evidence, debug notes
```

## What works today

- **Content Studio** (`tools/content-studio`) — browse every extracted asset in the
  browser: weapons, vehicles (with family wheel assembly), characters, clothing,
  prop animations, textures, materials, districts, and audio.
  - three.js preview with auto-rotate, drag orbit/pan/zoom, animation playback
  - glTF export pipeline (PSK → skinned GLB) with animation rebase and
    frame-0-exact skinning
  - UV decal placement workflow for clothing, texture zoom/pan inspection
  - 20+ Playwright e2e tests, 190+ backend pytest suites
- **Frontend flow** — 2011-fidelity main menu: login, character create/select,
  district select, settings overlay, replay videos, credits, register.
- **Multiplayer foundation** — lobby, district tickets, replicated player state
  (faction, threat, currency), freeroam admission, chat relay.
- **Domain layer** — pure C++17 `apb::WorldService` with standalone MSVC test
  harness for characters, districts, economy, missions, and combat resolve.

## Try the asset viewer

```bash
# server (Python 3.11+, reads Content/Extracted)
cd tools/content-studio/server
python -m venv .venv && .venv/Scripts/pip install -r requirements.txt
.venv/Scripts/python -m uvicorn main:app --port 8777

# web (Node 18+)
cd ../web
npm install
npm run dev        # http://localhost:5173
```

The viewer requires a local `Content/Extracted` tree (produced by the extraction
tooling from a retail APB install — not distributed here).

### Tests

```bash
cd tools/content-studio/server && .venv/Scripts/python -m pytest -q
cd tools/content-studio/web   && npx playwright test
```

## Build the game (UE 5.8)

```powershell
# Domain tests (no engine needed)
powershell -File D:\APBReloaded\tests\build_and_run.ps1

# Editor + game targets
D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat APBReloadedEditor Win64 Development -Project=D:\APBReloaded\APBReloaded.uproject -WaitMutex
D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat APBReloaded Win64 Development -Project=D:\APBReloaded\APBReloaded.uproject -WaitMutex

# Play (windowed)
D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe D:\APBReloaded\APBReloaded.uproject -game -windowed -log
```

Launch helpers: `Launch_APB.bat`, `Create_Desktop_Shortcut.bat`, `HOW_TO_PLAY.md`.

## Contribution

- **Issues** — bug reports, asset gaps, and feature requests are welcome.
  Attach screenshots; for viewer bugs include the browser console output.
- **PRs** — keep one concern per PR. C++ lives in `Source/APBReloaded/Domain` and
  `Systems`; catalogs in `Content/Data` (apbdb-seeded, no guessed values); viewer
  logic in `tools/content-studio`.
- Run the relevant test suite before opening a PR (see above).

## Legal

This is a fan recreation for research and preservation. It contains no retail game
binaries and no extracted retail assets are distributed — `Content/Extracted` and the
reference installs are local-only. All trademarks belong to their respective owners.
