# M15 — LoadingMovieTips catalog (loading-screen gameplay hints)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **LoadingMovieTips** table — the gameplay hints shown over the loading movie
while a district streams in ("In the Customization Editors, press 1 to select the Move tool.",
"Never give your account details or password to anyone else.", ...) — into a Domain catalog.

- Extractor: `tools/scripts/extract_loading_tips.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\LoadingMovieTips.INT` (UTF-16LE, mirror of
    cooked SDD table "LoadingMovieTips").
  - One key per id: `LoadingMovieTips_<id>_Message`.
  - Drops the `None` DNT placeholder + empty-message rows. `U+21B5 -> newline`; strips other C0 control.
- Data: `Content/Data/loading_tips.json` — **134 rows**.
- Header: `Source/APBReloaded/Domain/APBLoadingTips.h` — `LoadingTipCatalog`
  (`Find` / `Message` / `ForCategory` / `Count`, merge-by-id, order-sorted); `static Category(id)` = first
  `_`-token (GP 61 / SL 15 / OW 9 / UI 7 / ED / EG / ...). Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token `loading_tips=`).
- Test: `TestLoadingTipsFromRetail` in `tests/run_domain_tests.cpp` (count==134, Message verbatim,
  no literal `\u`, Category + ForCategory, order sort, missing-id safety, end-to-end via WorldService).

## Verify constants
count=134, stray_printable_u=0, order 0..133 unique, id_uniq=134, empty_message=0, has_None=0.
Category spread: GP 61, SL 15, OW 9, Epidemic 8, LS 8, UI 7, ED 4, EG 4, ...

## Notes for other agents
- Remaining unported high-value INT tables (129 surveyed): `Subtitles_MASC/FEM.int` (8864 kv each —
  voice subtitles, largest tables), `DisplayPoint.INT`, `TaskOperationUIProfile.INT`,
  `HUDMarkerVisualText.INT`, `Tooltips.INT`, `PopupDialogs.INT`. Dead-end: `InventoryItemPrices.INT`
  (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package ids
  from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
