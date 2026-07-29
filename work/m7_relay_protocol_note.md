# M7 N4 (Domain half) — W↔D relay control-message codec — DONE + tested

**Author:** Qoder (2026-07-20). **Status:** additive, uncommitted (shared-worktree policy).
**Touched ZERO contended source** — `Systems/Server/` (N4 proper) is untouched and free for
Sisyphus.

## What landed

A pure-C++17 codec for the world↔district TCP/JSON control channel — the **message-format
layer only**. The `FSocket` transport + role wiring (N4 proper) stay in `Systems/Server/`.

New files (all additive):
- `Source/APBReloaded/Domain/APBRelayProtocol.h` — `RelayVerb`, `RelayMessage`, `RelayCodec`.
- `Source/APBReloaded/Domain/APBRelayProtocol.cpp` — encode/decode/stream de-frame + factories.
- `tests/run_relay_tests.cpp` — 9 groups, ~30 assertions.
- `tests/build_and_run.ps1` — added 11th suite `APBRelayTests` (links `APBRelayProtocol.cpp`
  alone — no other Domain deps).

**Verify:** `powershell -File tests\build_and_run.ps1` → all **11 suites `FAILS=0`**, exit 0.

## Wire format

One compact JSON object per line, `'\n'`-terminated (TCP is a stream → line framing). Only
fields relevant to a message are emitted; the decoder fills absent fields with defaults, so
round-trips are exact. Control chars in strings are neutralised to spaces so a body can never
inject a second line. CRLF is tolerated on decode.

Example: `{"v":"expect","district":"Financial","numeric_id":1,"account":"acct","character":"Nina","faction":"Enforcer","jti":"jti-abc"}`

## Verbs (grounded on `_active.md` M7 Files + `m7_spec.md` §4/§7)

| Verb | token | Direction | Fields | Maps to |
|---|---|---|---|---|
| `Register` | `register` | D→W | district, numeric_id, port | district comes online |
| `RegisterAck` | `register_ack` | W→D | numeric_id, ok | world accept/reject |
| `Heartbeat` | `heartbeat` | D→W | numeric_id, seq | liveness (→ M16 eviction) |
| `ReportLoad` | `report_load` | D→W | numeric_id, player_count | load balancing |
| `ExpectTicket` | `expect` | W→D | account, character, faction, jti, district, numeric_id | **ASK_DISTRICT_EXPECT** (pre-authorise jti) |
| `ExpectAck` | `expect_ack` | D→W | jti, ok | district will honour/refuse jti |
| `ChatRelay` | `chat` | W↔D | from, to, body, numeric_id | **chat.relay** (cross-district whisper) |
| `PlayerJoined` | `join` | D→W | account, character, numeric_id | presence up (redeemed ticket spawned) |
| `PlayerLeft` | `leave` | D→W | account, character, numeric_id | presence down |

Ports/addresses are **not** resolved here — `numeric_id`/`port` are carried as plain ints so
the Domain stays free of `Systems/APBPorts.h` (which lives above the Domain). Resolve the port
upstream with `apb::ports::DistrictPort(numeric_id)`.

## How Sisyphus wires N4 (handoff)

Include `Domain/APBRelayProtocol.h`. **Send** side:
```cpp
FString line = UTF8_TO_TCHAR(apb::RelayCodec::Encode(
    apb::RelayCodec::MakeExpectTicket(acct, chr, faction, jti, district, id)).c_str());
Socket->Send(...line as UTF-8 bytes...);   // Encode() already appends '\n'
```
**Recv** side — accumulate bytes into a `std::string` member and de-frame every tick:
```cpp
recvBuffer += <bytes read this tick>;
for (const apb::RelayMessage& m : apb::RelayCodec::DecodeStream(recvBuffer)) {
    switch (m.verb) { case apb::RelayVerb::ExpectTicket: /* pre-authorise m.jti */ ... }
}
// DecodeStream() erases consumed lines, leaves the partial tail in recvBuffer for next tick.
```

**Cross-district whisper (spec §6 + chat handoff):** when `ChatService::Submit` returns
`RecipientOffline`, forward as `MakeChatRelay(from, to, body, srcNumericId)` over the relay to
the district holding `to`; on receipt, feed it into the destination district's `ChatService`.

**ASK_DISTRICT_EXPECT flow (spec §4):** world sends `expect` before handing the client the
district address; district records the jti as pre-authorised, replies `expect_ack{ok}`. This is
"optional-but-preferred" for the HMAC MVP (district can verify HMAC standalone) but becomes
**mandatory** once Ed25519 lands (spec §5) — districts then hold only the public key and rely on
the world's `expect` to admit a jti.

## Design decisions

- **Codec is stateless** (all static methods) — no shared mutable state, thread-safe to call
  from any socket worker.
- **Emit-only-non-default** keeps periodic heartbeats tiny; `ok` is emitted explicitly for the
  two ack verbs so a rejection (`ok:false`) is unambiguous on the wire.
- **Skip-not-fail on garbage lines** in `DecodeStream` — a single malformed line can't wedge the
  control channel.
- Field names mirror `TicketClaims` (`account`/`character`/`faction`/`district`/`jti`) so an
  `ExpectTicket` maps 1:1 onto a ticket redeem with no renaming.

## NOT done here (still N4 proper — Sisyphus)

- `FSocket` TCP listener/connector on `apb::ports::Relay` (17800); district role that dials the
  world on boot and sends `register`; world-side directory of registered districts + heartbeat
  eviction timer (M16 hardening).
- Threading model for the socket worker; reconnect/backoff.
- Actually invoking the codec from `APBServerControl` / `APBDistrictGameMode`.

> Ready for a single-concern commit: `feat(M7/N4): Domain relay control-message codec + tests`.
