# m15 — Task-operation UI-profile tracked-value labels (TaskOperationUIProfile.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0; real_compiler_errors=0)

## What landed
Extracted the retail APB **task-operation UI-profile** table — the per-tracked-value HUD sub-labels each
mission OPERATION shows beside its progress counters — into the Domain layer. Every mission stage runs an
"operation" (AntiGraffiti, ArmedGuard, BombDisposal, Escape120, ...) whose id shares the TaskOperation id
space 1:1; that operation's UI profile carries a fixed 4-slot array of short labels (`TrackedValueDescription
[0..3]`). Slot 0 is the primary label ("Cover Graffiti:", "Guard Targets:"); multi-counter operations fill
later slots ("Bombs Armed:" / "Bombs Disarmed:"). This is the **per-tracked-value companion** to the already
-ported `task_operations` catalog (`MissionOperationCatalog`), which holds the single `UIDescription` per op.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_task_operation_ui_profiles.ps1` |
| Data | `Content/Data/task_operation_ui_profiles.json` (178 profiles) |
| Domain catalog | `Source/APBReloaded/Domain/APBTaskOperationUIProfiles.h` (header-only `TaskOperationUIProfileCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`task_operation_ui_profiles` member, load, `task_operation_ui_profiles=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestTaskOperationUIProfilesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\TaskOperationUIProfile.INT` (UTF-16LE) — mirror of the cooked SDD
  table `TaskOperationUIProfile`. Single `[TaskOperationUIProfile]` section, 772 kv lines.
- Key grammar: `TaskOperationUIProfile_<id>_TrackedValueDescription[<n>]` where `<n>` is 0..3. This is a **new
  indexed-array schema** (bracketed index), distinct from the numbered-variant `_<n>` seen in DailyActivity.
- **Flattened to one row per profile** with a fixed `desc0..desc3` quartet so the flat JSON-catalog helpers
  apply unchanged. 193 raw profiles × 4 slots = 772 lines → **178 rows** (15 all-empty placeholders such as
  `None`/`Simple` dropped by the keep-if-any-slot-non-empty rule). desc0 filled in 174, desc1 30, desc2 6,
  desc3 4; 34 profiles carry ≥2 slots.
- Text kept **VERBATIM** (no stray `\u` — \uXXXX-restore): `<col: ...>` markup preserved (2 rows carry it);
  U+21B5 → real `\n`; C0 control stripped.

## Shape & the helpers
- `TaskOperationUIProfileEntry{ id, std::string desc[4], order }`.
- `TaskOperationUIProfileCatalog` API: `Find(id)`, `TrackedValueDescription(id, index, def)` (index 0..3;
  out-of-range/negative → def), `PrimaryDescription(id, def)` (slot 0), `TrackedValueCount(id)` (non-empty
  slots; 0 if unknown), `Descriptions(id)` (non-empty slots in order), `Count()` (= 178). Merge is keyed by
  **id**; order-sorted. Same private JSON helpers as the sibling catalogs.

## Notes for other agents
- **Use with the mission HUD:** when a stage of operation `<id>` is active, render its tracked counters using
  `PrimaryDescription(id)` for the main counter and `TrackedValueDescription(id, n)` for the nth. Do NOT
  hardcode these labels. The operation id is the same one `MissionOperationCatalog` / `task_operations` uses.
- **Precedent set:** this is the pattern for any future INDEXED-ARRAY INT table (`Key[<n>]`) — flatten to one
  row per id with fixed `descN` fields (or an array), keep-if-any-non-empty, merge-by-id. (Distinct from the
  numbered-variant `_<n>` flatten in `m15_daily_activity_contacts_note.md`, which keys on (id, variant).)
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  concurrent agent builds can collide with transient `C1083 ... Permission denied`. Not a code error — retry.
- Build-check gotcha: the naive `: error ` regex matches the passing test-name string "PASS: error banner ..."
  — use `error C\d{3,4}|fatal error` to count REAL compiler errors (this build: 0).
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
- Remaining unported high-value INT tables: `Subtitles_MASC/FEM.int` (8863 voice-line subtitles EACH — but
  masc/fem text is **byte-identical** in this build, so port ONE flat id→text catalog, not a gender pair);
  `Tooltips.INT` (412 kv across 54 `[Scene]` sections, `Scene@Widget` key grammar — needs a **section-scoped**
  parser, NOT the `<Table>_<id>_<Suffix>` schema); `TrackedActivityUnits.INT` (only 4 rows — number-format
  templates); `TaskItemSizes.INT` (7 rows — item-size display names). Dead-end: `InventoryItemPrices.INT`
  (all empty).
