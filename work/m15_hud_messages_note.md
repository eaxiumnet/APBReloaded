# m15 — HUD-message catalog (HUDMessages.INT) — handoff note

**Author:** Qoder  **Status:** COMPLETE (all 17 domain suites FAILS=0)

## What landed
Extracted the retail APB HUD-message catalog (the broad on-screen HUD notifications / prompts / error
banners: mission events, item/vehicle deliveries, contact-standing prompts, "You cannot abandon
opposed missions.", arrest/bounty banners, ...) into the Domain layer, following the established
data-catalog recipe. This is the **HUD banner system**, distinct from the combat score-feed already
ported as `hud_combat_messages` (HUDCombatMessages.INT).

| Artifact | Path |
|---|---|
| Extractor (durable) | `tools/scripts/extract_hud_messages.ps1` |
| Data | `Content/Data/hud_messages.json` (882 rows) |
| Domain catalog | `Source/APBReloaded/Domain/APBHUDMessages.h` (header-only `HUDMessageCatalog`) |
| WorldService wiring | `APBWorldService.h`/`.cpp` (`hud_messages` member, load, `hud_messages=` INIT token) |
| Test | `tests/run_domain_tests.cpp` -> `TestHUDMessagesFromRetail` |

## Source & provenance
- Source: `<retail>\APBGame\Localization\INT\HUDMessages.INT` (UTF-16LE) — mirror of the cooked SDD
  table `HUDMessage`. Single `[HUDMessages]` section, 1802 keys.
- Two keys per message id, grouped by the extractor:
  - `HUDMessages_<id>_DisplayText=<on-screen banner text>`
  - `HUDMessages_<id>_ChatText=<system-chat-log version>` (usually empty)
- 901 distinct ids; rows where both fields are empty (e.g. `HUDMessages_None`) dropped -> **882 real
  rows** (867 display texts, 168 chat texts).
- Text/markup kept **VERBATIM** (no stray `\u` — \uXXXX-restore). Two markup families are preserved:
  - `<col:NAME>...</col>` — named colour spans (240 rows); `NAME` (e.g. `HUDMessage_Error`) resolves
    against the HUD colour palette at render time.
  - `<CharacterNameA>` / `<VehicleName>` / `<ContactName>` / `<Amount>` / ... — runtime substitution
    tokens (278 rows).
- The manual line-break glyph U+21B5 is converted to a real `\n` (the banner renders multi-line; 48
  rows); other C0 control chars are stripped.

## Shape & the helpers
- 882 rows. `HUDMessage{ id, display_text, chat_text, order }`.
- `HUDMessageCatalog` API: `Find / DisplayText / ChatText / PlainDisplayText / FormatDisplay / Count`,
  merge-by-id, order-sorted.
- **`static StripColor(text)`** removes the `<col:...>`/`</col>` wrappers (case-insensitive) while
  keeping the inner text and every other `<Token>`. `PlainDisplayText(id)` applies it to a message.
- **`static Format(text, token, value)`** replaces every literal `<token>` with `value`;
  `FormatDisplay(id, token, value)` does it on a message. Multi-token banners are filled by repeated
  `Format` calls (verified end-to-end: `<CharacterNameA> delivered the <VehicleName>`).

## Notes for other agents
- Drive the **UMG HUD banner queue** from `WorldService.hud_messages`: look up the message by id,
  `FormatDisplay`/`Format` in the runtime values, then either render the `<col:NAME>` spans (map
  `NAME` -> palette colour) or `PlainDisplayText` for a plain banner. Do not hardcode banner strings.
- Remaining reconciliation (not blocking): build a `HUDMessage_*` colour-name -> RGBA table when the
  HUD palette is wired (the `<col:NAME>` names are a small enumerable set). The catalog is ready for
  that colour map to sit on top; `StripColor` already lets callers fall back to uncoloured text.
- `chat_text` (168 rows) is the system-chat-log variant of a banner — feed it to the chat panel when
  that path is built.
