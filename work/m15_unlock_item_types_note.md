# m15 — unlock item-type catalog (UnlockItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **unlock item-type catalog** — the `id -> player-facing description` map for
"unlock items" (tokens/entitlements granted through progression, Armas, the Joker Store or events that
enable something for the character: emotes, inventory-capacity increases for clothing/outfit/symbol/song
slots, daily-activity unlocks, chat/marker features, ...) — into the Domain layer, following the
established data-catalog recipe. This is the text the inventory / store / progression UI shows to explain
what an unlock grants, and a **companion to the master item-name dictionary** in `APBInventoryItemTypes.h`
(InventoryItemTypes.INT) ported in the previous increment.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_unlock_item_types.ps1` |
| Data | `Content/Data/unlock_item_types.json` (1972 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBUnlockItemTypes.h` (header-only `UnlockItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`unlock_item_types` member, load, `unlock_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestUnlockItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\UnlockItemTypes.INT` (UTF-16LE) — mirror of the cooked SDD
  table `UnlockItemTypes`. Single `[UnlockItemTypes]` section, 8655 kv lines.
- One key per unlock id: `UnlockItemTypes_<id>_Description=<player-facing text>`.
- 8655 distinct ids; only **1972 carry a non-empty description** — rows with an empty description are
  dropped (leaving the unlocks that actually render text). Of the 1972, **83 are internal `DNT - ...`
  (Do-Not-Translate) developer notes** kept verbatim for a faithful 1:1 mirror; the other 1889 are real
  player-facing strings (e.g. `Unlock_Emote_Angry` -> `Unlocks the Angry Emote - "/angry".`).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): embedded double-quotes round-trip through
  JSON `\"` (the `/emote` slash-command names are quoted). U+21B5 -> real `\n`; C0 control stripped.

## Shape & the helpers
- 1972 rows. `UnlockItemType{ id, description, order }`.
- `UnlockItemTypeCatalog` API: `Find / Description / HasDescription / ForCategory / Count`, merge-by-id,
  order-sorted. Same private JSON helpers as the sibling catalogs.
- **`static Category(id)`** returns the FIRST token (`Unlock_Emote_Angry` -> `Unlock`). Families are the
  second segment in practice (Emote / Capacity / DailyActivity / ...); ForCategory groups by first token.

## Notes for other agents
- **Dead-end tables checked this pass:** `InventoryItemPrices.INT` (5495 rows) is a stub — EVERY
  `CustomAdditionalItem` value is empty; real prices live in the apbdb catalogs
  (`weapons_catalog.json` / `vehicles_catalog.json` / `apbdb_meta.json`), NOT in that INT. Do not port it.
- This catalog pairs with `inventory_item_types` (id->name): when the unlock/store UI shows an unlock,
  use `unlock_item_types.Description(id)` for the blurb and `inventory_item_types.DisplayName(id)` for the
  granted item's name. Do NOT hardcode unlock strings.
- **Reward PAYLOAD increment still pending (next big step):** attach a `contents` list (item ids + counts
  + lease flags) to each reward/package id from cooked SDD / apbdb.com so completion grants real items;
  `inventory_item_types` + `unlock_item_types` resolve those ids to display text. Payload data is NOT in
  any INT — it lives in the cooked SDD / apbdb item API.
