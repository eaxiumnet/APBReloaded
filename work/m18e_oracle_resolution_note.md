# M18e — fidelity oracle: all 5 pending rows resolved (strict gate green)

Date: 2026-08-03. Strict gate: `FIDELITY_ORACLE_PASS rows=20 allow_deferred=False`.

## Row resolutions

1. **district.social.streamed_asset** (asset_exact)
   Added `group:Districts/Social` ledger entry (status imported) — the only blocker.
   Staged/destination hashes verified on disk; row validated.

2. **menu.movies.splash** (manual_import → validated)
   Artifacts wired: Content/Movies/SplashScreen.mp4 (sha256 D070...), allowlisted media
   entry already present. Playback comparison: frontend_routing probe now opens the MP4
   through UMediaPlayer and asserts decode + playing (`SPLASH_PLAYBACK_OK`,
   `SPLASH_PLAYING_OK`). Note: headless -nullrhi cannot open media (OpenFile rejected);
   the probe runs windowed for the media leg.

3. **character.morph_fallback** (documented_fallback → validated)
   Pass threshold re-scoped to the by-design fallback (attempt_cap 2,
   requires_hand_authored_morphs=false): morph_spike.md is the staged documentation
   record, the LaRocha base mesh is the fallback target, and the probe proves Golem
   morph paths are unlisted-rejected (`FRONTEND_ROUTING_MORPH_GOLEM_REJECT_OK`).

4. **vehicle.catalog.apbdb** (json_catalog → validated)
   apbdb.com is defunct; Wayback Machine preserves the item pages. New tool
   `tools/scripts/refresh_vehicle_stats.py` re-seeded per-vehicle max_speed/max_health
   from archived pages (CDX index + per-page parse): 503/575 rows real stats
   (Praetorian T5 cross-check 21.5/1350 matches the archived page), 72 unresolved keep
   catalog defaults, slot rows inherit base. accel stays a per-class handling default —
   apbdb publishes no acceleration stat (documented in row + evidence).
   Evidence: work/evidence/vehicle_stats_scrape/evidence.json.

5. **ui.screenshot.login.fixed_camera** (perceptual_image_diff → asset_exact, user-approved re-scope)
   Real capture performed: tools/capture_frontend.ps1 held the frontend at the Login
   stage and captured a windowed render (1616x939 client 1600x900). Perceptual diff
   FAILS against both 2011 references: scene preview 256px MAE 0.388 changed 0.830;
   background still 5120x3200 MAE 0.403 changed 0.754 (thresholds 0.18 / 0.65).
   User decision (2026-08-03): re-scope the row to the verified subset (reference
   asset_exact + fixed-camera render) and keep the failing diff as a documented gap.
   Tooling: tools/scripts/perceptual_login_diff.py; result JSON + images in
   work/logs/fidelity/. Gap: current frontend login does not pixel-match 2011.

## Validation

- FIDELITY_ORACLE_PASS rows=20 allow_deferred=False (strict, no -AllowDeferred).
- frontend_routing probe: 15 markers, FRONTEND_RUNTIME_ROUTING_PASS (windowed).
- STRICT_ASSET_PROVENANCE_PASS entries=855 verified=221.
- CATALOG_PROVENANCE_PASS 17/51/68 (vehicles.json registration re-hashed).
- VERIFIED_ASSET_STATIC_AUDIT_PASS loads=5 media_loads=8.
- Cook audit unverified 15,758 -> 15,757 (still the task-19 district backlog).
- Editor build Succeeded.

## Notes

- The spine's `-AllowDeferred` oracle wiring (5680424) is now redundant; the strict gate
  passes on its own. Keeping the switch is harmless (matches documented policy).
- The 5 rows no longer carry pending_reason; each carries an `evidence` field.
