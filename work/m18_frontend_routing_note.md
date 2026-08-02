# M18 — Task 18: Frontend routing through the verified asset registry

Date: 2026-08-02

## Result

`FRONTEND_RUNTIME_ROUTING_PASS` (probe) and `M3R_ASSET_QA_PASS` (orchestrator).
Strict gate: `verified=197`. Allowlist schema v2: 161 entries + 36 media entries.

## What changed

- `APBVerifiedAssetRegistry`: media_entries parse (schema v2), `IsMediaAllowed`
  (file-path keyed; object-path aliases tolerated), `IsAllowedWithSourceBuild` +
  `ExpectedSourceBuild` on all typed loads, media file resolution against
  `FPaths::ProjectDir()` (not ProjectContentDir — avoids double `Content/Content`).
- `APBFrontendWidget`: `VerifyMediaFile` gate on startup movie, stage-bg video,
  replay open, and theme WAV loop. `LoadMenu2011Tex`/`LoadUiSounds` request
  source `2011`; `APBCharacterCreatePreviewActor` requests `retail`.
- `APBSessionProbeSubsystem`: `frontend_routing` probe mode (texture ALLOW_OK,
  SFX LOAD_OK, media ALLOW_OK, wrong-source REJECT_OK, unlisted media REJECT_OK).
- Tools: static audit media-token scan, generator media_entries emission,
  cook audit leaf-stripped matching, QA orchestrator frontend step.
- `tools/scripts/record_frontend_menu_import.py` (+ unit tests): builds D17
  evidence for 118 menu textures, 12 UI sounds, and 36 media rows from the
  on-disk 2011 upk -> PNG/WAV -> uasset chain.

## Evidence

- `work/evidence/frontend_menu2011_batch/textures_exact.json`
- `work/evidence/frontend_menu2011_batch/sounds_exact.json`
- `work/evidence/frontend_menu2011_batch/media_exact.json`
- Probe logs (gitignored): `.omo/evidence/task18_probe_final/frontend_routing.log`
- Orchestrator: `.omo/evidence/m3r_asset_qa/m3r_asset_qa_*.json` (gitignored)

## Bugs found and fixed

1. Media manifest parse: duplicate object_path stems (mp4 + webm variants) are
   legal; only file-path keys must be unique. Registry tolerates object-key
   collisions in the media map.
2. Recorder re-run wiped promotion: `merge_rows` now preserves
   `status/verified_at/verified_by` when replacing verified rows (unit-tested).
3. Cook audit unverified: 15,764 -> 15,759 (5 frontend refs closed). Remaining
   unverified = district (15,445) / vehicles (201) / characters (102) + catalog
   tails — task 19 territory.

## Remaining

Task 19: district/character/vehicle routing through the registry.
