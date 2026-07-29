# m15 — Chat-message-category catalog (ChatMessageCategories.INT) — handoff note

**Author:** Cline  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB chat-channel catalog into the Domain layer, following the
established data-catalog recipe (extractor .ps1 -> JSON -> header-only catalog -> WorldService
wiring -> test).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_chat_message_categories.ps1` |
| Data | `Content/Data/chat_message_categories.json` (21 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBChatMessageCategories.h` (header-only `ChatMessageCategoryCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`chat_message_categories` member, load, `chat_message_categories=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestChatMessageCategoriesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\ChatMessageCategories.INT` (UTF-16LE) — the localized
  mirror of the cooked SDD table `ChatMessageCategory`. Each channel has five fields:
  `SlashCommand` / `SecondarySlashCommand` / `Tag` / `Description` / `SyntaxExample`.
- **All five strings are verbatim from the INT.** The `None` placeholder (Tag = DNT) is dropped;
  21 real channels remain.
- 12 channels have real slash commands the player can type (Clan /c, District /d, Group /g,
  Officer /o, Reply /r, Say /s, Team /t, Whisper /w, Yell /y, Trade /tr).
- 11 channels are system-only (no player slash command; DNT fields): Broadcast_System, Combat,
  Mission, AutoReply, System, Vehicle, Whisper_Sent, Tutorial, Minigame, GiftBox, Challenge.

## Shape
- 21 rows. `ChatMessageCategoryDef{ id, slash_command, secondary_slash_command, tag, description,
  syntax_example, order }`.
- `ChatMessageCategoryCatalog` API: `Find/Tag/SlashCommand/SecondarySlashCommand/Description/
  SyntaxExample/Count/PlayerChannelCount`, merge-by-id, order-sorted.
- **`FindBySlashCommand(cmd)`** resolves either primary or secondary command (case-insensitive)
  to its channel. E.g. `/c` -> Clan, `/clan` -> Clan, `/TR` -> Trade.
- `HasSlashCommand()` / `HasSecondarySlashCommand()` distinguish real commands from DNT sentinels.

## Key retail findings (discrepancies with existing Domain ChatService)
- **Trade uses `/tr` not `/trade`** — the Domain `ChatService::ParseCommand` maps `/trade` but
  not `/tr`. The secondary `/trade` is the long-form alias.
- **`/t` is Team in retail**, not Whisper — the Domain maps `/t` to Whisper (from `/tell`).
  Retail uses `/w`/`/whisper` for Whisper, `/t`/`/team` for Team.
- **Officer channel (`/o`)** — clan officer chat; not in the Domain's `ChatChannel` enum.
- **Reply (`/r`)** — reply-to-last-whisper; not in the Domain's `ChatChannel` enum.
- **Yell (`/y`)** — wider-range local chat; not in the Domain's `ChatChannel` enum.

## Notes for other agents
- This is the authoritative source for the **chat UI help/autocomplete** (slash command list +
  descriptions + syntax examples) and for **validating the Domain ChatService slash commands**
  against retail.
- The Domain `ChatService` (`APBChat.h`) should be updated to match retail: add `/tr` for Trade,
  remap `/t` to Team, add Officer/Reply/Yell channels. That is a follow-up increment requiring
  ChatChannel enum changes + tests.
- Chat tag display labels (Broadcast, Combat, Mission, To, Event, Challenge!, GiftBox, Tutorial)
  are needed for the chat UI tab labels and message routing badges.
