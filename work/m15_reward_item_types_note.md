# m15 — reward-package item-type catalog (RewardPackageItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **reward-package ITEM-TYPE catalog** — the per-component entries of a reward
package (vehicle customization kits, clothing/outfit/title/weapon-skin components, seasonal + affiliate
items) — into the Domain layer, following the established data-catalog recipe. Each item type carries
BOTH a rewards-UI DISPLAY description AND (for many) a confirmation mail subject/body. This is the
per-ITEM layer beneath the reward-package display blurbs (`APBRewardPackages.h` / RewardPackages.INT)
and the reward-mail catalogs (`APBWeightedRewards.h`, `APBRedeemableRewards.h`). **This completes the
reward-text INT family** — all four reward-text tables are now in the Domain layer.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_reward_item_types.ps1` |
| Data | `Content/Data/reward_item_types.json` (139 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBRewardItemTypes.h` (header-only `RewardItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`reward_item_types` member, load, `reward_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestRewardItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\RewardPackageItemTypes.INT` (UTF-16LE) — mirror of the cooked
  SDD table `RewardPackageItemTypes`. Single `[RewardPackageItemTypes]` section, 432 kv lines.
- Three keys per item id, grouped by the extractor:
  - `RewardPackageItemTypes_<id>_Description=<rewards-UI blurb>`
  - `RewardPackageItemTypes_<id>_MailSubject=<mail subject>`  (empty for many desc-only components)
  - `RewardPackageItemTypes_<id>_MailBody=<mail body>`        (empty for many desc-only components)
- 142 distinct ids; 3 have all three fields empty and are dropped -> **139 real rows**
  (125 descriptions, 61 subjects, 43 bodies).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): embedded double-quotes survive (e.g. the
  `RewardPackage_Outfit_CSASting_Male` subject `Joker Distribution: "C.S.A. Sting" Outfit!` round-trips
  through JSON `\"`). The manual line-break glyph U+21B5 (33 rows) becomes a real `\n`; other C0 control
  chars are stripped.

## Shape & the helpers
- 139 rows. `RewardItemType{ id, description, mail_subject, mail_body, order }`.
- `RewardItemTypeCatalog` API: `Find / Description / MailSubject / MailBody / HasDescription / HasMail /
  ForCategory / Count`, merge-by-id, order-sorted.
- **`static Category(id)`** returns the SECOND token because every id shares the `RewardPackage_` prefix
  (`RewardPackage_Components_Espacio_Kit1` -> `Components`, `RewardPackage_Outfit_CSASting_Male` ->
  `Outfit`). Families: Clothing(32)/Title(21)/Christmas(19)/Outfit(12)/Components(11)/Affiliates(10)/
  WeaponSkin(8)/PoliceLightsMain(5)/Vehicle(3)/...

## Notes for other agents
- The reward-text INT family is now COMPLETE across four Domain catalogs:
  - `reward_packages` (RewardPackages.INT) — top-level reward-bundle DISPLAY blurbs.
  - `weighted_rewards` (WeightedRewards.INT) — standalone auto-grant reward mails.
  - `redeemable_rewards` (RedeemableRewards.INT) — player-choice reward confirmation mails.
  - `reward_item_types` (RewardPackageItemTypes.INT) — per-component descriptions + grant mails (this).
  When the in-game mail/rewards-UI path renders a reward or its components, pull text from these
  catalogs by id; do NOT hardcode reward strings.
- The item PAYLOAD (which concrete items/cash/mods a reward or component actually contains, quantities,
  lease durations) still lives in the cooked SDD / apbdb.com item API, not in any of these INTs. The
  remaining reward increment is a **payload increment**: attach a `contents` list (item ids + counts +
  lease flags) to each reward/package id so milestone/mission completion can grant the real items, using
  these text catalogs only for the display/mail wrapper.
