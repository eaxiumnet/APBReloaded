# District Content Coverage — Metric Correction & Real Remaining Work

Author: GPT-5.8 agent · 2026-07-20
Status: investigation complete; status tooling corrected; no heavy export launched.

## TL;DR

The old `IMPORT_STATUS.md` claimed **"financial: 1 of ~270 blocks, 269 to export"**
and **"waterfront: 1 of ~268, 267 to export"**. **That was a false metric** and should
not be used to schedule work. It compared *manifest-file-count* (1 JSON file) against a
*raw retail `.upk` package count* (270) — apples to oranges. The single MASTER-derived
manifest already covers the district's full streaming set.

Fixed this turn:
- `tools/scripts/build_import_status.py` — now reports real streaming coverage
  (streamed chunks, source packages, placement resolvability) per district and labels
  the raw `.upk` count "ref only". Regenerated `work/IMPORT_STATUS.md` (gaps=0).
- `tools/import_ledger.json` — Financial/Waterfront notes corrected to real numbers.

## Evidence

A district's playable geometry = the `LevelStreamingKismet` entries baked into its
`*_MASTER.APB` map, decoded by `tools/scripts/decode_level_streaming.py`. `decode_master()`
extracts **every** LevelStreamingKismet row (no artificial cap), so the decoded chunk
count is the actual streaming-block set — not a sample.

Measured directly from the committed manifests (`Content/Data/district_placements/`):

| manifest | district | placements | resolvable | streamed chunks | src pkgs |
|---|---|---|---|---|---|
| Asylum_Block.json | PGAsylum | 417 | 100% | 5 | 3 |
| Beacon_Block.json | PGBeacon | 430 | 100% | 9 | 2 |
| Crate_Block.json | PGCrate | 336 | 100% | 47 | 2 |
| Financial_Block09.json | Financial | 2345 | 100% | 57 | 25 |
| Social_Block.json | Social | 466 | 100% | 17 | 4 |
| Waterfront_Block05.json | Waterfront | 1394 | 100% | 30 | 18 |

All six base districts decode at **100% mesh-ref resolvability**. The raw retail `.upk`
count (270 for Financial) includes LOD variants, ArtProps sub-blocks, minimaps and other
non-streamed packages — it is NOT the streaming set and NOT an export target.

## Real remaining district work (correctly scoped)

The dashboard only measures manifests that exist, so it shows "Remaining work: None".
That is accurate *for the manifests present*, but the following genuine gaps remain and
are invisible to the auto-derived list:

1. **District variants have no manifest yet.** Retail ships MASTER maps for action-district
   variants that are separate entries in `Content/Data/districts.json`:
   - `FinancialChaosDistrict_MASTER.APB`   → districts.json `FinancialChaos` (numeric_id 2)
   - `FinancialRiotDistrict_MASTER.APB`    → districts.json `FinancialRiot`  (numeric_id 12)
   - `WaterfrontChaosDistrict_MASTER.APB`  → (variant)
   - Also present: `*ChristmasDistrict_MASTER`, `*EpidemicDistrict_MASTER` (seasonal/event).
   These likely reuse the base district geometry with different mission/spawn layers, so a
   manifest may be derivable by diffing the variant MASTER against the base rather than a
   full re-export. Confirm before treating as new content.

2. **Base-district completeness is decoded, not visually verified.** 100% mesh-ref
   resolvability proves every referenced mesh is imported; it does not prove the decoded
   chunk set matches the retail world 1:1 in-editor. A visual pass (spawn the freeroam map,
   compare skyline/landmarks vs retail) is the real acceptance gate for M9/M10.

## Pipeline reproducibility gap (READ before re-running decode)

`decode_level_streaming.py` reads **already-decompressed** MASTER maps from:

    UNPACKED = %LocalAppData%\Temp\grok-goal-9ca60165ac93\implementer\unpacked_maps

That temp dir is **gone** (`unpacked_maps` no longer exists). The retail `*_MASTER.APB`
files on disk are compressed (~0 MB cooked, APB FileVersion 564/33) and are NOT in
`.../Release/Packages` — stock umodel often fails to decompress them
(see `tools/scripts/extract_with_umodel.ps1` and `parse_ue3_streaming.py:3`).

To reproduce/extend decode you must FIRST re-run the PackageUnpack/decompress step that
populated `unpacked_maps` (UPKUtils is staged at
`work/_archive/login_swap/tools/UPKUtils`), then point `decode_level_streaming.py`'s
`UNPACKED` at the new output and add the target MASTER(s) to its hardcoded `main()` list.
Do NOT use `export_apb_level_parallel.py` for this — that tool dumps 2011 meshes/textures
via umodel; it does not produce retail placement manifests.

## Recommendation for the content-pipeline agent

- Do not chase the phantom "269 blocks". Base Financial/Waterfront streaming sets are
  present and 100% resolvable.
- If pursuing variants (item 1): first decompress the variant MASTER, decode it, and diff
  its LevelStreaming table against the base manifest — expect mostly-shared geometry.
- Gate M9/M10 "complete" on a visual in-editor comparison, not on manifest counts.
