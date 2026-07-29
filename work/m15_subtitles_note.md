# M15 — Voice-Line Subtitles Catalog (orphan closure)

**Date:** 2026-07-22
**Status:** Complete — all 17 domain test suites FAILS=0
**Scope:** Closed the orphaned subtitles pipeline: extractor + JSON already existed
(`tools/scripts/extract_subtitles.ps1` → `Content/Data/subtitles.json`), but the Domain catalog
header, WorldService wiring, test, note, and `_active.md` bullet were all missing.

## Source

- **Retail file:** `Subtitles_MASC.int` — the UTF-16LE `.int` mirror of the cooked SDD subtitle
  table (the on-screen captions spoken by NPCs / contacts / enforcers / criminals during missions,
  greetings, taunts, radio chatter, etc.).
- **Extractor:** `tools/scripts/extract_subtitles.ps1` (pre-existing) → `Content/Data/subtitles.json`.
- **Masc vs fem:** `Subtitles_MASC.int` and `Subtitles_FEM.int` are **byte-identical** in this build
  (SHA256-verified), so the male/female split is a non-feature here. One flat id→text catalog is ported
  from the MASC file. If a future build ever diverges, add a `fem` text column.

## Data shape

`Content/Data/subtitles.json` is a flat array of `{id, text, order}`:

```json
{"id":"CHA_Greeting_Known_1","text":"Ok, so you're here. What's up?","order":0}
{"id":"WLD_Greeting_1","text":"Looking for a new gun? I think I've got you covered.","order":121}
{"id":"ORL_Dispatch_Bounty_1","text":"There's some no-name lowlifes riding round Tigers turf. Go fuck 'em up.","order":7007}
```

- **8864** kv lines in the source; **20** empty-value rows dropped by the keep-if-non-empty rule →
  **8844** rows in the JSON.
- **1 duplicate id** (`ORL_Dispatch_Bounty_1`) appears twice with *different* text:
  - order 7006: "There's some no-name **scumbags** riding round Tigers turf. Go fuck 'em up."
  - order 7007: "There's some no-name **lowlifes** riding round Tigers turf. Go fuck 'em up."
  - Merge-by-id keeps the **last** (order 7007, "lowlifes"), so the catalog holds **8843** distinct ids.
- No `None` DNT placeholder exists in this table (unlike some other catalogs).

## Category prefixes (first `_`-separated token of the id)

| Prefix | Meaning                              | Count (approx) |
|--------|--------------------------------------|----------------|
| CHA    | character greetings                  | 121            |
| WLD    | world / vendor greetings             | 27             |
| PRF/PRM| prestige enforcer / criminal         | …              |
| PTF/PTM| notoriety enforcer / criminal        | …              |
| GKF/GKM| gang kill enforcer / criminal        | …              |
| GRI    | gang radio                            | …              |
| BRF/BRM| bounty enforcer / criminal           | …              |
| SUJ    | subject                              | …              |
| VER    | vendor                                | …              |
| TER    | territory                            | …              |
| ORL    | oral dispatch (bounty radio)         | …              |
| …      | (other mission/contact categories)   | …              |

`SubtitleCatalog::Category(id)` returns the first `_`-separated segment; `ForCategory(cat)` returns
all captions in that category in display order.

## Domain catalog

**File:** `Source/APBReloaded/Domain/APBSubtitles.h` (header-only, matches the `LoadingTipCatalog` /
`TooltipCatalog` recipe — every method defined in-class, implicitly inline).

```cpp
namespace apb {
struct SubtitleEntry { std::string id; std::string text; int32_t order = 0; };

class SubtitleCatalog {
public:
    std::vector<SubtitleEntry> items; // sorted by order
    bool LoadFromJsonFile(const std::string& path);
    bool LoadFromJsonText(const std::string& text); // additive/merge-by-id (last wins)
    const SubtitleEntry* Find(const std::string& id) const;
    std::string Text(const std::string& id, const std::string& def = {}) const;
    std::vector<const SubtitleEntry*> ForCategory(const std::string& category) const;
    int32_t Count() const;
    static std::string Category(const std::string& id); // first '_'-token
private:
    // SplitTopObjects, RawStr, RNum, Unescape, AppendUtf8 — JSON helpers
};
}
```

- **Merge-by-id:** existing ids are updated in place; new ids appended. Never clears on empty input.
- **Order-sorted:** after load, `items` is sorted by `order` (stable display/file order).
- **JSON parser:** depth-aware `{...}` splitter + string-aware `RawStr` (honours `\"` escapes) +
  full `Unescape` (`\" \\ \/ \b \f \n \r \t` and `\uXXXX`→UTF-8).

## WorldService wiring

**`APBWorldService.h`:**
- `#include "APBSubtitles.h"` (after `APBLoadingTips.h`).
- `SubtitleCatalog subtitles;` member (after `loading_tips`).

**`APBWorldService.cpp` (`InitFromDataDir`):**
- Load: `const bool subtitlesOk = subtitles.LoadFromJsonFile(dir + "/subtitles.json");`
  (after the `loading_tips` load line).
- INIT log token: `+ " subtitles=" + std::to_string(subtitles.Count()) +
  (subtitlesOk ? "" : " (subtitles_missing)")` (after the `loading_tips` token).

## Test

**`tests/run_domain_tests.cpp` — `TestSubtitlesFromRetail`** (registered in `main()` after
`TestLoadingTipsFromRetail`):

- `subtitles.json parses` — file loads.
- `loaded all 8843 distinct voice-line subtitles (1 duplicate id merged)` — count.
- `greeting caption verbatim` — `CHA_Greeting_Known_1` → "Ok, so you're here. What's up?".
- `world/vendor caption begins verbatim` — `WLD_Greeting_1` starts with "Looking for a new gun?".
- `no literal \u in caption` — no unescaped `\u` leakage.
- `duplicate id merged to last (lowlifes)` — `ORL_Dispatch_Bounty_1` → contains "lowlifes".
- `duplicate id first value dropped (scumbags)` — does NOT contain "scumbags".
- `Category is the first token` — `Category("CHA_Greeting_Known_1") == "CHA"`.
- `ForCategory returns the CHA captions` — non-empty.
- `121 CHA greeting captions grouped` — exact CHA count.
- `items sorted by file order` — `items[0].order == 0`.
- `no None DNT placeholder in this table` — `Find("None") == nullptr`.
- `missing caption Find is null` / `missing caption returns default text` — safety.
- `subtitles world init` / `world loaded subtitles` / `world subtitle resolves caption` — end-to-end.

## Verification

```
powershell -File D:\APBReloaded\tests\build_and_run.ps1
```

Result: **17/17 test binaries FAILS=0**, stderr empty. The subtitles INIT log line reads
`subtitles=8843` (no `(subtitles_missing)` suffix).

## Open / next

- The subtitle *playback* path (tying a caption id to an audio cue + on-screen display widget) is not
  yet wired — this catalog only provides the id→text lookup. A future task will connect it to the
  audio/Wwise system and the UMG subtitle widget.
- If a future build's `Subtitles_FEM.int` diverges from `Subtitles_MASC.int`, add a `fem` text column
  to `SubtitleEntry` and a second load pass.

