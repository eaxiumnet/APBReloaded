# APB Content Studio — E2E (Playwright)

Track B Slice 1 QA gate. Drives the real stack end-to-end: the FastAPI backend
converts an extracted ActorX `.psk` to GLB, the React + three.js viewer renders
it, and Playwright asserts the mesh is on-screen with a clean console.

## One-time setup

```pwsh
cd D:\APBReloaded\tools\content-studio\web
npm install            # adds @playwright/test (pinned in package.json)
npm run e2e:install    # downloads the chromium binary (~150 MB, once)
```

## Run

```pwsh
npm run e2e
```

Playwright auto-starts both servers (see `../playwright.config.ts`):

- **backend** — `server/.venv/Scripts/python.exe -m uvicorn main:app --port 8777`
- **frontend** — `npm run dev` (Vite on :5173, proxies `/api` -> :8777)

…reusing them if `run.ps1` (or another agent) already has them up, and tearing
down only the servers it started itself.

## What the suite proves (Slice 1 "S1 happy")

| Gate | How it's asserted |
|---|---|
| backend converts `.psk` -> GLB | the viewer's GLB request returns 200 and three.js loads it |
| screenshot shows the mesh | `test-results/slice1-viewer.png` artifact |
| browser console clean | `page.on('console'/'pageerror')` collected errors == `[]` |
| scene-graph: mesh present, >0 triangles | `.stage[data-status="ready"][data-triangles>0]` |

Target mesh: `Weapon_Armas_Magnum` ("ACT 44") — the same mesh the pytest suite
(`server/tests/test_gltf.py`) proves parses + converts to a well-formed GLB, so
this spec runs the *networked* full stack on a known-good mesh.

## Artifacts (gitignored)

- `test-results/` — screenshots + failure traces/videos
- `playwright-report/` — HTML report (`npx playwright show-report`)
