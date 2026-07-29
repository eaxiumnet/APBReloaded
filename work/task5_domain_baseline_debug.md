# Task 5 baseline blocker

The canonical standalone harness was run twice on 2026-07-23. Attempt one stopped before any suite with MSVC C1083 permission denied opening the compiler-generated `APBWorldService.obj`. An exact retry after the transient `cl` process exited compiled and ran ten harness groups, each with `FAILS=0`, then stopped before the relay suite.

The remaining failure is deterministic and source-backed: `APBRelayProtocol.h` declares nine `RelayCodec::Make*` factories with additional defaulted request ID, sent time, auth, and ticket-expiry arguments, while `APBRelayProtocol.cpp` still defines the older shorter signatures. MSVC reports C2511 for each definition. The harness cannot reach relay or later suites until the owning relay task reconciles this interface.

No product or test source was edited by this audit. Evidence and raw reports:

- `.omo/evidence/task-5-apb-reloaded-lan-port-completion.json`
- `.omo/evidence/task-5-apb-reloaded-lan-port-completion.log`
- `tests/task5_build_and_run_raw.log`
- `tests/task5_build_and_run_raw_retry.log`
