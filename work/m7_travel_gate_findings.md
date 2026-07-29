# M7 — Travel-Spine Gate (N7) Findings & Green Evidence

**Status**: GREEN 2026-07-24 · Milestone: M7 (Travel / District / Relay / Chat)
**Deliverable**: `tools/run_m7_gate.ps1` (new) + REQUIRED wiring into
`tools/run_verification_gates.ps1`. Satisfies `work/m7_spec.md` acceptance #6
("`tools/run_m7_gate.ps1` prints a single terminal `M7_TRAVEL_GATE_OK`").

---

## Design decision

`run_m7_gate.ps1` is a **pure compositor**, not a re-implementation. The N7 spec
one-liner (world proc + district proc; client logs in → travels → redeems → chats)
is exactly the surface already proven by five green leg gates. The gate runs them
**strictly sequentially** (shared APB port contract → parallel = port collision),
trusting each leg's `exit 0/1` AND its terminal `*_GATE_OK` marker (belt-and-suspenders:
exit 0 but marker missing → FAIL).

| Leg (order) | Script | Required terminal marker |
|---|---|---|
| Travel | `run_m7_travel_gate.ps1` | `TRAVEL_GATE_OK` |
| Ticket | `run_m7_ticket_gate.ps1` | `DISTRICT_TICKET_GATE_OK` |
| Handoff | `run_m7_handoff_gate.ps1` | `HANDOFF_GATE_OK` |
| Chat | `run_m7_chat_gate.ps1` | `CHAT_GATE_OK` |
| Relay | `run_m7_district_client_gate.ps1` | `RELAY_DISTRICT_CLIENT_GATE_OK` |

Own contract literals: success `M7_TRAVEL_GATE_OK`, failure
`M7_TRAVEL_GATE_FAIL <reason>` (`leg_failed:<Name>`, `leg_marker_missing:<Name>`).
Capacity enforcement is intentionally **out of scope** (owned by Task 15).

## Green evidence (real end-to-end run)

- **Command**: `powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_m7_gate.ps1`
  (all legs; launches `UnrealEditor.exe` per leg).
- **Date**: 2026-07-24T15:16:19+02:00 · elapsed ~387s · pid 1530308.
- **Terminal log (durable copy)**: `work/logs/m7_travel_gate_20260724-151619.terminal.log`.
- **Markers observed** (in order):
  ```
  M7 leg OK: Travel (TRAVEL_GATE_OK)
  M7 leg OK: Ticket (DISTRICT_TICKET_GATE_OK)
  M7 leg OK: Handoff (HANDOFF_GATE_OK)
  M7 leg OK: Chat (CHAT_GATE_OK)
  M7 leg OK: Relay (RELAY_DISTRICT_CLIENT_GATE_OK)
  M7_TRAVEL_GATE_OK
  ```
- **Process hygiene**: clean exit 0; UNREAL_PROCS=0 after run (LEAKED=0); error log 0 bytes.

## Verification-gates wiring

`tools/run_verification_gates.ps1` runs `run_m7_gate.ps1` as a REQUIRED sub-gate
after `world_server_gate`, before `GATE_PASS`: child invoke → `Fail` on non-zero
exit → `Require-Fresh <log> -Minutes 30 "M7_TRAVEL_GATE_OK"`. The final
`gate_summary.json` includes `m7_travel_gate = "M7_TRAVEL_GATE_OK"`. A missing or
stale M7 marker now blocks the whole verification run.

## Scope notes

- M7 does **not** block on the district `.APB` asset export (spec R-M7-d); an
  existing freeroam map proves travel.
- Acceptance #5 (build green + `lsp_diagnostics`) is owned by the leg gates and the
  standing build gate, not re-checked here.
