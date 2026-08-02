# M18d — Verification spine pass/block table

Date: 2026-08-03. Run: `tools/run_verification_gates.ps1 -Scratch .omo/evidence/gates_spine`.
State: task-18 frontend routing, task-18b character-create batch (verified=221,
allowlist 185), task-18c cook-audit negative matrix all landed.

## In-spine result (fail-fast, EXIT=1 at step 4)

| Step | Status | Evidence |
|---|---|---|
| financial_manifest_gate | PASS | FINANCIAL_MANIFEST_OK; 10 sub-gates PASS, extractor_parity SKIPPED |
| m3r_source_registry | PASS | SOURCE_REGISTRY_PASS roots=4 callers=3 |
| m3r_catalog_provenance | PASS | CATALOG_PROVENANCE_PASS 17/51/68 |
| m3r_fidelity_oracle | **BLOCK** | 5 documented deferrals (below); passes with `-AllowDeferred` |
| all later steps | NOT_REACHED | fail-fast design; no marker written |

Oracle block rows (all carry `pending_reason`; gate's own `-AllowDeferred` policy
accepts them — verified `FIDELITY_ORACLE_PASS rows=20 allow_deferred=True`):
- menu.movies.splash (pending_manual — needs current UE playback comparison)
- character.morph_fallback (pending_manual — by-design bounded fallback, morph_spike.md)
- district.social.streamed_asset (pending_manual — ledger group row missing)
- vehicle.catalog.apbdb (pending_manual — uniform defaults, not fidelity stats)
- ui.screenshot.login.fixed_camera (deferred_require_binary — orchestrator capture)

## M3R steps verified fresh this session (individually, routing layer in place)

| Step | Status | Evidence |
|---|---|---|
| m3r_semantic_parity | PASS | M3R_SEMANTIC_PARITY_PASS |
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

1. fidelity_oracle: wire `-AllowDeferred` into the spine's oracle invocation
   (matches the gate's documented deferral policy) OR resolve the 5 rows.
2. cook audit: district routing (task 19) closes 15,758 unverified refs.
3. playable/drive/mp parity: M9/M12 content milestones.
