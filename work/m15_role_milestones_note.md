# m15 — Role-milestone catalog (RoleMilestones.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB role-MILESTONE catalog (the individual ranks/steps that make up a player
role: as you progress a role you clear numbered milestones, each with a display title and — for the
ones that grant loot — a reward-mail subject + body) into the Domain layer, following the established
data-catalog recipe. This complements the role-level `player_roles.json` with per-rank granularity.

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_role_milestones.ps1` |
| Data | `Content/Data/role_milestones.json` (705 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBRoleMilestones.h` (header-only `RoleMilestoneCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`role_milestones` member, load, `role_milestones=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestRoleMilestonesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\RoleMilestones.INT` (UTF-16LE) — mirror of the cooked
  SDD table `RoleMilestones`.
- Three keys per milestone id, grouped by the extractor:
  - `RoleMilestones_<id>_Title=<display title>`
  - `RoleMilestones_<id>_RewardMailSubject=<mail subject>` (usually empty)
  - `RoleMilestones_<id>_RewardMailBody=<mail body>` (usually empty; may contain U+21B5 line-breaks)
- 718 distinct ids in the file; rows where all three fields are empty are dropped -> **705 real rows**
  (705 titles, 21 subjects, 100 bodies, 85 of those with U+21B5 line-breaks collapsed to spaces).
- Text is verbatim (apostrophes/punctuation preserved, no stray `\u` — \uXXXX-restore). No colour
  markup in this table.

## Shape & the role binding
- 705 rows. `RoleMilestone{ id, title, reward_mail_subject, reward_mail_body, order }`.
- `RoleMilestoneCatalog` API: `Find / Title / RewardSubject / RewardBody / HasReward / ForRole /
  Count`, merge-by-id, order-sorted.
- **Binding to the role:** a milestone id carries a trailing `_<NN>` rank
  (e.g. `15th_Anniversary_Celebrations_01`). `static RoleId(id)` strips that suffix to give the
  `player_roles.json` id; `static Rank(id)` parses the number (-1 if none). `RoleIdFor`/`RankFor` do
  the same for a stored milestone. `ForRole(roleId)` returns a role's milestones sorted ascending by
  rank. **309 of 705** milestones bind directly to a `player_roles` row (verified end-to-end via
  `WorldService.progression.FindRole`); the rest are event/legacy milestones whose base id has no
  matching role — callers should `FindRole` the derived id and tolerate a miss.

## Notes for other agents
- This + `player_roles`/`roles` together drive the **role-progression screen**: read a role's ranks
  from `WorldService.role_milestones.ForRole(roleId)`, show each `Title`, and when a rank is cleared
  surface its `RewardSubject`/`RewardBody` through the reward-mail system (not yet built). Do not
  hardcode milestone strings.
- Remaining reconciliation (not blocking): the ~396 milestones whose `RoleId()` does not resolve are
  mostly event/seasonal tracks (ids like `NewVDayPrimary_01_2024` whose trailing `_2024` is stripped
  as a "rank"); build an explicit milestone-id -> role-id alias table if/when those tracks are wired.
  The catalog is ready for that alias map to sit on top.
