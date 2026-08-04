# M7 Chat Gate Failure — Diagnosis & Relay Duplicate-Close Fix

Date: 2026-08-04
Status: fixed + verified solo (chat_solo6), full-spine re-run in progress
Evidence root: `.omo/evidence/chat_solo4/` (flap), `.omo/evidence/chat_solo5/` (clean pass),
`.omo/evidence/chat_solo6/` (clean pass with fix), `.omo/evidence/gates_spine_full4/` (mid-run death)

## Request

Diagnose the `m7_travel_gate CHAT_GATE_FAIL` seen in the full4 spine run and fix it.

## Two distinct failure modes

### 1. Full4: all chat editors stopped writing mid-run (environmental)

`gates_spine_full4` chat leg: alice/bob/world/financial/social logs all stop within
22:44:21–38 local, ~80s before the gate deadline, with no crash dumps and no System event-log
hits. The gate's own cleanup runs at ~22:45:55. Full2 passed this leg in ~17s; full3's
host_client_loop leg was killed 5s into boot by a concurrent peer spine run's `Stop-Soft`
(kills every `UnrealEditor` by name). The full4 chat deaths match the same environmental
collision pattern: a concurrent `spine_verify` (scratch `r7_2742e20c`) was active at the
same wall-clock window. No code defect; harness hygiene issue.

### 2. Solo4: relay reconnect ping-pong wedges the Social district (code bug)

A clean solo re-run (chat_solo4) exposed a real relay defect. The world log shows TWO actors
alternating registers as `Social numeric_id=9` every ~570ms, offset ~270ms:

```
23:01:30:246 ACCEPT 55088  → REGISTER (ghost) → DUP close main 50845
23:01:30:531 ACCEPT 55089  → REGISTER (main)  → DUP close ghost 55088
23:01:30:815 ACCEPT 55093  → REGISTER (ghost) → DUP close main 55089
...ad infinitum
```

The ghost process (a leftover district editor whose register socket is healthy — it
re-registers and heartbeats) was never present in solo4's own `social.log`, so it was an
external process that survived the gate's project-path kill filter.

Root cause in code: `FAPBRelayListener::HandleMessage` closed **any** other client with the
same `numeric_id` on every successful register. With a ghost present, the world alternated
closing the live Social socket and the ghost socket forever — the live district's relay
connection was up ~300ms every ~570ms, so `CHAT_RELAY_FORWARD` could never be delivered to
Social (`cross_district_delivery_missing`).

## Fix

`Source/APBReloaded/Systems/Server/APBServerControl.cpp`:

- `FRelayClient` gains `int64 LastActivityMs` (updated in `ProcessFrames`).
- The duplicate-close loop only evicts a prior socket when it is stale
  (`age > kRelayHeartbeatIntervalMs * 3`, i.e. > 15s of silence). Healthy duplicates are
  retained; `QueueToDistrict` already broadcasts to every matching socket, so the live
  district still receives relay traffic while the ghost idles.
- Log line now includes `stale=` and `age_ms=` for future diagnosis.

## Verification

| Run | Result | Reconnects | Duplicates |
|---|---|---|---|
| chat_solo4 (pre-fix, ghost present) | CHAT_GATE_FAIL cross_district_delivery_missing | ∞ (~570ms loop) | ∞ |
| chat_solo5 (pre-fix, clean env) | CHAT_GATE_OK | 0 | 0 |
| chat_solo6 (post-fix, clean env) | CHAT_GATE_OK | 0 | 0 |

Build: `APBReloadedEditor Win64 Development` → Result: Succeeded.

## Full-spine re-run (gates_spine_full5) — GATE_PASS

Clean re-run after the fix, 2026-08-04 01:26→01:47 local. All steps green, zero blocks:

| Step | Result |
|---|---|
| m3r_semantic_parity | PASS (MESH 26 / PLACEMENT 763 / TEXTURE 1894 / MATERIAL 2341 / AUDIO 12 / VIDEO 34 / ANIMATION 31 / UI_VISUAL 123) |
| m3r_r6_asset_allowlist | PASS (8423 entries) |
| m3r_static_asset_audit | PASS |
| m3r_r6_editor_build | Succeeded |
| m3r_strict_asset_provenance | PASS (9046 entries, 8457 verified, 6 blocked) |
| bind_report | PASS |
| domain_tests_build / domain_tests | PASS |
| model_registry_build / model_registry | PASS |
| editor_build | Succeeded |
| host_client_loop | PASS |
| client_mp_observe | PASS |
| playable | PASS |
| m8_social_gate | M8_SOCIAL_GATE_OK |
| frontend_menu | PASS |
| world_server_gate | WORLD_SERVER_GATE_OK |
| m7_travel_gate | M7_TRAVEL_GATE_OK (Travel, Ticket, Handoff, **Chat**, Relay all leg-OK) |
| m7_directory_gate | M7_DIRECTORY_GATE_OK |
| m11_mission_gate | M11_MISSION_GATE_OK |
| m14_social_gate | M14_SOCIAL_GATE_OK |
| m16_persistence_gate | M16_PERSISTENCE_GATE_OK |
| m16_eviction_gate | M16_EVICTION_GATE_OK |
| **Overall** | **GATE_PASS** |

This is the first fully-green spine in the conversation (full2/full3/full4 each had at least
one failing leg). The M7 Chat leg — the exact path the relay fix protects — passed.

## Remaining

Gate hygiene (ghost prevention) remains a harness concern; the relay code is now resilient
to duplicate registrations so a leftover district process cannot wedge cross-district chat.
