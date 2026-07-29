# m15 — Modification-effect tooltip catalog (ModifierEffects.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB modification-effect tooltip catalog (the coloured, multi-line stat
descriptions shown for character / vehicle / weapon / consumable mods in the modification screen
and item inspector) into the Domain layer, following the established data-catalog recipe
(extractor .ps1 -> JSON -> header-only catalog -> WorldService wiring -> test). This closes the
long-flagged #1 open INT gap.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_modifier_effects.ps1` |
| Data | `Content/Data/modifier_effects.json` (163 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBModifierEffects.h` (header-only `ModifierEffectCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`modifier_effects` member, load, `modifier_effects=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestModifierEffectsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\ModifierEffects.INT` (UTF-16LE) — the localized mirror
  of the cooked SDD table `ModifierEffect`.
- Each mod owns ONE OR MORE description lines. The keys are **scattered** across the INT:
  `ModifierEffects_<id>_Description` (line 1) + `ModifierEffects_<id>_Description_<N>` (lines 2,3,4).
  The extractor groups by id (hashtable-of-hashtables keyed id -> N), reassembles lines 1..maxN
  (missing -> ""), trims trailing empties, and drops mods whose lines are all empty (TestMod /
  placeholder Minigame_* stubs). 163 real rows remain.
- **Lines are verbatim from the INT**, including the inline `<Color:R=.. G=.. B=..>` markup that
  recolours the following text. Retail quirks are preserved as-is: a malformed tag with no closing
  `>` (`<Color:R=1 G=1 B=1 health`), unknown/closing tags (`</Col>`), swapped channel order
  (`<Color:R=1 B=1 G=1>`), and a leading-space tag (`<Color: R=..>`). U+21B5 line-break glyphs and
  other control chars are collapsed to a space during extraction. No stray `\u` (\uXXXX-restore).
- `category` = the first `_`-token of the id: **Character** (27), **Usable** (4), **Vehicle** (40),
  **Weapon** (92).

## Shape
- 163 rows. `ModifierEffect{ id, category, lines[], order }`.
- `ModifierEffectCatalog` API: `Find / Lines / LineCount / PlainLines / ForCategory / Categories /
  Count`, merge-by-id, order-sorted.
- **Markup-aware helpers live in the header (tested):**
  - `ParseSegments(line)` -> `vector<ColorSegment{text,r,g,b}>`: splits a raw line into coloured
    runs the UE tooltip can render. Text before the first tag is white (1,1,1); each `<Color:...>`
    recolours everything after it. Channel parse (`ParseChannel`) is tolerant of channel order and
    a leading space. Unknown/closing tags are dropped from output; a `<` with no `>` is literal.
  - `PlainText(line)` -> readable string with all markup stripped.
- `RawStrArray` in the parser tolerates both a JSON array `"lines":["a","b"]` AND a bare scalar
  `"lines":"a"`, so it is immune to a PS 5.1 ConvertTo-Json single-element-array unwrap (the user's
  pwsh 7 preserves single-element arrays correctly; this is a belt-and-braces guard).

## Notes for other agents
- This is the authoritative source for **mod tooltip text**. When the modification screen / item
  inspector UI is built, read from `WorldService.modifier_effects` and render `ParseSegments` runs
  (or `PlainText` for plain contexts) — do not hardcode the stat strings.
- **Follow-up (not done here):** bind each catalog mod item (Armas / inventory mod entries) to its
  `modifier_effects` id so the inspector can resolve a purchased/equipped mod to its tooltip. The
  id namespace here (`Weapon_Bandolier1`, `Vehicle_Nitro3`, `Character_Kevlar2`,
  `Usable_Consumable_Epinephrine`, ...) should be reconciled against the Armas/inventory mod ids
  when that screen is wired.
- The green/red convention in the data: green (`R=0 G=1 B=0`) = a buff, red (`R=1 G=0 B=0`) = a
  drawback, yellow (`R=0.7 G=0.7 B=0`) = vehicle blast/handling, purple (`R=0.7 G=0 B=0.7`) = a
  deployed-object name. Preserve these when styling the tooltip.
