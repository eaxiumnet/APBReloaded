# Track B — Content Studio: Slice 1 (VIEWER) complete

Date: 2026-07-22 · Agent: Cline (Sonnet) · Plan: `work/trackB_content_studio.md` §5 Slice 1

## What this is

The only unmet Track B Slice 1 gate — the §6 **Playwright QA** — implemented and proven
green. Picked up a partially-built scaffold (server + web from prior sessions) and added
the end-to-end harness without touching `Source/` or `Content/` (the Qoder M11–M16 domain
edits were live in `Source/` at the same time — Track B stayed isolated under
`tools/content-studio/`).

## Gate evidence (all exit 0, run live ~02:47 2026-07-22)

| §6 gate | Command | Result |
|---|---|---|
| Converter unit tests | `server/.venv python -m pytest -q` | `87 passed in 6.86s` |
| Frontend build (TS strict) | `web npm run build` | `✓ built in 4.80s` (no TS errors) |
| Playwright S1 happy | `web npm run e2e` | `1 passed (5.9s)` |

Playwright proves the full pipeline the plan gates on:
- backend (FastAPI) auto-started → served the 90-weapon catalog + converted
  `Weapon_Armas_Magnum` `.psk` → GLB on demand (`/api/mesh.glb`);
- frontend (Vite + React + three.js) auto-started → loaded the GLB into an orbit camera;
- scene-graph assert: `.stage[data-status="ready"][data-triangles>0]` (triangles counted
  from the three.js Mesh geometry, so a real mesh with real geometry rendered);
- browser console + pageerror collected → `[]` (clean);
- screenshot artifact captured: `web/test-results/slice1-viewer.png` (22,362 bytes).

Target mesh: `Weapon_Armas_Magnum` ("ACT 44") — the same mesh `server/tests/test_gltf.py`
proves parses + converts to a well-formed GLB, so the E2E runs the *networked* full stack
on a known-good mesh (deterministic; falls back to catalog[0] if ever renamed).

## Files touched (all under `tools/content-studio/`)

New:
- `web/playwright.config.ts` — chromium project; `webServer[]` auto-starts backend (venv
  uvicorn :8777) + frontend (vite :5173), both `reuseExistingServer:true`.
- `web/e2e/viewer.spec.ts` — the S1 happy spec (catalog → select Magnum → ready + tris>0
  + console clean → screenshot).
- `web/e2e/README.md` — run/setup instructions.

Edited:
- `web/package.json` — added `@playwright/test@1.61.1` (devDep) + `e2e`/`e2e:install` scripts.
- `.gitignore` — added `web/test-results/`, `web/playwright-report/`, `web/blob-report/`,
  `web/.playwright/`.

## Reproduce

```pwsh
cd D:\APBReloaded\tools\content-studio\web
npm install            # one-time: installs @playwright/test
npm run e2e:install    # one-time: downloads chromium (~150 MB)
npm run e2e            # auto-starts both servers + runs the suite
```

Or launch the interactive dev session (no tests): `tools/content-studio/run.ps1`.

## Notes for other agents

- **No commit made** — per Track B plan §7 risk ("No commits without user OK") and the
  shared-worktree constraint. All files are written to disk; a human should review + commit.
- Browser binaries are machine-local (`%USERPROFILE%\AppData\Local\ms-playwright`),
  gitignored — `npm run e2e:install` reproduces them on a fresh clone.
- The `npm run build` stderr is only a chunk-size *warning* (three.js bundle >500 kB),
  NOT an error — exit code is 0. A future Slice can code-split three.js if desired.
- Slice 1 is the harness for Slice 2 (ColMask/symbol editor). Keep the converter + viewer
  green as the regression net when adding the skin compositor.
