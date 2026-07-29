
## 2026-07-24 delayed-exec investigation
- Artifact: gate scratch C:\Users\Support\AppData\Local\Temp\apb_m7_chat_gate_final.
- Observed: authenticated Financial admission succeeds for ChatAlice and ChatBob; no CHAT_CLIENT_SUBMIT marker; gate fails in_district_delivery_missing.
- Hypotheses: (1) ExecCmds dispatch before APBPlayerController exists; (2) Exec parsing quotes malformed; (3) command timers are lost during travel.
- Cleanup: gate finally killed tracked processes; no additional runtime resources retained.

### 2026-07-24T11:07:16Z - first runtime gate
- Source: C:\Users\Support\AppData\Local\Temp\apb_m7_chat_gate_final\financial.log
- Value: DISTRICT_TICKET_ADMITTED account=ACC-chat_alice char=ChatAlice faction=Enforcer; DISTRICT_TICKET_ADMITTED account=ACC-chat_bob char=ChatBob faction=Enforcer; no CHAT_CLIENT_SUBMIT in alice engine log.
- Interpretation: admission passed; command automation did not reach Server_SubmitChat.
- Refutes/Confirms: refutes pre-admission failure; leaves H1/H2/H3 open.

## Planned debug fix
- Modify APBSessionProbeSubsystem.{h,cpp}: parse APBChat/APBChatTravel tokens from -ExecCmds on startup, retain typed commands, and arm them only after the first authenticated district arrival. Revert: source edit is intended final fix, not a temporary artifact.
- Verification: re-run the bounded chat gate and require CHAT_CLIENT_SUBMIT plus delivery/denial markers.

### 2026-07-24T11:20:38Z - green runtime gate
- Source: C:\Users\Support\AppData\Local\Temp\apb_m7_chat_gate_green2\financial.log and social.log
- Value: CHAT_DELIVERED channel=District from=ChatAlice to=ChatBob; CHAT_RELAY_FORWARD to=ChatBob; CHAT_DELIVERED channel=Whisper from=ChatAlice to=ChatBob; CHAT_DENIED reason=Muted; terminal CHAT_GATE_OK.
- Interpretation: all gate scenarios passed through the server authority path.
- Refutes/Confirms: confirms the final fix.
- Cleanup: process sweep output required before completion; scratch log directory intentionally retained as QA evidence.

### 2026-07-24T11:20:40Z - cleanup receipt
- Source: process sweep after green gate.
- Value: LEAKED_APB_GATE_PROCESSES=0.
- Interpretation: gate finally cleanup removed all tracked UnrealEditor and CrashReportClientEditor processes.
- Refutes/Confirms: confirms teardown requirement.
