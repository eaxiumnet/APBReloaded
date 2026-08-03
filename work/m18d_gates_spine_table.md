# M18d — Verification spine pass/block table

Date: 2026-08-03. Run 1: `tools/run_verification_gates.ps1 -Scratch .omo/evidence/gates_spine`.
Run 2: same spine with `-AllowDeferred` wired into the oracle step
(`.omo/evidence/gates_spine2_run.log`).
State: task-18 frontend routing, task-18b character-create batch (verified=221,
allowlist 185), task-18c cook-audit negative matrix all landed.

## In-spine result (fail-fast, EXIT=1 at step 5)

| Step | Status | Evidence |
|---|---|---|
| financial_manifest_gate | PASS | FINANCIAL_MANIFEST_OK; 10 sub-gates PASS, extractor_parity SKIPPED |
| m3r_source_registry | PASS | SOURCE_REGISTRY_PASS roots=4 callers=3 |
| m3r_catalog_provenance | PASS | CATALOG_PROVENANCE_PASS 17/51/68 |
| m3r_fidelity_oracle | PASS | FIDELITY_ORACLE_PASS rows=20 allow_deferred=True (5 documented deferrals, below) |
| m3r_semantic_parity | **BLOCK** | mesh+placement parity PASS; 6 parity classes have no validator (below) |
| all later steps | NOT_REACHED | fail-fast design; no marker written |

Oracle deferral rows (all carry `pending_reason`; the gate's documented
`-AllowDeferred` policy accepts them, now wired into the spine invocation):
- menu.movies.splash (pending_manual — needs current UE playback comparison)
- character.morph_fallback (pending_manual — by-design bounded fallback, morph_spike.md)
- district.social.streamed_asset (pending_manual — ledger group row missing)
- vehicle.catalog.apbdb (pending_manual — uniform defaults, not fidelity stats)
- ui.screenshot.login.fixed_camera (deferred_require_binary — orchestrator capture)

## M3R steps verified fresh this session (individually, routing layer in place)

| Step | Status | Evidence |
|---|---|---|
| m3r_semantic_parity | PASS (standalone) | M3R_SEMANTIC_PARITY_PASS — mesh+placement only; spine requires 8 markers |
| asset_allowlist | PASS | VERIFIED_ASSET_ALLOWLIST_PASS entries=185 media=36 schema=2 |
| static_asset_audit | PASS | VERIFIED_ASSET_STATIC_AUDIT_PASS loads=5 media_loads=7 frontend_media_route=1 |
| strict_asset_provenance | PASS | STRICT_ASSET_PROVENANCE_PASS verified=221 |
| cook audit | FAIL (expected) | unverified=15,758 — district backlog (task 19) |
| frontend_routing probe | PASS | FRONTEND_RUNTIME_ROUTING_PASS, 12 markers incl. character-create |
| M3R asset QA orchestrator | PASS | M3R_ASSET_QA_PASS (static/positive/frontend/negative/canonical) |
| editor build | PASS | APBReloadedEditor Result: Succeeded |

## Not reached (post-oracle milestone gates, pre-existing)

bind_report, domain_tests, model_registry, host_client_loop, mp_parity, playable,
m8_social, frontend_menu, frontend_flow (off unless -IntegrationGate),
world_server, m7_travel, m7_directory, m11_mission, m14_social, m16_persistence,
m16_eviction. Historical: M6-era spine reached GATE_PASS ~11 min; playable/drive
steps caveated on M9/M12 content (work/_active.md).

## Blockers to full spine green

1. m3r_semantic_parity: the spine's `Require-Fresh` marker set demands 8 parity
   classes; only `validate_m3r_semantic_parity.py` emits any (mesh+placement,
   optional social). TEXTURE/MATERIAL/AUDIO/ANIMATION/VIDEO/UI_VISUAL have no
   validator anywhere in tools/. Needs 6 new parity validators or a spine
   contract change to the implemented subset.
2. cook audit: district routing (task 19) closes 15,758 unverified refs.
3. playable/drive/mp parity: M9/M12 content milestones.
