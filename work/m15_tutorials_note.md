# M15 — Tutorials catalog (in-game City Guide onboarding text)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **Tutorials** table — the in-game "City Guide" / tutorial book onboarding
help system ("Welcome to San Paro", "Basic", "Movement & Actions", ...) — into a Domain catalog
carrying each topic's Title + SubTitle + HTML Body.

- Extractor: `tools/scripts/extract_tutorials.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\Tutorials.INT` (UTF-16LE, mirror of cooked
    SDD table "Tutorials").
  - Three keys per id: `Tutorials_<id>_Title` + `_SubTitle` + `_Body`.
  - `U+21B5 (↵) -> newline`; strips other C0 control; prose + HTML markup kept verbatim.
- Data: `Content/Data/tutorials.json` — **120 rows** (`None` DNT placeholder dropped; none present).
- Header: `Source/APBReloaded/Domain/APBTutorials.h` — `TutorialCatalog`
  (`Find` / `Title` / `SubTitle` / `Body` / `HasBody` / `Count`, merge-by-id, order-sorted). Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token `tutorials=`).
- Test: `TestTutorialsFromRetail` in `tests/run_domain_tests.cpp` (count==120, Title/SubTitle verbatim,
  literal `&` proves `\u` restore, HTML `<br>` kept in body, no literal `\u`, order sort,
  missing-id safety, end-to-end via WorldService).

## HTML Body note
The Body carries the game's own lightweight HTML markup (`<br>`, `<b>`, `<img>`, ...). It is kept
**VERBATIM** (not stripped) so the UE5 tutorial-book UI can render it exactly like retail — a 1:1
fidelity requirement. 108 of 120 bodies contain markup; the ConvertTo-Json `\u`-restore leaves
`< > &` literal.

## Verify constants
count=120, stray_printable_u=0, order 0..119 unique, id_uniq=120, empty_title=0, empty_subtitle=0,
empty_body=0, body_has_html=108, has_None=0.

## Notes for other agents
- Remaining unported high-value INT tables (129 surveyed): `Subtitles_MASC/FEM.int` (8864 kv each —
  voice subtitles), `DisplayPoint.INT`, `LoadingMovieTips.INT` (134 × Message — loading-screen tips),
  `TaskOperationUIProfile.INT`, `HUDMarkerVisualText.INT`, `Tooltips.INT`, `PopupDialogs.INT`.
  Dead-end: `InventoryItemPrices.INT` (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package ids
  from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
