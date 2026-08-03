# M15: MaterialDatabase Texture Pipeline — Registry Routing

**Status:** complete
**Date:** 2026-08-03
**Baseline commit (prior task):** `4e59b1d` (M19 district mesh routing)
**This task:** Route MaterialDatabase material + texture uassets through
`UAPBVerifiedAssetRegistry` so district material bindings enter the verified allowlist.

## Scope

The 865 `linked_hidden_base_mesh` district placements resolve via material bindings,
not mesh imports. Their material slots reference the retail MaterialDatabase (2,343
materials, 479 upks). Those material/texture uassets were on disk but had no verified
ledger rows — only 475 evidence-less package markers + 1 verified row existed.

## What was built

`tools/scripts/record_material_texture_import.py` — records verified-eligible ledger
rows for every MaterialDatabase uasset with a full D17 evidence chain:

- Source: retail upk (`${retail_steam}/.../Packages/MaterialDatabase/<sub>/<pkg>.upk`),
  sha256 computed from the retail package on disk.
- Intermediate: the extracted TGA texture (role-priority Diffuse > Normal > Emissive >
  Opacity > Specular > Cube) with sha256.
- Evidence: `work/evidence/material_texture_batch/<pkg>.json` per package
  (schema `apb_material_database_v1`), one file per upk, listing every recorded object.
- Dest: `/Game/Imported/MaterialDatabase/<pkg>/<obj>.<obj>` with uasset sha256.
- Class mapping: `MI_*` → `MaterialInstanceConstant`, `M_*` → `Material`,
  else → `Texture2D` (matched to a same-stem TGA in the extracted tree).

## Results

| Metric | Before | After |
|---|---|---|
| Ledger entries | 4,904 | 9,015 |
| Verified rows | 4,315 | 8,426 |
| Allowlist entries | 4,281 | 8,392 |
| Duplicate object_paths | 0 | 0 |
| MaterialDatabase verified rows | 1 | 4,097 |

Coverage: 2,342/2,343 materials + 1,755 texture uassets + 15 `M_`-prefixed texture
aimports (hair opacity/normal maps) recorded. Skipped (no TGA source or no upk): 1
(`ESD_StaticMesh_01/ESD_Bar_Normal_` — trailing-underscore stem mismatch).
Rejected at promotion: 589 pre-existing evidence-less rows (475 package markers +
6 blocked_source + 108 group/other markers) — unchanged.

## Gate results

- `STRICT_ASSET_PROVENANCE_PASS entries=9015 verified=8426 blocked=6`
- `VERIFIED_ASSET_ALLOWLIST_PASS entries=8392`
- Runtime probe: `RUNTIME_ALLOWLIST_ALLOW_OK` (8,392-entry manifest loads, no dups,
  no SHA assert)
- Static cook audit: unverified refs still 1,178 — residual is the documented
  3-class set (865 `linked_hidden_base_mesh` no-import placements, 300
  reference-catalog rows, 14 oracle keys). No `/Game/Imported/MaterialDatabase/`
  refs exist in `Content/Data` JSONs, so the allowlist closes them proactively.

## Files changed

- `tools/scripts/record_material_texture_import.py` (new)
- `tools/import_ledger.json`
- `Content/Data/verified_asset_allowlist.json`
- `work/evidence/material_texture_batch/*.json` (464 package evidence files)
- `work/m15_material_texture_registry_routing.md` (this note)

## Follow-ups

- 1 skipped uasset (`ESD_Bar_Normal_`) has a trailing-underscore stem with no TGA
  match — verify against retail texture extraction naming.
- The 475 package markers + 108 group rows stay rejected until evidence is added.
