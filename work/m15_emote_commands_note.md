# m15 — Emote-command catalog (EmoteCommands.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB emote-command catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_emote_commands.ps1` |
| Data | `Content/Data/emote_commands.json` (50 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBEmoteCommands.h` (header-only `EmoteCommandCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`emote_commands` member, load, `emote_commands=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestEmoteCommandsFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\EmoteCommands.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `EmoteCommand`. Each emote has two fields:
  `SlashCommand` / `DisplayName`.
- **Both strings are verbatim from the INT.** All 50 emotes in the INT have non-empty display
  names (no placeholder rows dropped).
- Emote ids may contain spaces (e.g. `Body Pop`, `Dance 80s`, `Strike A Pose 1`); the extractor
  regex handles this via non-greedy match up to the known `_SlashCommand`/`_DisplayName` suffix.

## Shape
- 50 rows. `EmoteCommandDef{ id, slash_command, display_name, order }`.
- `EmoteCommandCatalog` API: `Find/SlashCommand/DisplayName/Count/DanceCount`, merge-by-id,
  order-sorted.
- **`FindBySlashCommand(cmd)`** resolves a slash command to its emote (case-insensitive).
  E.g. `/dance` -> Dance, `/WAVE` -> Wave, `/bodypop` -> Body Pop.
- `IsDance()` / `DanceCount()` classify the 15 dance-variant emotes (ids starting with "Dance").

## Emote families
- **Dance** (15): Dance, Dance 80s, Dance Airguitar, Dance Ballet, Dance Comical, Dance Goth,
  Dance Hip Hop, Dance Irish, Dance Metal, Dance Michael, Dance Punk, Dance Robot,
  Dance Signature, Dance Techno, Dance Urban.
- **General** (35): Angry, Animal, Body Pop, Bored, Bow, Brag, Celebrate, Chicken, Chuckle,
  Clap, Coin Toss, Cold, Confused, Congratulations, Cry, Disagree, Fart, Flirt, Hello, Insult,
  No, Ready, Shock, Smoke, Strike A Pose 1, Strike A Pose 2, Surrender, Taunt, Threaten,
  Thumbs Up, Victory, Wait, Wave, Whistle, Yes.

## Notes for other agents
- This is the authoritative source for the **emote wheel UI** (display names), **chat
  autocomplete** (slash commands), and **animation binding** (emote id -> animation asset).
- The animation mapping (emote id -> animation asset/sequence) is a separate increment that
  needs the retail `Anim\*.upk` extraction (M13/M17 scope per ARCHITECTURE.md §6).
- Emote unlock availability (some emotes are premium/unlockable) lives in the SDD cooked data,
  not the INT mirror; that entitlement data is a follow-up increment from apbdb / SDD.
- The `ChatService::ParseCommand` could be extended to recognize emote slash commands and
  route them to the animation system instead of the chat router.
