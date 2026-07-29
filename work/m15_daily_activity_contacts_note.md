# m15 — Daily-activity contact catalog (DailyActivityContacts.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0; real_compiler_errors=0)

## What landed
Extracted the retail APB **daily-activity contact catalog** — the text for "daily activities", the small
"do X today" objectives a player picks up from a contact each day (e.g. "Blow up 3 enemy vehicles") — into
the Domain layer. This is the **first variant-bearing catalog**: many activities ship several randomised
flavour VARIANTS of their text (the game rotates them so the same objective reads differently day to day /
faction to faction — variant 1 often reads from the Criminal angle, variant 2 from the Enforcer angle).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_daily_activity_contacts.ps1` |
| Data | `Content/Data/daily_activity_contacts.json` (133 rows / 85 activities) |
| Domain catalog | `Source/APBReloaded/Domain/APBDailyActivityContacts.h` (header-only `DailyActivityContactCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`daily_activity_contacts` member, load, `daily_activity_contacts=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestDailyActivityContactsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\DailyActivityContacts.INT` (UTF-16LE) — mirror of the cooked
  SDD table `DailyActivityContacts`. Single `[DailyActivityContacts]` section, 399 kv lines.
- Key grammar: `DailyActivityContacts_<id>_<Field>[_<n>]` where Field is `Title` / `HUDDescription` /
  `LongDescription`. The unnumbered key is **variant 1**; `_2`/`_3`/`_4` are extra variants.
- **New schema — FLATTENED to one row per (id, variant)** so the flat JSON-catalog helpers apply unchanged;
  the Domain catalog re-groups by id. 85 activities -> **133 rows** (44 have a 2nd variant, 3 a 3rd, 1 —
  `Mission_MVP_Waterfront` — a 4th). No `None`, no empty rows.
- Text kept **VERBATIM** (no stray `\u` — \uXXXX-restore): `<col: ...>` markup preserved (132 rows carry
  it); apostrophes round-trip (76 rows). U+21B5 -> real `\n`; C0 control stripped.

## Shape & the helpers
- `DailyActivityEntry{ id, variant, title, hud_description, long_description, order }`.
- `DailyActivityContactCatalog` API: `Find(id, variant=1)`, `Variants(id)` (variant-sorted vector),
  `VariantCount(id)`, `Title/HUDDescription/LongDescription(id, variant=1, def)`, `Count()` (total rows =
  133), `ActivityCount()` (distinct ids = 85). Merge is keyed by **(id, variant)**; order-sorted. Same
  private JSON helpers as the sibling catalogs.

## Notes for other agents
- **Variant selection is a runtime/gameplay decision** — this catalog only stores the text. When the daily
  system assigns an activity, pick a variant (RTW randomises; a faithful port can seed from the day/faction)
  and render `Title/HUDDescription/LongDescription(id, variant)`. Do NOT hardcode daily-activity strings.
- The mapping from a daily-activity id to its objective LOGIC (target counts, tracked units) lives in the
  cooked SDD daily-activity tables / `TrackedActivityUnits.INT`, not here — a follow-up increment.
- **Precedent set:** this is the pattern for any future numbered-variant INT table — flatten to one row per
  (id, variant) in the extractor, add a `variant` field, and key merge/Find on (id, variant).
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  concurrent agent builds can collide with transient `C1083 ... Permission denied`. Not a code error — retry.
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (voice subtitles, largest),
  `Tooltips.INT` (needs a different `Scene@Widget` key parser — NOT the `<Table>_<id>_<Suffix>` schema),
  `TaskOperationUIProfile.INT`, `TrackedActivityUnits.INT`. Dead-end: `InventoryItemPrices.INT` (all empty).
