# m15 — HUD combat score-feed catalog (HUDCombatMessages.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB on-screen combat score-feed catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_hud_combat_messages.ps1` |
| Data | `Content/Data/hud_combat_messages.json` (145 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBHUDCombatMessages.h` (header-only `HUDCombatMessageCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`hud_combat_messages` member, load, `hud_combat_messages=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestHUDCombatMessagesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\HUDCombatMessages.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `HUDCombatMessage`. Each feed entry is a two-line floating
  message: `<id>_Line0` (top) + `<id>_Line2` (bottom).
- **Both lines are verbatim from the INT.** Line0 is usually a token (`<CharacterNameA>`,
  `<MedalName>`, `<GameplayObject>`) or a short label ("Arson", "Teamkill"); Line2 is the score
  message ("Enemy Killed", "Objective Complete", "Demerit!", "Match Won", ...).
- Tokens (including the `($<Score>)` cash line) are preserved by the \uXXXX-restore (no stray `\u`).
- 149 ids exist; the 4 unused Easter placeholders whose Line0 AND Line2 are both empty
  (`Minigame_Mugging_Easter_GainedItems` / `LostItems`, singular + plural) are dropped, leaving
  145 real rows. Entries with only ONE empty line (e.g. `Score_Match_Won` empty top line) are kept.

## Shape
- 145 rows. `HUDCombatMessage{ id, line0, line2, order }`.
- `HUDCombatMessageCatalog` API: `Find/Line0/Line2/Count`, merge-by-id, order-sorted, plus
  `FormatLine0(id, value)` / `FormatLine2(id, value)` which substitute the single `<...>` token
  with a live value (e.g. the killed player's name or the score amount) exactly as the HUD does.

## Notes for other agents
- This is the authoritative source for the **combat score feed** (the floating "Enemy Killed" /
  "Kill Assist" / "Objective Complete" / "Demerit!" toasts). Read from
  `WorldService.hud_combat_messages` and use `FormatLine0/FormatLine2` for token lines — do not
  hardcode the message strings in the HUD.
- The feed covers: `Score_Combat_*` (kill/arrest/assist/stun/rescue), `Score_Earned_Medal*`
  (medal-earned + dishonour "Demerit!" variants — same medal set as `medals.json`),
  `Score_Mission_CSA_*` + `Score_Mission_Assist_*` (objective complete/assist per objective type,
  same objective family as `task_objectives.json`), `Score_Minigame_*` / `Minigame_*`
  (mugging / Halloween + Christmas infection / weapon-drop / survival seasonal events),
  `Mission_ObjectiveProximity_*` (defend prompts), and match/teamkill lines.
- Which event fires which feed id is decided by the live combat-resolve / mission-result /
  medal-award pipeline; this catalog only supplies the display text + token template. Wiring event
  -> feed id is a natural follow-up when the HUD feed widget is built.
- Still-open high-value INT gap: `ModifierEffects.INT` (508 lines, character/consumable stat
  modifiers with `<Color:R=..>` markup + multi-line `_2/_3` variants) needs a markup-aware parser;
  left for a dedicated increment.
