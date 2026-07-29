# Tests Scope

## Overview

Standalone MSVC C++17 executables exercise the engine-free Domain layer without Unreal.
This harness does not cover replication, GameModes, UMG, or runtime networking.

## Harness

`build_and_run.ps1` locates Visual Studio with `vswhere`, enters an amd64 developer
environment, builds 17 `/std:c++17 /O2` executables into `Binaries/Win64`, and runs each
immediately. It stops at the first nonzero result.

```powershell
powershell -File D:\APBReloaded\tests\build_and_run.ps1
```

## Conventions

- Test files use a local `fails` counter and `CHECK(condition, message)` output, group
  assertions in `Test*` functions, print `FAILS=N`, and return nonzero on failure.
- Keep clocks, IDs, and persistence paths deterministic and caller-controlled.
- Add the new suite executable and every required Domain `.cpp` to
  `build_and_run.ps1`; headers compiling elsewhere is not harness coverage.
- Use isolated temporary state. Persistence tests own `%TEMP%\apb_persist_test` and
  intentionally recreate it at suite start.
- Catalog/fidelity/domain tests may read the real `D:\APBReloaded\Content\Data` tree;
  update fixtures and expectations together when a catalog schema intentionally changes.

## Coverage Boundaries

- `run_domain_tests.cpp` is the broad integration suite.
- Focused `run_*_tests.cpp` files own auth, persistence, chat, social, relay,
  matchmaking, auction, vehicles, progression, anti-cheat, and directory behavior.
- `run_layout_math_tests.cpp`, `run_client_loop.cpp`, and
  `run_model_registry_tests.cpp` are standalone probes not included by the canonical
  harness. Do not claim them as covered unless they were invoked explicitly.
- `Source/APBReloaded/Tests` is empty; no UE Automation suite currently replaces manual
  runtime gates.

## Anti-Patterns

- Adding an external test framework for a local change when the shipped harness pattern
  is sufficient.
- Weakening or removing an assertion to make the harness green.
- Treating a standalone Domain pass as proof that replication or UI behavior works.
