# Import Status

Generated: 2026-07-20 19:15 UTC by `tools/scripts/build_import_status.py` (read-only).

## Ledger entries (tools/import_ledger.json)

| asset_key | source | status | dest | note |
|---|---|---|---|---|
| `group:Districts/Asylum` | retail | **imported** | `/Game/Imported/Districts/Asylum` |  |
| `group:Characters/Contacts` | retail | **imported** | `/Game/Imported/Characters/Contacts` |  |
| `group:Characters/Wardrobe` | retail | **imported** | `/Game/Imported/Characters/Wardrobe` |  |
| `group:Vehicles` | retail | **imported** | `/Game/Imported/Vehicles` |  |
| `group:Audio/LoginTheme` | retail | **imported** | `/Game/Audio` |  |
| `group:2011/MenuArt` | 2011 | **imported** | `/Game/Imported/UI/Menu2011` | umodel 547 spike: WORKS (work/umodel_547_spike.md). 77 pkgs, 2085 PNG extracted at Content/Extracted/2011/MenuArt. M4a: 123 spec-referenced textures imported (Login 14, Chrome 52, CharSelect 20, DistrictSelect 12, Loading 19, Reference 6) via tools/scripts/import_menu2011_assets.py — chrome sourced from the fuller LiveCurrentScene APBMenus_Art dump (222 PNGs; MenuArt tree has only 8 for that package). Spec: work/menu2011_spec.md |
| `group:2011/UISfx` | 2011 | **imported** | `/Game/Audio/UI` | 72 UISounds WAV (Basic_Media+Main_Media) extracted at Content/Extracted/2011/UISfx. M4a: 12 menu-interaction WAVs imported to /Game/Audio/UI (hover/click/back/error/popup/scene-open/list-select/char-confirm/slider/loading) — mapping table in work/menu2011_spec.md §7 |
| `group:2011/Movies` | 2011 | **manual** | `/Game/Movies` | SplashScreen.bik + IntroTitles.bik preserved at Content/Extracted/2011/Movies (Bink 1). ffmpeg unavailable; UE5.8 BinkMedia is Bink2/.bk2-only. Re-encode to .bk2 (Bink2ForUnreal.exe) or mp4 pending M4 — see work/umodel_547_spike.md |
| `data:retail/palettes` | retail | **extracted** | `Content/Data/palettes.json` | 7 palettes, 1134 colors from retail Colours/*.ini via tools/convert/parse_colours.py |
| `data:ui_strings` | 2011+retail | **extracted** | `Content/Data/ui_strings_2011.json + ui_strings_retail.json` | 7 frontend sections each (504 keys 2011 / 588 keys retail) via tools/convert/parse_int_tables.py |
| `group:Districts/Financial` | retail | **imported** | `/Game/Imported/Districts/Financial` | MASTER LevelStreaming decoded: 57 chunks / 2345 placements, 100% resolvable (Financial_Block09.json). Streaming set complete; verify variants (Chaos/Riot) separately — see work/district_coverage_note.md |
| `group:Districts/Waterfront` | retail | **imported** | `/Game/Imported/Districts/Waterfront` | MASTER LevelStreaming decoded: 30 chunks / 1394 placements, 100% resolvable (Waterfront_Block05.json). Streaming set complete — see work/district_coverage_note.md |

## Imported uassets on disk (4499 total)

| category | .uasset count |
|---|---|
| Characters | 48 |
| Districts/Asylum | 554 |
| Districts/Beacon | 223 |
| Districts/Crate | 166 |
| Districts/Financial | 2558 |
| Districts/Social | 256 |
| Districts/Waterfront | 511 |
| UI | 123 |
| Vehicles | 60 |

## District placement manifest coverage

Each manifest is decoded from its district's `*_MASTER.APB` LevelStreamingKismet
table (the authoritative streaming-block set). `resolvable` = mesh refs that map
to an imported `.uasset`; `chunks` = distinct streamed packages placed in-world.

| manifest | district | placements | resolvable | coverage | chunks | pkgs |
|---|---|---|---|---|---|---|
| Asylum_Block.json | PGAsylum | 417 | 417 | 100.0% | 5 | 3 |
| Beacon_Block.json | PGBeacon | 430 | 430 | 100.0% | 9 | 2 |
| Crate_Block.json | PGCrate | 336 | 336 | 100.0% | 47 | 2 |
| Financial_Block09.json | Financial | 2345 | 2345 | 100.0% | 57 | 25 |
| Social_Block.json | Social | 466 | 466 | 100.0% | 17 | 4 |
| Waterfront_Block05.json | Waterfront | 1394 | 1394 | 100.0% | 30 | 18 |

## District streaming coverage

A district's geometry is complete when every LevelStreamingKismet entry in its
`*_MASTER.APB` has a decoded chunk whose meshes resolve. The `RETAIL_UPK_BLOCKS`
figures below are the raw retail `.upk` package count (LODs + ArtProps + minimaps
included) and are shown ONLY to contrast against the true streamed-chunk count —
they are **not** an export target. See `decode_level_streaming.py`.

| district | manifests | streamed chunks | retail .upk (ref only) |
|---|---|---|---|
| financial | 1 | 57 | ~270 |
| pgasylum | 1 | 5 | n/a |
| pgbeacon | 1 | 9 | n/a |
| pgcrate | 1 | 47 | n/a |
| social | 1 | 17 | n/a |
| waterfront | 1 | 30 | ~268 |

## Remaining work (auto-derived)

- None — all manifests resolve and block coverage is complete.

Legend: `extracted` = dumped from source packages, not yet imported · `imported` = .uasset exists · `bound` = referenced by a manifest · `manual` = must be rebuilt by hand.
