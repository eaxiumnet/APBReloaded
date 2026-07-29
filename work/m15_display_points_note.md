# M15 — DisplayPoint catalog (collectible / achievement / progression display entries)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **DisplayPoint** table — the progression / collectible entries surfaced in the UI
(e.g. the "Graffiti - Empire Slipway, South" spray-tag collectibles found around each district) — into a
Domain catalog. Each point carries a full Title, an abbreviated ShortTitle, a Description and an
ObtainedBy blurb explaining how the player earns it.

- Extractor: `tools/scripts/extract_display_points.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\DisplayPoint.INT` (UTF-16LE, mirror of cooked
    SDD table "DisplayPoint").
  - Four keys per id: `DisplayPoint_<id>_{Title|ShortTitle|Description|ObtainedBy}`.
  - Drops the `None` DNT placeholder + empty-title rows. `U+21B5 -> newline`; strips other C0 control.
- Data: `Content/Data/display_points.json` — **277 rows** (278 retail ids minus the `None` row).
- Header: `Source/APBReloaded/Domain/APBDisplayPoints.h` — `DisplayPointCatalog`
  (`Find/Title/ShortTitle/Description/ObtainedBy/ForDistrict/Count`, merge-by-id, order-sorted);
  `static District(id)` = first `_`-token ("Financial", "Waterfront", ...). Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token `display_points=`).
- Test: `TestDisplayPointsFromRetail` in `tests/run_domain_tests.cpp` (count==277, Title/ObtainedBy
  verbatim, DNT ShortTitle preserved, no literal `\u`, District + ForDistrict, order sort, missing-id
  safety, end-to-end via WorldService).

## RETAIL DATA QUIRK (preserved 1:1)
Many entries were never given an authored ShortTitle and carry the literal placeholder
`DNT - DO NOT TRANSLATE` in that field. This is the real retail string — kept **verbatim**, do NOT
"fix" it. The whole-row `None` id (a DNT placeholder row) IS dropped (278 -> 277).

## Verify constants
count=277, stray_printable_u=0, order 0..276 unique, id_uniq=277, empty title/shorttitle/desc/obtainedby=0,
has_None=0, has_DNT(title)=0.

## Notes for other agents
- Transient build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root
  (no `/Fo`), so two agents building concurrently can collide with `C1083 ... Permission denied` on a
  shared obj (e.g. `APBWorldService.obj`). It is NOT a code error — retry the build; it clears when the
  other build releases the lock. (A durable fix would add a per-run `/Fo` obj dir, out of scope here.)
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (8864 kv each — voice subtitles,
  largest), `PopupDialogs.INT` (167 single `PopupBody`), `HUDMarkerVisualText.INT` (112 x 6 fields with
  `<Color:...>` markup), `Tooltips.INT` (needs a different `Scene@Widget` key parser),
  `TaskOperationUIProfile.INT`. Dead-end: `InventoryItemPrices.INT` (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package ids
  from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
