# M15 — Real retail contact level counts (ContactLevels.INT → contact_levels.json)

**Agent:** Qoder · **Date:** 2026-07-20 · **Status:** DONE, all 17 domain suites `FAILS=0`

## What this closes
The M15 "parse retail `ContactLevels.INT`" remaining item. The Domain previously applied one
invented `LevelLadder` (levels 0..15) to *every* contact. APB contacts have **different** level
counts. Those real counts are now recovered from the shipped retail localization file and drive
a per-contact ladder length.

## Source data
- **File:** `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ContactLevels.INT`
  (1339 lines). Header says: *"automatically generated … modify the SDD table: ContactLevel"* —
  i.e. this INT is the localization **mirror** of the cooked SDD `ContactLevel` table.
- **Key shape:** `ContactLevels_<Contact>_Level<NN>_RewardMailSubject=<text>` (+ `_RewardMailBody`).
  Per-contact `max_level` = highest `<NN>` present. Most subjects are the `DNT - DO NOT TRANSLATE`
  localization stub; a few real ones (e.g. Financial_C01) are captured as evidence.
- **IMPORTANT:** numeric per-level *standing thresholds* are **NOT** in this file (they live in the
  cooked SDD binary). Only the level *count* per contact is real here. The `LevelLadder` threshold
  values therefore remain **tunable recreation defaults** — do not present them as retail facts.

## Extraction tool (reusable / re-runnable)
`tools/scripts/extract_contact_levels.ps1`
- Params: `-Source` (defaults to the retail INT path), `-Out` (defaults to `Content/Data/contact_levels.json`).
- Emits a **pure top-level JSON array** (matches `contacts_lore.json` so the Domain
  `JsonSplitObjects` parser reads one object per contact). Each object:
  `{ contact_id, max_level, reward_mail_subjects[], source }`.
- Re-run any time the retail build updates: `pwsh -NoProfile -File tools\scripts\extract_contact_levels.ps1`
- Current output: **73 contacts** (Binky=3, Clyde=1, CriminalDefault=10, EnforcerDefault=1,
  Financial_C01=5, Financial_C07=15, Financial_C11..13=20, Waterfront_C11=20, all
  Organisation_*/Seasonal=1, etc.).

## Domain changes (`Source/APBReloaded/Domain/APBProgression.{h,cpp}`)
- `ProgressionCatalog.contact_max_level` (map, keyed by **normalized** id) + `LoadContactLevelsFromText/File`.
- `ContactMaxLevel(id)` — accepts padded or lore id form; 0 if unknown.
- `LadderForContact(id)` — ladder sized to the contact's real max level, else `DefaultContactLadder()`.
- `LevelLadder::ContactLadderWithMaxLevel(n)` — factored out; `DefaultContactLadder()` now delegates (n=15).
- `NormalizeContactId(id)` — strips leading zeros from a trailing digit run so the zero-padded
  ContactLevels ids (`Financial_C01`) resolve against the lore ids (`Financial_C1`).

## WorldService wiring (`APBWorldService.{h,cpp}`)
- New `ProgressionCatalog progression` member, loaded in `InitFromDataDir` (contacts_lore + roles +
  contact_levels). `INIT` log line now reports `contact_levels=<n>`.
- `CaptureSnapshot` computes `active_contact_level` via `progression.LadderForContact(contact_id)`
  instead of the fixed default ladder.

## Test
`TestContactLevelsFromRetail` in `tests/run_domain_tests.cpp` — asserts known max levels,
normalization (lore↔padded), per-contact ladder sizing, and end-to-end load via `InitFromDataDir`.

## Follow-ups for other agents
- `PlayerRoles.INT` is still unparsed (role DisplayName/Description strings — achievement/title
  localization, not a numeric table). Separate increment if role display names are wanted.
- Real per-level standing thresholds need the cooked SDD `ContactLevel` table (binary) — out of
  scope until an SDD reader lands.
- Contact/kiosk UMG still pending (the M15 UI half).
