# M15 — Contacts catalog (authoritative retail name + untruncated bio)

**Status:** COMPLETE. Build green (FAILS=0 across all 17 domain-test binaries; compile_error_hits=0).

## What
Ported the retail **Contacts** table — the mission-giver NPCs that drive all of APB
progression (Double-B, Veronika Lee, Britney Bloodrose, Grissom, ...) — into a new,
merge-safe Domain catalog carrying each contact's display **name** + full lore **bio**.

- Extractor: `tools/scripts/extract_contacts_catalog.ps1`
  - Source: `...\APB Reloaded\APBGame\Localization\INT\Contacts.INT` (UTF-16LE, mirror of
    cooked SDD table "Contacts").
  - Two keys per id: `Contacts_<id>_Title` (name) + `Contacts_<id>_Description` (bio).
  - Drops the `None` DO-NOT-TRANSLATE placeholder; keeps only rows that render a name.
  - `U+21B5 (↵) -> newline`; strips other C0 control; prose kept verbatim.
- Data: `Content/Data/contacts_catalog.json` — **97 rows** (of 98 retail ids; `None` dropped).
- Header: `Source/APBReloaded/Domain/APBContactsCatalog.h` — `ContactCatalog`
  (`Find` / `Title` / `Description` / `HasDescription` / `ForDistrict` / `Count`,
  static `District(id)` = first `_`-token: `Financial` / `Waterfront` / `Social`). Header-only.
- Wired into `APBWorldService` (`.h` include+member, `.cpp` load + INIT log token
  `contacts_catalog=`).
- Test: `TestContactsCatalogFromRetail` in `tests/run_domain_tests.cpp` (count==97,
  `Title("Financial_C1")=="Double-B"`, HasDescription, no literal `\u`, `None` dropped,
  District/ForDistrict, missing-id safety, end-to-end via WorldService).

## Why a NEW catalog (not overwriting contacts_lore.json)
`Content/Data/contacts_lore.json` is **apbdb-scraped** and its bios are **truncated (~500
chars, cut mid-sentence)**; it is consumed by `ProgressionCatalog::LoadContactsFromFile`.
`Contacts.INT` is the **authoritative, untruncated** retail source — bios run to several
thousand chars (Double-B 2621, Britney Bloodrose 2927, longest Grissom 7307). Keeping a
separate `contacts_catalog.json` / `APBContactsCatalog.h` is a genuine 1:1 fidelity
improvement AND merge-safe: it does not touch `contacts_lore.json` or the ProgressionCatalog
that other agents depend on. The two coexist.

## Verify constants
count=97, stray_printable_u=0, order 0..96 unique, id_uniq=97, empty_title=0,
empty_desc=6 (kept — those contacts render a name but carry no bio), has_None=0.

## Notes for other agents
- This completes the retail INT `_Description`/name catalog family that is high gameplay
  centrality. Remaining unported high-value INT tables surveyed (129 total): `Subtitles_MASC/FEM.int`
  (8864 kv each — voice subtitles), `DisplayPoint.INT`, `Tutorials.INT` (Title/SubTitle/Body,
  HTML `<br>` markup), `LoadingMovieTips.INT` (134 × Message), `TaskOperationUIProfile.INT`,
  `HUDMarkerVisualText.INT`, `Tooltips.INT`, `PopupDialogs.INT`. Dead-end: `InventoryItemPrices.INT`
  (5495 rows, all empty).
- Bigger follow-up still open: reward PAYLOAD increment (attach `contents` lists to reward/package
  ids from cooked SDD/apbdb — not present in any INT).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) ->
  4 sequential WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
