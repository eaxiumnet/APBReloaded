# m15 — Ceremony-message catalog (HUDCeremonyMsg.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB ceremony-message catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_hud_ceremony_msgs.ps1` |
| Data | `Content/Data/hud_ceremony_msgs.json` (93 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBCeremonyMsgs.h` (header-only `CeremonyMsgCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`ceremony_msgs` member, load, `ceremony_msgs=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestCeremonyMsgsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\HUDCeremonyMsg.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `HUDCeremonyMsg`. Each ceremony has a single `_Title` field.
- **All titles are verbatim from the INT.** Rows with empty titles or DNT titles are dropped;
  93 real rows remain from 103 INT lines.
- `<WeaponName>` placeholder token kept verbatim (2 rows: `Weapon_Pickup`, `Weapon_Override_Equiped`).

## Shape
- 93 rows. `CeremonyMsgDef{ id, title, category, order }`.
- `CeremonyMsgCatalog` API: `Find/Title/Category/ForCategory/Categories/Count`, merge-by-id,
  order-sorted, plus `HasPlaceholder()` for `<Token>` detection.
- Category = first id token: **AM** (54, achievement-manager gameplay events), **Minigame** (15),
  **ProvingGrounds** (10), **Reward** (4), **DailyActivityComplete** (3), **Weapon** (2),
  **TradeCompleted** (1), **TradeCanceled** (1), **JokerTicketStore** (1),
  **DailyActivityAutoAssigned** (1), **TimeLimitedReward** (1).

## Ceremony families (AM category)
- **Combat streaks**: KILL STREAK! / ARREST STREAK! / WIN STREAK!
- **Fame (progression)**: MEDAL EARNED! / STANDING LEVEL UP! / CONTACT MAXED! / ROLE LEVEL UP! /
  ROLE MAXED! / ORGANIZATION MAXED! / NEW ACHIEVEMENT / CONTACT REFERRAL
- **Heat (notoriety/prestige)**: NOTORIETY LEVEL UP/DOWN / PRESTIGE LEVEL UP/DOWN /
  NOTORIETY LEVEL 5! / PRESTIGE LEVEL 5! / BOUNTY AVAILABLE! / BOUNTY!
- **Bounty claimed**: BOUNTY CLAIMED / BAD SHOT! (faction-dependent kill feedback)
- **Rating**: RANK UP!
- **Reward unlocks**: NEW CLOTHING! / NEW WEAPONS! / NEW VEHICLES! / NEW EMOTES! /
  NEW SYMBOL! / NEW TITLE! / NEW MODS! / NEW EQUIPMENT! / NEW FEATURE! / NEW INSTRUMENT! /
  NEW PRIMITIVES! / NEW SONGS! / NEW THEMES! / NEW WEAPON SKINS! / NEW VEHICLE PARTS! /
  DISPLAY POINT!
- **Threat**: HIGH THREAT PLAYER

## Notes for other agents
- This is the authoritative source for the **big on-screen celebration popups** — the most
  recognizable APB HUD feedback element. The HUD reads the ceremony id from the achievement
  system and displays the title with a large animation.
- The `<WeaponName>` token in Weapon_Pickup/Weapon_Override_Equiped must be substituted with
  the weapon's display name at runtime (from `weapon_item_types` or `weapon_display_names`).
- Ceremony triggering logic (when to show which ceremony) lives in the achievement/fame/heat
  systems; this catalog only provides the display text. Wiring achievement events to ceremony
  ids is a follow-up increment.
- The `AM_Heat_BountyClaimed_*` ids encode faction + killer/victim: `You|Any + Crim|Enf +
  Kill + Crim|Enf` — the "BAD SHOT!" title appears for same-faction kills, "BOUNTY CLAIMED"
  for cross-faction kills.
