# M7 Chat leg — CHAT_GATE_FAIL alice_not_admitted (AESGCM key gaps)

**Date:** 2026-08-03
**Gate:** `tools/run_m7_chat_gate.ps1` (Chat leg of `run_m7_gate.ps1`, exercised by the
`m7_travel_gate` step of `tools/run_verification_gates.ps1`).

## Symptom

Full spine run (semantic-parity wave) reached `m7_travel_gate` and failed at the Chat leg:

```
CHAT_GATE_FAIL alice_not_admitted
M7_TRAVEL_GATE_FAIL leg_failed:Chat (exit=1)
```

The alice client probe logged only `PROBE_START` + `PROBE_TIMER` then sat silent for the
full 120 s gate budget. Its AbsLog showed ~1,081 drops of
`FAESGCMHandlerComponent::Incoming: received encrypted packet before key was set, ignoring.`
The world server accepted alice (`Join succeeded`) but every server->client packet was
dropped, so `AAPBPlayerState` never replicated and `RunWorldChatClientProbe` silently
early-returned (`!PlayerController || !PlayerState -> return`) forever.

## Root cause 1 — chat probe missing the client-side AESGCM key

M16 zero-trust made the world authority enable AES-GCM per connection
(`AAPBWorldGameMode::PostLogin` -> `NetConnection->EnableEncryption(Data)`, key from
`FAPBSecretProvider::TicketSecret()`). The engine ack handshake is not wired, so every
world-connecting probe must set the same key on its `ServerConnection`. That pattern
exists in `RunWorldServerClientProbe` (`WS_CLIENT_ENCRYPTION_ENABLED`),
`RunWorldTravelClientProbe` (`TRAVEL_CLIENT_ENCRYPTION_ENABLED`), `RunWorldHandoffClientProbe`
(per-connection re-key), and `RunReplicationProbe` — but **not** in
`RunWorldChatClientProbe`. The chat client therefore joined with no decryption key and
dropped all replication (login could never drive off `bWorldAuthOk`).

## Root cause 2 — districts never set their AESGCM key

After fixing root cause 1, the chat leg progressed (login, ticket, travel, both clients
admitted with `DISTRICT_TICKET_ADMITTED`) but stalled at the delivery step:
`CHAT_GATE_FAIL in_district_delivery_missing`. The Financial district logged the mirror
symptom — `received encrypted packet before key was set` — while dropping alice's
encrypted `Server_SubmitChat` RPC. Only `AAPBWorldGameMode::PostLogin` calls
`EnableEncryption`; `AAPBDistrictGameMode::PostLogin` never did, so district servers had
no key and could not decrypt any client->district RPC. (The handoff leg passed despite
this because district admission uses URL-carried tickets and plaintext replication passes
through the client's keyed component; chat delivery genuinely requires the RPC.)

## Latent defect 3 — M7 leg gates are not standalone

All five M7 leg gates rely on the spine exporting `APB_DEPLOYMENT_SECRET`
(`run_verification_gates.ps1` L376). Run in isolation, every spawned server process
(world + districts) halts immediately with
`DEPLOYMENT_SECRET_PROVIDER_HALT reason=missing_secret`, which surfaces as a confusing
gate timeout. The M16 gates set the secret in-process; the M7 legs never did.

## Fix

1. `APBSessionProbeSubsystem.cpp` — `RunWorldChatClientProbe` now enables AES-GCM on its
   `ServerConnection` with the shared ticket key, per-connection (static
   `TWeakObjectPtr<UNetConnection>` tracking) so the post-district-return reconnect gets
   its own key. Logs `CHAT_CLIENT_ENCRYPTION_ENABLED`.
2. `APBDistrictGameMode.cpp` — `AAPBDistrictGameMode::PostLogin` now mirrors the world
   authority's `EnableEncryption` block (shared key, `-DisableEncryption` guard). Covers
   `AAPBFreeroamGameMode` (derives from it). Restores client->district RPC delivery.
3. `tools/run_m7_{chat,travel,ticket,handoff,district_client}_gate.ps1` — each sets
   `[Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')`
   before spawning, matching the M16 gate pattern, so standalone leg runs are
   self-contained.

## Verification

- Editor target rebuilt: `Result: Succeeded` (both C++ changes).
- Isolated Chat leg (`tools/run_m7_chat_gate.ps1`): **CHAT_GATE_OK** — alice/bob admitted,
  district + relay + whisper + flood-throttle (`CHAT_DENIED reason=Muted`) all delivered.
- Full `tools/run_m7_gate.ps1` (all 5 legs) re-run in progress to confirm
  `M7_TRAVEL_GATE_OK` standalone.
- The AESGCM spam signature (1081 drops/client) is gone; per-connection re-key observed:
  `CHAT_CLIENT_ENCRYPTION_ENABLED` logged once per new ServerConnection (world -> district -> world).
