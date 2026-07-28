# Retail Map Port — Extraction Evidence

Started 2026-07-28. Engine UE5.8 at `D:\UE58\UE_5.8`.
Source of truth for districts: **RETAIL** install
`C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded`.
The `2011 apb` tree is for the main-menu/frontend work ONLY — not districts.

## Goal
1:1 port of the retail district map into the UE5.8 project: real geometry, materials,
actor placements (location/rotation/scale), streaming layout.

## Blocking discovery — shipped placement data is fabricated
`tools/scripts/extract_actor_transforms.py` docstring states verbatim that it replaces
a pipeline where *"rotations were synthesized as `(i*11)%360`, scale hardcoded `[1,1,1]`,
and mesh identity assigned round-robin (`imported[i % len]`)"*.

That fabricated output is what still ships in
`Content/Data/district_placements/Financial_Block09.json` (2345 placements):

| Check | Result |
|---|---|
| building-layer yaw, file order | `66, 77, 88, 99, 110, 121, 132, 143, 154, 165, 176, 187` — an `(i*11)` ramp |
| non-zero pitch | 0 of 2345 |
| non-zero roll | 0 of 2345 |
| non-unit scale | 0 of 2345 |
| `Financial_Block09_real.json` | `actor_count=46`, `unresolved_actor_count=461` |

`work/IMPORT_STATUS.md` "100% coverage" measures only mesh_id→uasset **bind** hit rate.
It says nothing about placement fidelity. `hit_rate=1.000` is meaningless for 1:1.

## Retail scope (actual)
`.../APBGame/Content/Release/Maps/` — 701 `.APB` packages total.

| District | packages |
|---|---|
| FinancialDistrict | 269 |
| WaterfrontDistrict | 268 |
| AsylumDistrict | 44 |
| PGCrateDistrict | 40 |
| PGBeaconDistrict | 30 |
| UIDistrict | 15 |
| RWorldSocialDistrict | 14 |

Financial spans **Block01–Block44** plus `_Props_`, `_ArtProps_`, `_Design`, `_TILE_*`
(ROADS/TERRAIN), `_Pathgraph`, `_MissionSpawnZones`, `_HubSpawns`, volumes and holiday
variants. The shipped manifest merged only 25 packages and claimed the whole district.

## Why the old extractor could not read retail
1. Unguarded buffer read in `scan_prop` → `struct.error` crash on retail Block09. Patched.
2. `MESH_ACTOR_CLASSES` is 2011-era: `StaticMeshActor`, `cStreamedLightingStaticMeshActor`,
   `cProp`, `cStreamedBuildingActor`. Retail Block09 contains **zero** `StaticMeshActor`
   and **zero** `cProp`, and **239 `PrefabInstance`** that it ignores entirely.
3. `scan_prop` brute-force scans bytes for a name index and takes the first plausible
   hit instead of walking the chain — the root cause of 46/507 resolution.
4. Its `__main__` default `maps_dir` pointed at the **2011** build, not retail.
5. Flat-image cache keyed by package stem only, so identically named 2011 and retail
   packages silently overwrote each other.

## Toolchain
- `tools\_incoming\umodel_apb\umodel_64.exe` — SHA-256 `3B8296DF0F2F5625…`
- `umodel_apb.rar` (repo root) — **identical** to the above; confirms the APB build,
  adds no new capability.
- `tools\UEViewer\umodel_64.exe` — `AFF4FCBFE40646FD…`, *different*, and it is the one
  the pipeline actually calls. Both give identical `-list` output (3400 exports).
- `tools\UEViewer\Tools\PackageUnpack\decompress.exe -game=apb` — LZO → flat image.

Retail Block09 header: `Ver: 564/33  Engine: 3908  Names: 815  Exports: 3400
Imports: 762  Game: 80001D`.

## New extractor — `tools/scripts/apb_level_dump.py`
Correct sequential UE3 tagged-property walker.

Two bugs found and fixed during bring-up:
- `walk_best` walked the full chain from **every** 4-byte offset → O(n²), hung the
  3400-export pass. Replaced with anchor detection: locate the 4-byte encoding of a
  property-type FName index via `bytes.find` (C speed), walk only those candidates.
- The anchor scan filtered to 4-byte alignment, but UE3 chains are **byte-packed**.
  Dropping that filter recovered the full chains (`PrefabInstance` 6 → 17 properties).

Chain start is per-class and unaligned: components at `off=8`, actors at `off=26`.

## Verified retail extraction — FinancialDistrict_Block09
**2455 / 3400 exports parsed in 1.8 s.** The 945 misses are `ShadowMap1D`,
`LightMapTexture2D`, `Model`, `Polys`, `cBulkDataCollection` — bulk binary with no
property chain, correctly skipped.

```
cStreamedBuildingActor_0  Location [179597.375, 129688.578, 837.758]
                          m_ComponentSet 167  StaticMeshComponent 1190
                          m_StreamingPriPos [179590.828, 129692.273, 830.725]
PrefabInstance_0          Location [177543.25, 116979.852, -167.381]
                          Rotation [0, -24328, 0]   (URU; 65536 == 360 deg)
                          TemplatePrefab -238  PI_Bytes count=519
                          m_ActiveFeatureBuildingName
                            financialdistrict_block09.TheWorld:PersistentLevel.cStreamedBuildingActor_31
StaticMeshComponent_885   StaticMesh -591   Scale3D [0.2, 4.0, 2.0]
```

`Scale3D [0.2, 4.0, 2.0]` is genuine non-uniform scale — precisely what the fabricated
manifest could never contain.

| class | ok/total | key coverage |
|---|---|---|
| PrefabInstance | 239/239 | Location 239 · Rotation 227 · TemplatePrefab 239 · PI_Bytes 239 · m_ActiveFeatureBuildingName 239 |
| cStreamedBuildingActor | 137/137 | Location 137 · m_ComponentSet 137 |
| StaticMeshComponent | 392/392 | StaticMesh 313 · explicit Scale3D 25 |
| PointLightComponent | 460/460 | — |
| PointNightLight | 173/173 | Location 173 · Rotation 161 |
| PointLight | 57/57 | Location 57 · Rotation 57 |
| SpotLight / SpotNightLight | 26/26 · 28/28 | Location+Rotation full |
| cGraffitiCrimeTarget | 23/23 | Location 23 · Rotation 22 |
| cStreamedComponentSet | 137/137 | m_aComponents **137** (after the ranking fix below) |

## Chain-ranking fix — earliest, not longest
`walk_best` originally kept the **longest** `None`-terminated chain. That is wrong. The
export header always *precedes* the property chain, so any later anchor necessarily lands
inside the real chain's value bytes and yields a plausible-looking but bogus sub-chain.

`cStreamedComponentSet_10` is the clean demonstration:

```
off=8   n=2   m_aComponents, m_ReferencedMaterials        <- true chain
off=36  n=3   m_sName, m_bIsPermanant, m_pPermanant       <- bogus, but longer -> won
```

Three ranking rules were measured across all 3400 exports rather than reasoned about:

| class | longest | earliest | widest |
|---|---|---|---|
| cStreamedComponentSet `m_aComponents` | 12/137 | **137/137** | 12/137 |
| cStreamedBuildingActor | 137 | 137 | 137 |
| PrefabInstance | 239 | 239 | 239 |
| StaticMeshComponent | 313 | 313 | 313 |
| PointNightLight / PointLight | identical | identical | identical |

`earliest` strictly dominates: it fixes the one broken class and is bit-identical on
every other. Now `return` on the first `None`-terminated chain in ascending offset order.

Post-fix full pass: **2455/3400 in 2.0 s**, `m_aComponents` **137/137**, and the three
golden exports unchanged (`cStreamedBuildingActor` off=26 n=11, `PrefabInstance` off=26
n=17, `StaticMeshComponent` off=8 n=9 `Scale3D [0.2, 4.0, 2.0]`).

## PI_Bytes is decodable — real transforms inside
The 519-byte `PI_Bytes` payload of `PrefabInstance_0` contains `FFFFFFFF <size>` record
framing, 16-byte GUIDs, and an **orthonormal 3x3 basis plus translation**:

```
-0.79855  -0.60179  -0.01227      row0 . row0 = 0.99998
-0.06369   0.06421   0.99590      -> genuine rotation basis, not noise
-0.59854   0.79606  -0.08961
```

So prefab-hosted geometry placements can be recovered. This matters because
`PrefabInstance` (239) outnumbers `cStreamedBuildingActor` (137) in Block09 — prefabs are
the dominant geometry carrier in retail and the old extractor ignored them completely.

## Open items
1. Decode `PI_Bytes` fully (embedded prefab actor set) — required for true 1:1.
3. Resolve `StaticMesh` import indices to names, cross-package.
4. `PointLight` / `StaticLightCollectionActor` need per-class handling.
5. Extend from one block to all 44 Financial blocks, then the other districts.
