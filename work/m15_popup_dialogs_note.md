# M15 — PopupDialogs catalog (in-game advisory / help popups)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **PopupDialogs** table — the advisory / help popups shown to the player during play
(ammo-low advice, arrest rules, vehicle controls, group/mission notices, ...) — into a Domain catalog.

- Extractor: `tools/scripts/extract_popup_dialogs.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\PopupDialogs.INT` (UTF-16LE, mirror of cooked
    SDD table "PopupDialogs").
  - One key per id: `PopupDialogs_<id>_PopupBody`.
  - Drops the `None` DNT placeholder + empty-body rows. `U+21B5 -> newline`; strips other C0 control.
- Data: `Content/Data/popup_dialogs.json` — **165 rows** (167 retail ids minus `None` + one empty body).
- Header: `Source/APBReloaded/Domain/APBPopupDialogs.h` — `PopupDialogCatalog`
  (`Find/Body/ForCategory/Count`, merge-by-id, order-sorted); `static Category(id)` = first `_`-token
  (AD 70 / TD 29 / SD 29 / RO 14 / HUDO 13 / GUI 10 / ...). Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token `popup_dialogs=`).
- Test: `TestPopupDialogsFromRetail` in `tests/run_domain_tests.cpp` (count==165, Body verbatim, no
  literal `\u`, Category + ForCategory, order sort, missing-id safety, end-to-end via WorldService).

## Verify constants
count=165, stray_printable_u=0, order 0..164 unique, id_uniq=165, empty_body=0, has_None=0.
22 bodies embed `<Key:...>` UI markup tokens (resolved to the player's bound key at runtime) — kept VERBATIM.
Category spread: AD 70, TD 29, SD 29, RO 14, HUDO 13, GUI 10, ...

## Notes for other agents
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  two agents building concurrently can collide with transient `C1083 ... Permission denied` on a shared
  obj. It is NOT a code error — retry the build.
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (8864 kv each — voice subtitles,
  largest), `HUDMarkerVisualText.INT` (112 x 6 fields with `<Color:...>` markup), `Tooltips.INT` (needs a
  different `Scene@Widget` key parser), `TaskOperationUIProfile.INT`. Dead-end: `InventoryItemPrices.INT`
  (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package ids
  from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
