# M15 — HUDMarkerVisualText catalog (role-dependent HUD marker labels)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **HUDMarkerVisualText** table — the text the HUD paints on world markers (mission
objectives, spawn points, items). Each marker carries up to six role-dependent label variants and the HUD
picks one based on the local player's relationship to the marker and the mission phase.

- Extractor: `tools/scripts/extract_hud_marker_text.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\HUDMarkerVisualText.INT` (UTF-16LE, mirror of cooked
    SDD table "HUDMarkerVisualText").
  - Six keys per id: `_{OwnerAttack|OwnerDefend|OppositionAttack|OppositionDefend|Neutral|Misc}`.
  - Drops `None` + rows where all six fields are empty. `U+21B5 -> newline`; strips other C0 control.
- Data: `Content/Data/hud_marker_text.json` — **112 rows**.
- Header: `Source/APBReloaded/Domain/APBHUDMarkerText.h` — `HUDMarkerTextCatalog`
  (`Find/Label(id, MarkerRole)/Count`, merge-by-id, order-sorted) + `enum class MarkerRole`;
  `static Family(id)` = first `_`-token. Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token `hud_marker_text=`).
- Test: `TestHUDMarkerTextFromRetail` in `tests/run_domain_tests.cpp` (count==112, role labels verbatim,
  `<Color:...>` markup preserved, no literal `\u`, Family, order sort, missing-id safety, end-to-end).

## Verify constants
count=112, stray_printable_u=0, order 0..111 unique, id_uniq=112, has_None=0, all_six_empty=0.
Values keep `<Color:R=g G=g B=g>` markup VERBATIM; U+21B5 in-string breaks normalised to '\n'.

## Notes for other agents
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  concurrent agent builds can collide with transient `C1083 ... Permission denied`. Not a code error — retry.
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (8864 kv each — voice subtitles,
  largest), `Tooltips.INT` (needs a different `Scene@Widget` key parser — NOT the `<Table>_<id>_<Suffix>`
  schema), `TaskOperationUIProfile.INT`. Dead-end: `InventoryItemPrices.INT` (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package ids
  from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
