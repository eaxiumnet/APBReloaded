# m15 — Frontend/menu UI hover tooltips (Tooltips.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0; real_compiler_errors=0)

## What landed
Extracted the retail APB **UI tooltip** table — the short hover-hint text shown for frontend/menu widgets —
into the Domain layer. Unlike every other localization catalog ported so far (flat
`<Table>_<id>_<Suffix>=<value>` grammar), Tooltips.INT is **SECTION-SCOPED**: each `[<Scene>]` header groups
the tooltips for one UI scene, and within it every key is `<Scene>@<Widget>=<tooltip text>`. A UI widget
looks up its hover hint by (scene, widget) — e.g. scene `Login_Scene`, widget `UILabelButton_TOS` ->
"Create a new APB Account.".

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_tooltips.ps1` |
| Data | `Content/Data/tooltips.json` (409 rows across 53 scenes) |
| Domain catalog | `Source/APBReloaded/Domain/APBTooltips.h` (header-only `TooltipCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`tooltips` member, load, `tooltips=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestTooltipsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\Tooltips.INT` (UTF-16LE) — mirror of the cooked SDD tooltip
  table. 54 `[<Scene>]` sections, 412 kv lines total.
- Key grammar: within a `[<Scene>]` section, keys are `<Scene>@<Widget>=<text>`. The prefix before `@` always
  equals the section header (verified: prefix==section for every row). This is a **new section-scoped schema**,
  distinct from the flat `<Table>_<id>_<Suffix>` and indexed-array `Key[<n>]` schemas.
- **Flattened to one row per (scene, widget)** so the flat JSON-catalog helpers apply unchanged; the Domain
  catalog re-groups by scene. 412 keys -> **409 rows** (3 empty-text placeholder widgets dropped by the
  keep-if-non-empty rule; 53 of 54 scenes retain at least one tooltip).
- Text kept **VERBATIM** (no stray `\u` — \uXXXX-restore; 43 rows carry apostrophes round-tripped): no
  `<col:>` markup or embedded newlines exist in this build; U+21B5 -> real `\n` and C0 control stripped
  defensively.

## Shape & the helpers
- `TooltipEntry{ scene, widget, text, order }`.
- `TooltipCatalog` API: `Find(scene, widget)`, `TooltipFor(scene, widget, def)`, `ForScene(scene)` (order
  -sorted vector), `SceneTooltipCount(scene)`, `SceneCount()` (= 53 distinct scenes), `Count()` (= 409). Merge
  is keyed by **(scene, widget)**; order-sorted. Same private JSON helpers as the sibling catalogs.

## Notes for other agents
- **Use with the frontend UMG/Slate UI:** when a widget shows a hover tooltip, resolve it via
  `TooltipFor(scene, widget)`. Do NOT hardcode tooltip strings. Lookups are **scene-scoped** — the same widget
  id under a different scene is a different tooltip (`Find("Login_Scene","UILabelButton_Logout")` is null; the
  logout tooltip lives under `Lobby_Scene`).
- **Precedent set:** this is the pattern for any future SECTION-SCOPED INT table (`[<Section>]` + `<key>@<sub>`
  ) — track the `[<Section>]` header, key on (section, sub), flatten to one row per pair, merge on that pair.
  (Distinct from the flat and indexed-array flattens in the sibling notes.)
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  concurrent agent builds can collide with transient `C1083 ... Permission denied`. Not a code error — retry.
- Build-check gotcha: the naive `: error ` regex matches the passing test-name string "PASS: error banner ..."
  — use `error C\d{3,4}|fatal error` to count REAL compiler errors (this build: 0).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (8863 voice-line subtitles EACH — but
  masc/fem text is **byte-identical** in this build, so port ONE flat id->text catalog, not a gender pair);
  `TrackedActivityUnits.INT` (only 4 rows — number-format templates); `TaskItemSizes.INT` (7 rows — item-size
  display names). Dead-end: `InventoryItemPrices.INT` (all empty).
