# Import Status

Generated: 2026-07-19 21:47 UTC by `tools/scripts/build_import_status.py` (read-only).

## Ledger entries (tools/import_ledger.json)

| asset_key | source | status | dest | note |
|---|---|---|---|---|
| `group:Districts/Asylum` | retail | **imported** | `/Game/Imported/Districts/Asylum` |  |
| `group:Characters/Contacts` | retail | **imported** | `/Game/Imported/Characters/Contacts` |  |
| `group:Characters/Wardrobe` | retail | **imported** | `/Game/Imported/Characters/Wardrobe` |  |
| `group:Vehicles` | retail | **imported** | `/Game/Imported/Vehicles` |  |
| `group:Audio/LoginTheme` | retail | **imported** | `/Game/Audio` |  |
| `group:2011/MenuArt` | 2011 | **extracted** | `/Game/Imported/UI/Menu2011` | umodel 547 spike: WORKS (work/umodel_547_spike.md). 77 pkgs, 2085 PNG at Content/Extracted/2011/MenuArt (APBMenus_Art*, Font, FrontEnd, Skins) via tools/scripts/export_2011_menu_art.py |
| `group:2011/UISfx` | 2011 | **extracted** | `/Game/Audio/UI` | 72 UISounds WAV (Basic_Media+Main_Media) at Content/Extracted/2011/UISfx via tools/scripts/extract_2011_ui_sfx.py (vgmstream decode) |
| `group:2011/Movies` | 2011 | **manual** | `/Game/Movies` | SplashScreen.bik + IntroTitles.bik preserved at Content/Extracted/2011/Movies (Bink 1). ffmpeg unavailable; UE5.8 BinkMedia is Bink2/.bk2-only. Re-encode to .bk2 (Bink2ForUnreal.exe) or mp4 pending M4 — see work/umodel_547_spike.md |
| `data:retail/palettes` | retail | **extracted** | `Content/Data/palettes.json` | 7 palettes, 1134 colors from retail Colours/*.ini via tools/convert/parse_colours.py |
| `data:ui_strings` | 2011+retail | **extracted** | `Content/Data/ui_strings_2011.json + ui_strings_retail.json` | 7 frontend sections each (504 keys 2011 / 588 keys retail) via tools/convert/parse_int_tables.py |
| `group:Districts/Financial` | retail | **imported** | `/Game/Imported/Districts/Financial` | partial — 1 of ~270 block manifests (M9) |
| `group:Districts/Waterfront` | retail | **imported** | `/Game/Imported/Districts/Waterfront` | partial — 1 of ~268 block manifests (M10) |

## Imported uassets on disk (4376 total)

| category | .uasset count |
|---|---|
| Characters | 48 |
| Districts/Asylum | 554 |
| Districts/Beacon | 223 |
| Districts/Crate | 166 |
| Districts/Financial | 2558 |
| Districts/Social | 256 |
| Districts/Waterfront | 511 |
| Vehicles | 60 |

## District placement manifest coverage

Mesh references in each manifest that resolve to an imported .uasset:

| manifest | district | placements | resolvable | coverage |
|---|---|---|---|---|
| Asylum_Block.json | PGAsylum | 417 | 417 | 100.0% |
| Beacon_Block.json | PGBeacon | 430 | 430 | 100.0% |
| Crate_Block.json | PGCrate | 336 | 336 | 100.0% |
| Financial_Block09.json | Financial | 2345 | 2345 | 100.0% |
| Social_Block.json | Social | 466 | 466 | 100.0% |
| Waterfront_Block05.json | Waterfront | 1394 | 1394 | 100.0% |

## District block coverage (vs retail streaming-block counts)

A district is content-complete only when every streaming block has a manifest
whose meshes all resolve. Retail block counts from the 2026-07-19 inventory.

| district | manifests present | retail blocks (approx) | status |
|---|---|---|---|
| financial | 1 | ~270 | ⚠️ partial — 269 blocks to export |
| waterfront | 1 | ~268 | ⚠️ partial — 267 blocks to export |

## Remaining work (auto-derived)

- **financial**: 1 of ~270 block manifests present — export remaining blocks via `tools/scripts/export_apb_level_parallel.py` (see roadmap M9/M10)
- **waterfront**: 1 of ~268 block manifests present — export remaining blocks via `tools/scripts/export_apb_level_parallel.py` (see roadmap M9/M10)

Legend: `extracted` = dumped from source packages, not yet imported · `imported` = .uasset exists · `bound` = referenced by a manifest · `manual` = must be rebuilt by hand.
