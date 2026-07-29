# m15 — redeemable-reward mail catalog (RedeemableRewards.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB **redeemable-reward MAIL catalog** — the confirmation mail sent when a player
CHOOSES a reward from a set (Retail/Leased weapon presets, clothing, titles, weapon skins, vehicles,
emotes, bundles, capacity unlocks, RewardPoints, Joker boxes, RAF, ...) — into the Domain layer, following
the established data-catalog recipe. This is the **player-choice half** of the reward-mail system, the
counterpart to the standalone weighted-reward mails (`APBWeightedRewards.h` / WeightedRewards.INT) and the
reward-package DISPLAY descriptions (`APBRewardPackages.h` / RewardPackages.INT).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_redeemable_rewards.ps1` |
| Data | `Content/Data/redeemable_rewards.json` (1471 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBRedeemableRewards.h` (header-only `RedeemableRewardCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`redeemable_rewards` member, load, `redeemable_rewards=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestRedeemableRewardsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\RedeemableRewards.INT` (UTF-16LE) — mirror of the cooked SDD
  table `RedeemableRewards`. Single `[RedeemableRewards]` section.
- Two keys per reward id, grouped by the extractor:
  - `RedeemableRewards_<id>_MailSubject=<subject>`
  - `RedeemableRewards_<id>_MailBody=<body>`   (empty for many Leased/preset ids — subject-only)
- 1474 distinct ids; 3 have both fields empty and are dropped -> **1471 real rows**
  (1471 subjects, 370 bodies).
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): apostrophes (90 bodies) survive. The manual
  line-break glyph U+21B5 (72 bodies use it for paragraph breaks) becomes a real `\n`; other C0 control
  chars are stripped.

## Shape & the helpers
- 1471 rows. `RedeemableReward{ id, mail_subject, mail_body, order }`.
- `RedeemableRewardCatalog` API: `Find / MailSubject / MailBody / HasReward / HasBody / ForCategory /
  Count`, merge-by-id, order-sorted.
- **`HasBody(id)`** distinguishes the many subject-only preset ids from the ones that carry an
  instructional body (e.g. `Retail_Shotgun` explains the PAPERCLIP/redeem flow).
- **`static Category(id)`** family token (`Retail_Shotgun` -> `Retail`, `Clothing_...` -> `Clothing`).
  Families: Weapon(353)/Clothing(253)/Title(161)/WeaponSkin(116)/Vehicle(74)/Armas(67)/Emote(50)/
  Challenges(36)/Bundle(36)/Capacity(33)/RewardPoints(31)/Leased(25)/RAF(22)/JokerBox(18)/Fixup(17)/...

## Notes for other agents
- The reward-mail system text is now fully covered by three catalogs: `redeemable_rewards` (player-choice
  confirmation mails), `weighted_rewards` (standalone-grant mails), and `reward_packages` (rewards-UI
  DISPLAY blurbs). When the in-game **mail path** delivers a chosen reward, fill Subject from
  `redeemable_rewards.MailSubject(id)` and Body from `redeemable_rewards.MailBody(id)`; for auto-granted
  rewards use the `weighted_rewards` MailSubjectFor/MailBodyFor pair; for the rewards-UI blurb use
  `reward_packages.Description(id)`. Do NOT hardcode reward-mail strings.
- Still not in the catalog (adjacent, future increment): `RewardPackageItemTypes.INT` (142 per-item
  reward component mails + descriptions, e.g. vehicle customization kits) — same recipe.
- The item PAYLOAD (which items/cash a reward actually contains) still lives in the cooked SDD /
  apbdb.com item API, not any of these INTs — a separate payload increment is needed to attach a
  `contents` list to a reward id.
