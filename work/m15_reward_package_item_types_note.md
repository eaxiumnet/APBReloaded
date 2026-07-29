# m15 — Reward-package item-type text catalog (RewardPackageItemTypes.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0; real_compiler_errors=0)

## What landed
Extracted the retail APB **reward-package item-type text catalog** — the `id -> {description, mail subject,
mail body}` prose for a "reward package" (a bundle granted through progression, Armas, the Joker
Distribution or an event: an outfit, a vehicle-component kit, a weapon package, ...) — into the Domain
layer, following the established data-catalog recipe. This is the **TEXT companion** to the existing
`reward_packages` catalog (`APBRewardPackages.h`): reward_packages holds the package -> item mapping /
metadata; this holds the prose shown when the package is described in the store/inventory or delivered by
in-game mail.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_reward_package_item_types.ps1` |
| Data | `Content/Data/reward_package_item_types.json` (139 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBRewardPackageItemTypes.h` (header-only `RewardPackageItemTypeCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`reward_package_item_types` member, load, `reward_package_item_types=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestRewardPackageItemTypesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\RewardPackageItemTypes.INT` (UTF-16LE) — mirror of the cooked
  SDD table `RewardPackageItemTypes`. Single `[RewardPackageItemTypes]` section, 426 kv lines.
- Three keys per package id: `_Description`, `_MailSubject`, `_MailBody`.
- 142 distinct ids; kept if ANY of the three fields carries text -> **139 rows** (3 all-empty dropped, no
  `None` placeholder present). Field density: 125 descriptions, 61 mail subjects, 43 mail bodies.
- Prose kept **VERBATIM** (no stray `\u` — \uXXXX-restore): MailSubject strings embed literal
  double-quotes (`Joker Distribution: "C.S.A. Sting" Outfit!`, 13 rows) that round-trip through JSON `\"`.
  `<col: ...>` markup preserved; U+21B5 -> real `\n`; C0 control stripped.

## Shape & the helpers
- 139 rows. `RewardPackageItemTypeEntry{ id, description, mail_subject, mail_body, order }`.
- `RewardPackageItemTypeCatalog` API: `Find / Description / MailSubject / MailBody / ForCategory / Count`,
  merge-by-id, order-sorted. Same private JSON helpers as the sibling catalogs.
- **`static Category(id)` returns the SECOND token** — every id is prefixed `RewardPackage_`, so the
  meaningful grouping is the token after it (`RewardPackage_Outfit_CSASting_Male` -> `Outfit`;
  `RewardPackage_Components_Espacio_Kit1` -> `Components`). Families in practice: Outfit, Components,
  Weapon, Vehicle, etc.

## Notes for other agents
- Pair with `reward_packages` (id -> contents/metadata): when the store/mail UI shows a package, use
  `reward_package_item_types.Description(id)` for the blurb and `.MailSubject(id)`/`.MailBody(id)` for the
  delivery mail. Do NOT hardcode package strings.
- **Reward PAYLOAD increment still pending (next big step):** attach a `contents` list (item ids + counts
  + lease flags) to each reward/package id from cooked SDD / apbdb.com so completion grants real items;
  `inventory_item_types` + `unlock_item_types` resolve those ids to display text, and this catalog + the
  mail system present them. Payload data is NOT in any INT — it lives in the cooked SDD / apbdb item API.
- Build hazard (multi-AI): `build_and_run.ps1` emits intermediate `.obj` at the repo root (no `/Fo`), so
  concurrent agent builds can collide with transient `C1083 ... Permission denied`. Not a code error — retry.
- Recipe unchanged: extractor -> json -> header-only catalog (no build_and_run.ps1 edit) -> 4 sequential
  WorldService edits -> test + main() reg -> build green -> note + _active bullet -> clean temps.
- Remaining unported high-value INT tables (per the running list): `Subtitles_MASC/FEM.int` (voice
  subtitles, largest), `Tooltips.INT` (needs a different `Scene@Widget` key parser — NOT the
  `<Table>_<id>_<Suffix>` schema), `DailyActivityContacts.INT` (has numbered `_Title_2/_3/_4` variants),
  `TaskOperationUIProfile.INT`. Dead-end: `InventoryItemPrices.INT` (all empty — real prices in apbdb).
