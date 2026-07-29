# m15 — weighted-reward mail catalog (WeightedRewards.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **weighted-reward MAIL catalog** — the in-game mail sent when a standalone
weighted reward is granted (contact/organisation biography lore, weapon/consumable/deployable reward
mails, minigame + legendary drops, seasonal grants) — into the Domain layer, following the established
data-catalog recipe. This is the **mail-body half** of the reward system, the counterpart to the
reward-package DISPLAY descriptions (`APBRewardPackages.h` / RewardPackages.INT) landed just before it.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_weighted_rewards.ps1` |
| Data | `Content/Data/weighted_rewards.json` (189 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBWeightedRewards.h` (header-only `WeightedRewardCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`weighted_rewards` member, load, `weighted_rewards=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestWeightedRewardsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\WeightedRewards.INT` (UTF-16LE) — mirror of the cooked SDD
  table `WeightedRewards`. Single `[WeightedRewards]` section, 5108 kv lines.
- Four keys per reward id, grouped by the extractor:
  - `WeightedRewards_<id>_RewardMailSubject=<subject>`
  - `WeightedRewards_<id>_RewardMailBody=<body>`
  - `WeightedRewards_<id>_OutOfSeasonSubject=<alt subject>`  (empty for every id in current retail)
  - `WeightedRewards_<id>_OutOfSeasonBody=<alt body>`        (empty for every id in current retail)
- 1277 distinct ids; the large `E_*` (413) / `C_*` (412) families are Enforcer/Criminal reward-pool
  placeholders with all-empty text. Rows where all four fields are empty (1088 of them) are dropped
  -> **189 real rows** (189 subjects, 132 bodies).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes (`world's`, `'suspect'`)
  survive. The manual line-break glyph U+21B5 (94 bodies use it for paragraph breaks) becomes a real
  `\n`; other C0 control chars are stripped.

## Shape & the helpers
- 189 rows. `WeightedReward{ id, reward_mail_subject, reward_mail_body, out_of_season_subject,
  out_of_season_body, order }`.
- `WeightedRewardCatalog` API: `Find / RewardSubject / RewardBody / OutOfSeasonSubject /
  OutOfSeasonBody / HasReward / MailSubjectFor(id,outOfSeason) / MailBodyFor(id,outOfSeason) /
  ForCategory / Count`, merge-by-id, order-sorted.
- **`MailSubjectFor(id, outOfSeason)` / `MailBodyFor(id, outOfSeason)`** return the OOS variant when
  non-empty else the regular text — the two calls the mail system should make once seasonal windows
  are wired.
- **`static Category(id)`** family token (`Bio_Agrotech` -> `Bio`, `Legendary_Corsair_JT` ->
  `Legendary`). Families: E(413)/C(412)/Deployable(172)/Bio(89)/Consumable(86)/Minigame(46)/
  Legendary(36)/Prototype(7)/WeaponPrototype(5)/JokerTickets(5)/... (most E/C are empty, dropped).

## Notes for other agents
- This + `reward_packages` (display descriptions) together cover the **reward-mail system** text. When
  the in-game **mail path** delivers a granted reward, fill Subject from `MailSubjectFor(id, ...)` and
  Body from `MailBodyFor(id, ...)`; use `reward_packages.Description(id)` for the rewards-UI blurb.
  Do NOT hardcode reward-mail strings. This directly unblocks the role-milestone follow-up
  ("tie milestone/mission completion to the reward-mail system").
- Still not in the catalog (adjacent, future increments): `RedeemableRewards.INT` (1474 player-choice
  reward mails — Retail/Leased weapon presets) and `RewardPackageItemTypes.INT` (142 per-item reward
  component mails + descriptions, e.g. vehicle customization kits). Both follow the SAME recipe.
- The item PAYLOAD (which items/cash a reward actually contains) still lives in the cooked SDD /
  apbdb.com item API, not any of these INTs — a separate payload increment is needed to attach a
  `contents` list to a reward id.
