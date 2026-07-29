# m15 — reward-package display catalog (RewardPackages.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **reward-package display catalog** — the player-facing text for the named
reward bundles that achievements, role milestones, missions, challenges, seasonal events and Armas
purchases grant — into the Domain layer, following the established data-catalog recipe. This is the
display/prose half of the reward system (the on-disk item payload lives in the cooked SDD and is
resolved separately); it is what the rewards UI / reward-mail body shows the player.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_reward_packages.ps1` |
| Data | `Content/Data/reward_packages.json` (1661 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBRewardPackages.h` (header-only `RewardPackageCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`reward_packages` member, load, `reward_packages=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestRewardPackagesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\RewardPackages.INT` (UTF-16LE) — mirror of the cooked SDD
  table `RewardPackages`. Single `[RewardPackages]` section, 6562 kv lines.
- Two keys per package id, grouped by the extractor:
  - `RewardPackages_<id>_Description=<player-facing description>`
  - `RewardPackages_<id>_OutOfSeasonDescription=<alt text outside a seasonal window>`
- 3281 distinct ids; `OutOfSeasonDescription` is **empty for every id** in the current retail build
  (kept in the schema for event rotation). Rows where both fields are empty (e.g.
  `RewardPackages_None`, 1620 of them) are dropped -> **1661 real rows** (all with a Description).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes (`'Country-Gent'`), quotes,
  and the `APB$` currency text all survive. U+21B5 -> real newline (multi-line prose); other C0
  control chars stripped.

## Shape & the helpers
- 1661 rows. `RewardPackage{ id, description, out_of_season_description, order }`.
- `RewardPackageCatalog` API: `Find / Description / OutOfSeasonDescription / HasDescription /
  DescriptionFor(id,outOfSeason) / ForCategory / Count`, merge-by-id, order-sorted.
- **`static Category(id)`** returns the family token before the first `_`
  (`Challenges_Silver_ShotgunCSG_Vintage` -> `Challenges`, `Ach_BackUp_01` -> `Ach`); id with no `_`
  returns the whole id. `CategoryFor(id)` is the instance form; `ForCategory(cat)` lists a family in
  display order. 20 families observed: Weapon(455)/Symbol(369)/Crim(266)/Enf(266)/Clothing(263)/
  Title(208)/Vehicle(156)/WeaponSkin(116)/Christmas(100)/Ach(90)/Armas(79)/Emote(50)/Halloween(41)/
  RewardPoints(35)/Challenges(33)/Capacity(33)/Easter(32)/Tutorial(31)/Decal(31)/Leased(25)/...
- **`DescriptionFor(id, outOfSeason)`** returns the OOS text when non-empty else the regular
  description — the single call the rewards UI should make once seasonal windows are wired.

## Notes for other agents
- This is the **display** side of the reward system. When the reward-mail path lands (see the
  role-milestone follow-up), look the granted package up by id and drop `Description(id)` /
  `DescriptionFor(id, outOfSeason)` into the mail body / rewards popup. Do not hardcode reward text.
- The **item payload** (what items/cash/currency a package actually contains) is NOT in this INT —
  it is in the cooked SDD (`RewardPackages` object graph) / the apbdb.com item API. A follow-up
  increment should port the payload table (WeightedRewards.INT / RedeemableRewards.INT are the
  adjacent tables) and key it by the SAME package id so `RewardPackage` gains a `contents` list.
- Package ids are referenced by achievements, role milestones (`RoleMilestones`), challenges and
  seasonal contacts — the id is the join key across all of them.
