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

So prefab-hosted geometry placements can be recovered, and the old extractor ignored them
completely.

### Correction (2026-07-28): prefabs are NOT proven to be the dominant carrier
An earlier revision of this file claimed prefabs dominate because `PrefabInstance` (239)
outnumbers `cStreamedBuildingActor` (137). **That inference is invalid.** It compares
*root actors*, not rendered mesh instances. The 137 component sets already reference 392
`StaticMeshComponent`s, so one building emits several meshes; a prefab may emit zero, one,
or many. The relative share is unknown until `PI_Bytes` is decoded and cannot be inferred
from root counts. `PI_Bytes` is therefore a **completeness gate, not the critical path**.

## Five confirmed fail-open blockers (verified by reading code, 2026-07-28)
Each was read directly, not inferred. All five let a partial or wrong result report success.

1. **Dedup key drops rotation and scale.**
   `APBDistrictPlacementLoader.cpp:346` builds `Key = E.MeshId + "@" + E.Location.ToString()`.
   Co-located instances differing only in rotation/scale collapse into one. Writing real
   rotations would *silently delete* real instances — and the skip is counted as success.

2. **`LoadPlacementMesh` substitutes assets across districts.**
   `APBDistrictPlacementLoader.cpp:241-267`: on a failed path it tries 6 district folders x
   5 stem variants and returns the first hit. A Financial mesh can resolve to a Waterfront
   asset. This defeats visual verification — the city looks plausible but is wrong.

3. **Import outer index is discarded.**
   `apb_level_dump.py:132` calls bare `r.i32()` for the import outer, so package-qualified
   resolution of `StaticMesh` (a negative import index) is impossible.

4. **Only the first component of a set is emitted, and the wrong set is walked.**
   `extract_actor_transforms.py:275-283` `return`s inside the `m_aComponents` loop on the
   first mesh-bearing member. Worse, `m_aComponents` is not the geometry list at all — see
   the corrected chain model below.

5. **Text-scanning Domain mirror never reads rotation.**
   `Domain/APBDistrictPlacement.h:182` scans for `"mesh_id"` instead of walking JSON and
   extracts no rotation. It actively certifies fabricated manifests as valid.

## Revised critical path
Prove the **non-prefab slice end-to-end** (extract -> import-resolve -> manifest -> UE
spawn -> visual) with per-row provenance first; decode `PI_Bytes` after. A `PI_Bytes` dead
end then leaves a *measured incomplete* port instead of invalidating the whole pipeline.

## Fidelity gates (replacing the weak ones)
Rejected as primary gates: "rotation is not an arithmetic ramp" catches only the one known
fraud; "non-unit scale count > 0" passes with 1 real row among thousands of fabricated ones;
per-class export count cannot equal placement count (actors/components/resources differ in
cardinality).

Adopted:
- **Shuffle invariance** — shuffle source-record order and imported-asset enumeration,
  regenerate, assert every `source_id` keeps identical mesh + transform. This is the
  *general* detector: it catches `(i*11)%360`, `imported[i % len]`, and any index-derived
  fabrication automatically.
- **Conservation** — per source class, `candidates = emitted + unresolved + excluded`, with
  machine-readable reason codes and zero unclassified exports.
- **Keyed lineage** — every row carries `source_id` = (package sha256, export index,
  component index), plus the mesh object-reference chain.
- **Missingness parity** — record `scale_present` separately from the value, so "absent
  therefore 1.0" stays distinguishable from a fabricated 1.0.
- **Fail-closed binding + runtime parity** — no fallback assets, no location-only dedup;
  UE-spawned asset path and world matrix must equal the manifest, keyed by `source_id`.

## Corrected chain model (measured on Block09, 2026-07-28)
Three assumptions carried by the old pipeline are false. All figures below come from
`tools/scripts/test_apb_level_dump.py` (29/29 green) plus two throwaway diagnostics.

| Claim | Measured reality |
|---|---|
| Geometry hangs off `m_ComponentSet.m_aComponents` | **False.** All 137 sets hold exactly `[FeatureGroupComponent, null, PointLightComponent]` — 411 members, **zero meshes**. |
| Buildings carry a `Rotation` | **False.** 0/137 `cStreamedBuildingActor` have `Rotation`. Only `PrefabInstance` (227), `PointNightLight` (161), `PointLight` (57), `SpotLight` (26), `SpotNightLight` (25), `cGraffitiCrimeTarget` (22), `cPlayerGraffitiDisplayPoint` (2) do. |
| Buildings carry a per-instance `Scale3D` | **False.** 0/137 building-owned components have `Scale3D`. The 25 `Scale3D` components are elsewhere (e.g. `StaticMeshComponent_885` = `[0.2,4.0,2.0]`, mesh `GenericAssets.1mCube`). |

The real geometry edge is the actor's **direct `StaticMeshComponent` property**: 137/137
resolve to a `StaticMeshComponent` export. `CollisionComponent` is a second edge, emitted
with `reason=collision_only` — a real porting obligation, but not a rendered placement.

**Export ref base is `ref - 1`** (umodel `-list` index is 0-based, UE3 refs are 1-based):
exact class match 137/137 on both edges, off-by-one on every edge under base `ref`. A
single proven base is used deliberately; trying both and accepting whichever matches would
bind the wrong object whenever the expected class repeats.

Consequence for the shipped manifests: the per-instance yaw ramp `(i*11)%360` did not merely
compute a wrong rotation, it **invented a field absent from the source**, and the hardcoded
`[1,1,1]` scale coincidentally matches the UE3 default while carrying no source evidence.
Hence `rotation_present` / `scale_present` are recorded separately from the values.

## Open items
1. Close the remaining fail-open blockers (1, 2, 5 — dedup key, cross-district asset
   substitution, Domain text-scan mirror). Blockers 3 and 4 are closed and gated.
2. Non-prefab Block09 micro-slice proven end-to-end with provenance.
3. Decode `PI_Bytes` fully (embedded prefab actor set) — required for true 1:1.
4. `PointLight` / `StaticLightCollectionActor` per-class handling.
5. Prove the 945 unparsed exports hide no `AActor`-derived exports (class-ancestry check,
   independent of `umodel -list`). `Model`/`Polys` may still carry BSP/collision and
   `ShadowMap1D`/`LightMapTexture2D` affect lighting — excluded from placements, but they
   remain separate fidelity obligations, not "irrelevant".
6. Extend to all 44 Financial blocks, then the other districts.
