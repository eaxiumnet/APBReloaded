# Work Scope

## Overview

Planning, architecture decisions, verification evidence, debug notes, generated status
reports, logs, archives, and local caches. Nothing here is runtime source.

## Structure

| Location | Purpose |
|---|---|
| `_active.md` | Single active master roadmap and current milestone state |
| `ARCHITECTURE.md` | Accepted ownership, networking, persistence, and pipeline decisions |
| `_archive/` | Completed or abandoned plans and superseded work artifacts |
| `logs/` | Runtime/build/gate evidence; may be regenerated |
| `apbdb_cache/` | Local source cache, not authored catalog output |
| `IMPORT_STATUS.md` | Generated view of `tools/import_ledger.json` |

## Conventions

- Read `_active.md` before implementation. Update the existing plan rather than creating a
  competing active roadmap.
- Record observable evidence: exact command, date, terminal marker, result, and relevant
  log path. Do not label a milestone proven from code inspection alone.
- Move completed or abandoned effort plans to `_archive/`; retain decision history.
- After more than two failed implementation attempts, write a focused debugging note with
  hypotheses, attempts, evidence, and the precise blocker before retrying further.
- Generated reports identify their source file/script. Edit the source and regenerate.
- Keep logs and caches out of commits unless the active plan explicitly requires a small,
  durable evidence artifact.

## Anti-Patterns

- Multiple files claiming to be the active plan.
- Editing archived plans to rewrite history instead of adding a current decision note.
- Treating cached apbdb responses, old logs, or prior build output as current proof.
- Storing runtime code, assets, secrets, or executable dependencies under `work/`.
- Marking work complete without the build/test/manual gate required by the root contract.
