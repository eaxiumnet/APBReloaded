# M7 Full-Compose Gate Run — Markers & Zero-Leftover Verification

Date: 2026-08-04 · Evidence: `.omo/evidence/m7_full_compose_run.log` +
`.omo/evidence/m7_full_compose/{travel,ticket,handoff,chat,relay}/` · Spine:
`.omo/evidence/spine_full6/`.

## Result

`M7_TRAVEL_GATE_OK` — all five legs compose green with the hardened teardown
(`tools/scripts/APBGateCleanup.ps1` sweep-and-verify).

| Leg | Gate script | Marker | Result |
|---|---|---|---|
| Travel | `run_m7_travel_gate.ps1` | `TRAVEL_GATE_OK` | PASS |
| Ticket | `run_m7_ticket_gate.ps1` | `DISTRICT_TICKET_GATE_OK` | PASS |
| Handoff | `run_m7_handoff_gate.ps1` | `HANDOFF_GATE_OK` | PASS |
| Chat | `run_m7_chat_gate.ps1` | `CHAT_GATE_OK` | PASS (incl. ghost-process regression) |
| Relay | `run_m7_district_client_gate.ps1` | `RELAY_DISTRICT_CLIENT_GATE_OK` | PASS |

## Markers

- Parent run log: `M7_TRAVEL_GATE_OK` x1, `FAIL` x0, `M7 leg OK` x5.
- Leg logs are UTF-16 (Tee-Object); verified via Python `utf-16` decode, not grep.
- `run_m7_relay_listener_gate.ps1` is NOT part of the compose — standalone socket
  probe only (register/auth-reject/oversize-reject), now M16-aligned.

## Zero-leftover verification

- Sweep-and-verify teardown held across every leg transition: 0 leftover
  `UnrealEditor`/`CrashReportClientEditor` processes at each poll and after
  `M7_TRAVEL_GATE_OK`.
- Each leg's start-time cleanup also runs over any leftover from the prior leg.

## Full-spine context

Inside `run_verification_gates.ps1` (spine_full6): `GATE_PASS`, all 32 steps
green, 38 required / 39 observed markers, missing=0, financial bind hit-rate 1.0.
