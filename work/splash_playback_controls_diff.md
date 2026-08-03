# Splash Playback: Windowed vs Headless Control Diff

**Date:** 2026-08-03
**Purpose:** Durable side-by-side evidence that the headless splash skip
(`SPLASH_PLAYBACK_UNAVAILABLE`) and the real decode path
(`SPLASH_PLAYBACK_OK`) are both exercised and both pass routing.

## Runs

| Control | Log | RHI | Result |
|---|---|---|---|
| Windowed | `work/evidence/splash_playback_windowed_v2.log` | real (no `-nullrhi`) | exit 0 |
| Headless | `work/evidence/splash_playback_headless.log` | `-nullrhi` (QA gate flags) | exit 0 |

Both launched `-APBProbe=frontend_routing -APBStrictAssetAllowlist` against
`Lvl_APB_Frontend` with `-unattended`.

## Marker Split (13 core routing markers identical in both)

| Marker | Windowed | Headless |
|---|---|---|
| TEXTURE_ALLOWED / TEXTURE_LOAD | OK | OK |
| SFX_LOAD | OK | OK |
| MEDIA_ALLOW / MEDIA_REJECT | OK | OK |
| WRONG_SOURCE_REJECT | OK | OK |
| MORPH_GOLEM_REJECT | OK | OK |
| CHAR_MESH_ALLOWED / CHAR_MESH_LOAD | OK | OK |
| CHAR_MATERIAL_ALLOWED / CHAR_MATERIAL_LOAD | OK | OK |
| CHAR_WRONG_SOURCE_REJECT / CHAR_UNLISTED_REJECT | OK | OK |
| FRONTEND_RUNTIME_ROUTING | **PASS** | **PASS** |

## Splash divergence (the documented split)

| Marker | Windowed | Headless |
|---|---|---|
| SPLASH_OPEN | REQUESTED | REJECTED |
| SPLASH_PLAYBACK | **OK** | **UNAVAILABLE reason=headless_session** |
| SPLASH_PLAYING | OK | — |

## Why

`FApp::CanEverRender()` (UE 5.8 `App.h:400`) is false only under `-nullrhi` /
commandlet / dedicated server. The splash block in
`APBSessionProbeSubsystem.cpp` checks it before judging decode:

- Headless (`-nullrhi -unattended`): `SPLASH_PLAYBACK_UNAVAILABLE
  reason=headless_session`, routing still passes on the 13 core markers.
- Windowed (real RHI): real media open+decode → `SPLASH_PLAYBACK_OK` +
  `SPLASH_PLAYING_OK`.

The QA gate (`run_m3r_asset_qa.ps1`) runs the frontend_routing leg headless
and asserts `FRONTEND_RUNTIME_ROUTING_PASS`; the windowed probe is the
windowed-side control for the splash comparison (oracle row
`menu.movies.splash`).
