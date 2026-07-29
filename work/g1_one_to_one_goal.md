# GOAL CONTRACT — 1:1 Port (binding notepad)

Started: 2026-07-28 · Owner: Sisyphus · Status: **ACTIVE**

> User directive: *"Finish the work. don't stop before we have a 1:1 port."*
> This file is the durable notepad for that goal. APPEND, never rewrite.
> It does NOT compete with `work/_active.md` (master roadmap); it tracks the
> 1:1 completion definition + evidence that the roadmap never defined.

## Why this file exists

`_active.md:14` states the goal as "1:1 **wherever possible**" and carries a standing
non-goals list (§2). `retail_map_port_evidence.md` names `PI_Bytes` decode as "required
for true 1:1" but supplies **no denominator**. Result: "1:1" was unauditable — it could
never be proven complete. This file fixes that by measuring the reference installs.

## Measured denominator (2026-07-28, enumerated from reference installs)

Retail root: `C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded`
2011 root: `D:\APBReloaded\2011 apb\APB All Points Bulletin`

| Axis | Reference total | Ported (real provenance) | Ratio |
|---|---|---|---|
| `.APB` map files (retail) | **700** | 1 block (Block09) | 0.1% |
| — FinancialDistrict | 270 | 1 | |
| — WaterfrontDistrict | 268 | 0 | |
| — AsylumDistrict | 44 | 0 | |
| — PGCrateDistrict | 40 | 0 | |
| — PGBeaconDistrict | 30 | 0 | |
| — UIDistrict | 15 | 0 | |
| — RWorldSocialDistrict | 14 | 0 | |
| — APB_CustomisationSystems | 6 | 0 | |
| `.APB` (2011 build) | 576 | 0 | 0% |
| `.upk` packages (retail) | 6620 | — | |
| `.upk` packages (2011) | 4630 | — | |
| Imported uassets | — | 8613 | |
| UE maps built | — | 9 `.umap` | |
| Placement manifests | — | 15 files | |
| — of which `provenance=real` | — | **1** (`Financial_Block09_realv2.json`) | |

**Critical finding:** 14 of 15 placement manifests are the FABRICATED data called out at
`retail_map_port_evidence.md:12` (invented yaw ramp `(i*11)%360`, hardcoded `[1,1,1]`
scale). They are not port progress; they are debt to be regenerated with provenance.

## Known hard blocker (carried from retail_map_port_evidence.md)

`PI_Bytes` / UE3 `PrefabInstance` payload is undecoded. On Block09 alone: **227
PrefabInstances vs 137 directly-placed buildings**, so prefabs are the majority carrier of
map geometry. Until decoded, every block ports at partial fidelity — Block09 yielded 36
renderable rows from 274 source rows (238 reason-coded rejects, mostly prefab).

**Consequence:** prefab decode is the single highest-leverage unblock for map 1:1. It gates
all 700 `.APB` files. It is Wave 1, not a later task.

## Now

Intelligence wave in flight (4 agents): UE3 PrefabInstance format research (librarian),
Domain+Systems real-vs-stub audit, milestone ledger extraction, extraction-pipeline blocker
audit. Reference-install denominator: DONE (table above).

## Findings

- F1 (2026-07-28) Reference denominator measured: 700 retail `.APB`, 6620 retail `.upk`,
  4630 2011 `.upk`, 576 2011 `.APB`. Source: direct enumeration of both install roots.
- F2 (2026-07-28) Only 1 of 15 placement manifests carries real provenance. The rest are
  fabricated and must be regenerated, not counted as progress.
- F3 (2026-07-28) `?game=/Script/APBReloaded.APBFreeroamGameMode` is MANDATORY for `-game`
  standalone launches; `GameModeMapPrefixes` loses to `GlobalDefaultGameMode`, silently
  yielding the login movie instead of the district. Cost 1 wasted 4-min run.
- F4 (2026-07-28) Full-screen GDI/DX screen capture is invalid evidence on this box — it
  grabs whatever window is frontmost (captured the user's unrelated game). Only the
  in-engine `-APBCapture` path (RHI backbuffer, `-RenderOffScreen`) is admissible.
- F5 (2026-07-28) Pre-existing RED: `TestTicketPayloadEscaped` fails; `build_payload` in
  `APBTicket.cpp` has no JSON escaping while `APBClan/APBFriends/APBMailClaimJournal` each
  carry a local `JsonEscape*` helper. Another agent's TDD in flight — NOT mine to fix.
  Baseline suite state to compare against: **1455 PASS / 1 FAIL**, 3 of 4 suites `FAILS=0`.

## CORRECTION to the "known hard blocker" above (2026-07-28, supersedes §"Known hard blocker")

The claim that prefabs are "the majority carrier of map geometry" and therefore Wave 1 is
**FALSE**. Measured: all 13 distinct `TemplatePrefab` targets across Block09's 239
`PrefabInstance` exports resolve to **lighting** packages —
`featurelightsprefabs.*` (139 Industrial01_Prefab, 27 Industrial01_Interior, 20+19
Rectangular01, 10 FireExitLight, 6 Strip01, 5 Round01, 4+3 Rectangular02, 3
Strip01_Interior), plus `exitsigns.*` (2) and `shopfront_financial.*_Light_Prop` (1).
The `PI_SavedNames` local table is `LightComponent / CachedParentToWorld / LightGuid /
LightmapGuid / bHasLightEverBeenBuiltIntoLightMap / bSavedInMap`. These are light-fixture
placements. Prefab decode does NOT gate building geometry and is NOT Wave 1.

## Findings (continued)

- F6 (2026-07-28) **`PI_Bytes` format cracked; 239/239 decode.** It IS a UE3
  tagged-property stream, contradicting `decode_prefab_instance.py:5`. Two corrections vs
  the package-level walker: FName inside is **4 bytes** (bare index, no Number), and
  indices resolve against the **local `PI_SavedNames` table**, not `pkg.names`. Chain start
  varies per instance (0x3e, 0x14a, ...), which is exactly why the fixed
  `_MATRIX_OFFSET=0x56` produced `NO_GO`. `CachedParentToWorld` is a 0x40 `Matrix`.
  Layout NOT yet asserted: slots 0/4/8 are exactly 0.0 and slot 12 exactly 1.0 across all
  239, with translation in slots 13/14/15 — one float later than a standard `FMatrix`.
  Under Oracle adjudication; do not encode a layout until resolved.
- F7 (2026-07-28) **ArVer 633/673 property gates do NOT apply to this build.** Probe over
  3400 Block09 exports: `ByteProperty`×2638, `BoolProperty`×3810, with enum names resolving
  correctly (`RemoteRole`→`ENetRole`, `PrefabInstanceState`→`EPrefabInstanceState`) and
  2455/3400 chains terminating cleanly on `None`. LicenseeVer=33 backported both changes.
  `walk_from` is correct as written — the suspected 8-byte over-read / 3-byte under-read is
  NOT a bug. Retail header verified: ArVer=**564**, LicenseeVer=**33**.
- F8 (2026-07-28) **ROOT CAUSE of the 79 `mesh_unresolved` rows (58% of Block09 buildings).**
  Those `StaticMeshComponent` exports carry **no `StaticMesh` property at all** — their full
  property set is `['HiddenEditor','HiddenGame','LightingChannels']`. This is UE3 archetype
  delta serialization: an instance serializes only properties differing from its
  `ObjectArchetype`, so an unchanged `StaticMesh` is never written. The extractor reads
  instances only and never follows the archetype chain. Accounting closes exactly:
  79 no-mesh + 58 resolved = 137 actors; 58 − 22 `asset_not_imported` = **36 renderable**.
  Fix requires the export table's `ArchetypeIndex`, which `parse_package` does not parse
  (`umodel -list` exposes only idx/off/size/cls/name). **This is the true Wave 1 candidate.**
- F9 (2026-07-28) Discrepancy to reconcile: my probe classes 79 `CollisionComponent` edges
  as no-`StaticMesh` (→ would be `mesh_unresolved`), but the shipped manifest histogram
  reports `collision_only:137`. `placement_records` checks `mpath is None` BEFORE the
  `collision_only` branch, so the two disagree. `write_block09_real_manifest.py` may
  post-process reasons. Reconcile before regenerating any manifest.
- F9-RESOLVED (2026-07-28) The F9 disagreement is a **masking bug in the manifest writer**,
  not a probe error. `write_block09_real_manifest.py:72-73` stamps `collision_only` on every
  `CollisionComponent` edge unconditionally, keyed on the edge NAME, before the extractor's
  own reason is consulted:
      `if record.get("edge") == "CollisionComponent": reason = "collision_only"`
  So all 137 collision edges report `collision_only` even though 79 of them carry no
  `StaticMesh` whatsoever (F8). `mesh_unresolved:79` counts only `StaticMeshComponent` edges.
  Accounting closes at 274 = 137 `collision_only` + 79 `mesh_unresolved` + 22
  `asset_not_imported` + 36 renderable.
  **Authoritative semantics going forward:** `collision_only` MUST mean "mesh resolved, and
  this edge is collision geometry deliberately not rendered". It MUST NOT be used when the
  mesh failed to resolve — that case is `mesh_unresolved` (or the new archetype-inherited
  case) regardless of which edge it came from. Evidence decides the reason; edge name only
  decides renderability. Fix this before regenerating any manifest, or every regenerated
  file inherits the mislabel and the 79 stay invisible.
- F10 (2026-07-28) **Plan agent is unusable this session — 3 consecutive failures**:
  (1) `EnterPlanMode` tool error, (2) empty output, (3) `Session error: Tool call not
  allowed while generating summary: BashOutput`. Per the 3-failure rule, escalated to
  Oracle as sanctioned fallback for the wave-ordered plan. Do not retry the plan agent.

## F13 - SHIPPED BUG: FName-valued properties lose their Number

`decode_prop` returns only the base name for `NameProperty`, so
`m_VertexLitComponent` on `StaticMeshComponent_0` decoded as `'StaticMeshComponent'` when
the stored body is `d6020000 8d020000` = names[726]=`StaticMeshComponent`, Number=0x28d=653,
i.e. **`StaticMeshComponent_652`** - a different export entirely.

Same bug class as F11.2, but in the shipped extractor rather than a probe. Every
FName-valued property in the pipeline silently collapses distinct references onto one bare
name. Verified 0 numbered-name collisions across all 3400 exports, so once the Number is
kept, resolution by name is unambiguous.

## F14 - F8 OVERTURNED: retail renders the VERTEX-LIT component, not the actor's mesh

`HiddenGame=True` and `HiddenEditor=True` on **all 137** direct `StaticMeshComponent`
exports - including the 58 that carry a real mesh, 36 of which the shipped manifest spawns
as renderable. Verified against raw bytes, so this is not a BoolProperty decode artefact.

The visible geometry is the `m_VertexLitComponent` target (resolvable only after F13):

- 57/57 targets are `StaticMeshComponent` with a `StaticMesh` and **no Hidden properties**
  (UE3 default false -> visible); they also carry the collision flags and `Materials`.
- 57/57 are outered to a **different** `cStreamedBuildingActor`, never the naming actor.
- 57/57 have **no transform of their own** -> world placement comes from the owner actor.
- 57 distinct meshes, all `*_VertexLit_LOD` in `..._block09_package`.

Host identity closes the accounting exactly:

```
host_is_nomesh_actor 57/57      |Location(namer) - Location(host)| = 0.000 on 57/57
137 = 57 namers + 57 hosts + 22 no-mesh non-hosts + 1 resolved-without-vertex-lit
      (the 57 hosts are a subset of the 79 no-mesh actors)
```

So the "79 missing buildings" of F8 are not 79 losses: **57 are the render hosts** and only
**22** are genuinely unexplained. This is APB's baked-lighting scheme - authored mesh
hidden, pre-lit duplicate rendered.

**Consequence:** Block09's faithful renderable count is **57**, not the 36 currently
shipped, and those 36 were built from the one component retail explicitly hides. The
in-engine evidence in `work/logs/block09_realv2_s4/` proves the pipeline runs, NOT that it
is 1:1. `Financial_Block09_realv2.json` must be regenerated from the vertex-lit edge.

## F15 - The remaining 22 are genuine source-side absences

The 22 no-mesh non-host actors pair off internally at byte-identical Locations, in the same
namer/host shape as the explained 57: twelve 160-byte actors (the ones carrying
`LightingChannels`) against ten 70-byte actors, plus `_14`/`_15` unpaired. All four
hypotheses were tested:

- **C1 duplicates - rejected.** All 22 have unique Locations; none coincides with an
  accounted (namer or host) actor.
- **C3 component set - rejected.** All 22 sets are the invariant
  `[FeatureGroupComponent, null, PointLightComponent]`; 0/22 contain any component with a
  `StaticMesh`.
- **C2 cross-package - rejected.** `FinancialDistrict_Props_Block09` (2500 exports),
  `_ArtProps_Block09` (1192) and `_Block09_Design` (244) contain **0/12** actors at those
  Locations.
- **C4 real absence - accepted.** No geometry for these 22 exists in Block09's package
  family. They must be emitted with an explicit reason code, never silently dropped and
  never back-filled with a substitute mesh.

## F16 - Sibling packages use placement classes the extractor does not handle

`FinancialDistrict_Props_Block09` has 1038 placed actors but `with_mesh=0` through the
`StaticMeshComponent` edge, while holding 123 `cStreamedLightingStaticMeshActor`, 289
`cProp` and 408 `PrefabInstance`. `_ArtProps_Block09` has 499 placed / 44 meshed.

`mesh_component_refs` only knows `StaticMeshComponent` and `CollisionComponent` on
`cStreamedBuildingActor`, so props and street furniture are invisible to the pipeline
regardless of the F14 fix. Any "district is 1:1" claim must cover these classes too; scope
this before promising a full block.

Related: `m_aComponents` decodes to a **dict**, not a list. `mesh_component_refs`'
docstring asserts a three-slot layout measured via a raw walk, but every consumer that
treats the decoded value as a list sees nothing at all.

## Learnings

- The repo's own gate scripts are the trustworthy evidence source; ad-hoc capture is not.
- `_active.md` is 1404 lines and authoritative for sequencing — read it before planning.
- Concurrent agents share this worktree. The dirty tree in `Source/` and `tools/scripts/`
  is NOT mine; never revert it.
- **Verify a premise before planning on it.** Two premises this session survived multiple
  documents and one specialist audit before measurement killed them: "prefabs carry the
  building geometry" (they carry lights) and "the ArVer gates make the walker buggy" (the
  licensee build backported them). Both were cheap to test and expensive to assume.
- A `reason` code that collapses several distinct root causes into one label hides the
  actual bug. `mesh_unresolved` conflated "no property, inherited from archetype" with
  "reference failed to resolve" — different fixes, same label.
- Reconcile a probe against the shipped artifact before trusting either (see F9).
- **An exact one-record shift is an index-base bug, not a layout bug.** Garbage means the
  layout is wrong; a perfect `parsed[N] == truth[N-1]` means the comparison base is wrong.
  Reading the symptom correctly saved discarding a layout that was already right (F11).
- **Read property VALUES, not just their presence.** F8 recorded that
  `HiddenGame`/`HiddenEditor` existed and stopped there. Their values were `True` on all 137
  and inverted the entire conclusion about what renders (F14).
- **A green in-engine capture proves the pipeline runs, not that it is 1:1.** Block09's
  `STREAM_SPAWN n=36` / `MESH_LOAD failed=0` evidence was real and the manifest was still
  wrong, because fidelity was never compared against the source's visibility flags.
- **Drop the FName Number and every instanced reference aliases.** Bit me twice in one
  session, once in a probe and once in shipped code (F11.2, F13).

## F11 - Export table layout PROVEN at ArVer 564 / LicenseeVer 33

`FObjectExport::Serialize3` as transcribed from `tools/UEViewer/Unreal/UnrealPackage/UnPackage3.cpp:367-487`
is correct: `MATCH=3400 MISMATCH=0 no_umodel_row=0` against `umodel -list` off/size on
`FinancialDistrict_Block09`. No ComponentMap at 564 (gated `ArVer < 543`). The earlier
`MISMATCH=3399` was two probe bugs, not a layout error:

1. Parsed rows were keyed 1-based while `umodel`'s index column - and `deref`'s already
   proven `ref - 1` base - are 0-based. Symptom was a perfect one-record shift
   (`parsed[N] == umodel[N-1]`), which is the signature of an index-base error, not of a
   desynchronised layout. A wrong layout yields garbage, never an exact shift.
2. `R.fname` returns only the base name, so instanced component names collapsed
   (`StaticMeshComponent` instead of `StaticMeshComponent_632`) and every lookup missed.

## F12 - Archetype chasing CANNOT recover the missing meshes (kills the planned Wave 1)

All 1177 non-null `Archetype` refs in Block09 are **imports**. The four known no-mesh
components all point at `APBGame.Default__cStreamedBuildingActor.StaticMeshComponent0` -
one shared class-default subobject in `APBGame.u`, outside the map package.

A single CDO cannot carry 79 different building meshes. Adding `archetype_ref`
chain-following inside the package would resolve all 79 to one identical mesh - fabrication
wearing a provenance label. `tools/scripts/test_archetype_resolve.py` encodes this wrong
premise and MUST NOT be driven to GREEN as written; it needs rewriting per F14.

H-DELTA (properties genuinely absent on disk) confirmed over H-WALKER (walker losing
bytes): anchor=8 on all 137, `hit_None=True` on all 137, and the unresolved component
exports are physically smaller (70B x67, 160B x12) than resolved ones (220B x57, 188B x1).
Hand-decoded a 70-byte export end to end: `HiddenGame`(25B) + `HiddenEditor`(25B) +
`None`(8B) + 4B trailing = 62B = exactly `size - anchor`. The walker is not lossy here.


## F17 - loader contract vs fidelity record split (RESOLVED)

The 22 source-visible rows with no recovered geometry previously reached
`APBDistrictPlacement.h` L572-577 and were rejected as `invalid_mesh_id` - a schema-fault
label for what is actually a documented retail recovery gap. Behaviour was already correct
(they must not spawn); only the label was wrong, and it conflated the two causes Oracle
forbade conflating.

Fixed WITHOUT touching the concurrently-modified `APBDistrictPlacement.h`, by making the
manifest self-describing across two INDEPENDENT contracts:

- `reason` = loader contract ("this row yields no runtime actor"). Now set to
  `retail_geometry_not_recovered` on the 22. The C++ parser short-circuits on `reason` at
  L561-571, BEFORE its `mesh_id` check, so the honest code flows through.
- `geometry_resolution` = fidelity record. Unchanged.
- `renderable_count` = 35 (spawnable, matches the parse-test fixture semantics at
  `run_placement_parse_tests.cpp` L330-338).
- `source_visible_placement_count` = 57 (fidelity obligation, reason-independent).

Counts now conserve as 35 spawnable + 102 reasoned = 137. `PLAYER_START` is byte-identical
to the previous 57-row centroid, proving spawn geometry still derives from all 57
source-visible rows and not from the 35.

Verified at the REAL C++ boundary with a throwaway driver over the real manifest:

    BEFORE (reason stripped):  SCHEMA_FAULT_LABELS=22  NAMED_RECOVERY_GAP=0
    AFTER  (shipped manifest): SCHEMA_FAULT_LABELS=0   NAMED_RECOVERY_GAP=22

`SPAWNABLE_ENTRIES=35` in BOTH states - runtime behaviour unchanged, only the label moved.

## F18 - GATE_ROW_IDENTITY never pinned row identity (BUG, FIXED)

`set_sha()` hashed ONLY the mesh-object stem of `mesh_path`. A forged `source_id` left the
stem multiset untouched and walked straight through a gate literally named
GATE_ROW_IDENTITY. The earlier "4/4 mutations caught" claim was weaker than reported: those
mutations SWAPPED rows between sets, perturbing the stem multiset incidentally.

`set_sha()` now hashes `source_id::stem` pairs. Constants updated:
`EXPECTED_BOUND_SET_SHA=ea0c6f8833d314d8`, `EXPECTED_MISSING_SET_SHA=b54639f621d0b3e9`.
Re-proven against 6 mutations, 6/6 CAUGHT (was 5/6 before the widening):
strip_loader_reason, relabel_as_not_imported, launder_to_invisible, drop_unrecovered_row,
swap_identity_hold_counts, revert_count_split.

### Learnings

- A gate's NAME is not evidence of what it checks. GATE_ROW_IDENTITY hashed mesh stems, not
  row identity. Read the predicate, never trust the label.
- Mutation testing that only PERMUTES data can pass a hash that omits the identity field.
  Forge a single field in place to find what a digest actually covers.
- Two consumers of one artifact need two independent fields. Overloading `reason` to mean
  both "no runtime actor" and "no fidelity gap" is what produced the mislabel.
- When the consumer is another language, predicting its classification is not evidence.
  A ~40-line driver over the real header gave a RED/GREEN pair in one compile.
- A concurrently-modified file is not editable. Making the DATA self-describing achieved
  the same corrected classification with zero conflict risk.


## F19 - F17's 22 were NEVER absent: `sanitized_stem` emitted double underscores (BUG, FIXED)

F17 labelled 22 source-visible rows `retail_geometry_not_recovered` and F12 concluded the
missing meshes were unrecoverable. For **F17's 22 that verdict was wrong**, and the label
was load-bearing enough to be believed for two milestones. The geometry was present in
Block09 and already imported the whole time.

`write_block09_real_manifest.py:sanitized_stem` mapped each non-alphanumeric character to
one underscore. Archetype names contain the literal run `" ("` and `") "`, e.g.
`Upmarket Corporate (UC)`, so the function produced consecutive underscores:

    stub:            ...Upmarket Corporate (UC) 0004_VertexLit_LOD
    sanitized_stem:  ...Upmarket_Corporate__UC__0004_LOD     <- 2x "__"
    imported asset:  ...Upmarket_Corporate_UC_0004_LOD       <- 1x "_"

The importer had already collapsed runs; the resolver had not. Every lookup for those 22
missed by exactly two characters and fell through to `retail_geometry_not_recovered`. Fix is
one regex, `re.sub(r"_+", "_", ...)`, in `sanitized_stem`.

`base_mesh_path` was present on all 57 input records all along, so the ONLY defect was the
stem mismatch. A `derived_base_mesh` name-derivation tier was written, proven to never fire
in production, and removed rather than shipped as dead code.

Counts, before -> after:

| Field | F17 | F19 |
|---|---|---|
| `geometry_bound` / spawnable | 35 | **57** |
| `geometry_missing` | 22 | **0** |
| `retail_geometry_not_recovered` rows | 22 | **0** |
| `EXPECTED_BOUND_SET_SHA` | ea0c6f8833d314d8 | 8bcb965a5a398ec7 |
| `EXPECTED_MISSING_SET_SHA` | b54639f621d0b3e9 | e3b0c44298fc1c14 (sha256 of "") |

Verified at the REAL C++ boundary with a throwaway driver over `ParsePlacementManifestJson`:

    PARSE_OK=1  PLACEMENTS=57  SPAWNABLE_ENTRIES=57  DISTINCT_UE_PATHS=57
    IMPORTED_DISTRICT_PATHS=57  ENGINE_CUBE_PLACEHOLDERS=0  USES_ENGINE_CUBES=0
    NAMED_RECOVERY_GAP=0  DRIVER_VERDICT=PASS

`DISTINCT_UE_PATHS=57` is the anti-F14 check: 57 rows bind 57 **distinct** meshes, so this
is not the archetype collapse F12 rejected (many buildings onto one shared CDO mesh). Each
stub binds its own same-named base. `PLAYER_START` is byte-identical to the F17 centroid
(178011.422149, 132207.000137, 2340.019775), so spawn geometry did not move.

**Scope - F15 and F12 still stand.** A DIFFERENT population of 22 exists: the null-`mesh_path`
non-host actors labelled `no_geometry_in_package_family`. Overlap with F19's 22 is **0**.
Those have no mesh reference to resolve, F15's real-absence verdict is untouched, and F12's
CDO reasoning remains correct for the 79 no-mesh components. Only F17's source-visible 22
are reclassified. Gate 7/7 PASS, `test_archetype_resolve.py` green **unchanged** (its
`EXPECTED_RENDERABLE = 57` was written anticipating 57 and only now holds), C++ harness at
baseline 1455 PASS / 1 FAIL (`TestTicketPayloadEscaped`, pre-existing; `APBTicket.cpp` and
`run_auth_tests.cpp` contain 0 references to placement/manifest, so it cannot read this data).

### Learnings

- `retail_geometry_not_recovered` was a resolver MISS, never an absence proof. A negative
  label earned by a failed lookup is a statement about the lookup, not about the retail data.
- Two normalizers over one namespace must be the same function. The importer collapsed
  underscore runs and the resolver did not; nothing on either side could detect the drift.
- Reconcile against the artifact that already exists. One `Compare-Object` of resolver stems
  vs imported asset stems exposed in seconds what two milestones of package archaeology missed.
- Test the path production actually takes. The first GREEN came through a fallback tier that
  never runs in production, which would have shipped an untested resolver as "verified".
- Identical counts are not the same population. Two distinct sets of 22 sat in one document;
  assuming they were one set would have wrongly overturned F15.
