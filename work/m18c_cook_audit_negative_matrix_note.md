# M18c — Cook audit negative-fixture matrix

Date: 2026-08-02

## Result

`tools/scripts/test_check_static_cook_assets.py` — 10-cell matrix, all green.
Real audit counts unchanged (refs=15985 allowlisted=227 unverified=15758
engine 0/0). `M3R_ASSET_QA_PASS`.

## Matrix cells (task-21 negative controls, path-closure layer)

- missing_allowlist (named reason)
- malformed JSON (named reason, parse detail appended)
- missing entries field (detail=entries_missing)
- empty entries -> every ref unverified (QA orchestrator's negative shape)
- missing object_path (detail=missing_object_path)
- unsupported class (reason=unsupported_class)
- supported-but-wrong class documented limitation (fails via unverified;
  runtime probes own that check)
- engine ref blocked (engine_blocked=1, sample in JSON report)
- positive control (PASS, closed counts)
- dotless catalog ref closed via leaf-strip

## Audit hardening

- `Get-AllowlistPaths` now fails closed with named reasons on malformed
  allowlist, missing object_path/class, and unsupported class (media entries
  exempt; supported set mirrors the registry, source of truth noted).
- Scan pattern fixed: was `/Game/`-only, so `/Engine/` refs were never seen
  and the task-20 engine-internal policy was dead code. Now `(?:/Game|/Engine)`.
  No real `/Engine/` refs exist in Content/Data, so real counts are unchanged.
