# M14 Social Architecture — Implementation Findings

**Date:** 2026-07-29
**Status:** Implementation complete, build GREEN, domain tests GREEN (1 pre-existing fail unrelated)

## Architecture

The cross-process social authority blocker is resolved. Social authority lives in the world
process (`AAPBWorldGameMode::SocialAuthority`), but district clients can now reach it through
the relay protocol:

```
District Client → Server_Social* RPC → DistrictGameMode → SocialRequest relay → WorldGameMode
  → HandleSocialRequest (dispatch via UGI bridge) → SocialResult relay → DistrictGameMode
  → ApplySocialResult → Client_SocialResult → PlayerState replication
```

## Changes (11 files)

### T13: Server RPCs (APBPlayerState.h/.cpp)
- `Server_SocialClan(Op, Arg1, Arg2)` — clan create/invite/accept/decline/kick/motd/rank
- `Server_SocialFriend(Op, Target)` — friend request/accept/decline/remove
- `Server_SocialGroup(Op, Target, Arg)` — group create/invite/accept/leave/ready
- `Server_SocialMail(Op, Payload)` — mail send/claim
- `Client_SocialResult(Op, Status, Body)` — server→client result callback
- `DispatchSocialOp` (district: forwards as SocialRequest relay)
- `DispatchSocialOpDirect` (world: dispatches via UGI bridge directly)
- `AuthCharacterFor` helper resolves character name on both world and district

### T14: Relay Presence + Social Request Handling (APBWorldGameMode.h/.cpp)
- `MarkRelayPlayerJoined/Left` now calls `SocialAuthority.friends_svc.SetOnline()`
- `HandleSocialRequest()` — dispatches relayed social ops via UGI bridge, sends SocialResult
  back to originating district, emits `SOCIAL_GATE_*` markers for gate validation
- `PushSocialStateToPlayerStates()` — reads clan/group/friend state from SocialAuthority
  and pushes to replicated PlayerState fields (ClanId, ClanRole, GroupId, etc.)
- Gate markers: `SOCIAL_GATE_CLAN_OK`, `SOCIAL_GATE_FRIEND_OK`, `SOCIAL_GATE_GROUP_OK`,
  `SOCIAL_GATE_MAIL_OK`

### Cross-Process Relay Routing (APBServerControl.cpp)
- World relay listener routes `SocialRequest` to world's inbound queue
- District relay client routes `SocialResult` and `SocialChat` to district's inbound queue
- `ProcessRelayReturns()` in both GameModes extended to handle new verbs

### District-Side Handling (APBDistrictGameMode.h/.cpp)
- `ApplySocialResult()` — receives SocialResult from world, parses op from operation_id,
  delivers via `Client_SocialResult`, updates replicated social fields
- `ApplySocialChat()` — receives SocialChat from world, delivers to chat system
- `GetRelayControl()` public accessor for PlayerState RPC forwarding
- `ProcessRelayReturns()` extended: `SocialResult → ApplySocialResult`, `SocialChat → ApplySocialChat`

### T15: Chat Seam (APBFreeroamGameMode.cpp)
- `PostLogin()` now calls `ChatService.SetClan()` and `ChatService.SetGroup()` with the
  player's clan/group membership from PlayerState

### T16: Social Probe (APBSessionProbeSubsystem.cpp/.h)
- `RunSocialProbe()` implemented for alice/bob roles
- Alice: login → create clan → send friend request → create group → send mail → emit OK
- Bob: login → accept clan invite → accept friend request → accept group invite → emit OK
- 30-second timeout guard prevents infinite polling
- State flags reset when timer arms

### T17/T18: Social Widget (APBSocialWidget.h/.cpp)
- 4-tab UMG panel: Clan, Friends, Group, Mail
- Built entirely in C++ via WidgetTree (no Blueprint asset needed)
- Clan tab: clan info display + create clan input/button
- Friends tab: online friend count + add friend input/button
- Group tab: group info display + create group button
- Mail tab: To/Subject/Body inputs + send mail button
- All actions dispatch through `Server_Social*` RPCs
- 1-second auto-refresh of displayed state from replicated PlayerState fields

### T19: HUD Mount (APBFreeroamHUD.h/.cpp)
- `SocialWidget` member added to `AAPBFreeroamHUD`
- Widget created in `BeginPlay()`, added to viewport at Z-order 20
- Toggle key bound via `InputComponent` (default: Tab key)
- `ToggleSocialWidget()` shows/hides the panel

## Validation

- ✅ `APBReloadedEditor` build: Succeeded
- ✅ Domain tests: 30/31 pass (1 pre-existing fail: `TestTicketPayloadEscaped` — unrelated to M14)
- ✅ Code reviewer: reviewed (findings addressed: TObjectPtr lambda fix, UWidgetTree::Slot removal,
  mail body field added, social probe timeout added, state flag reset on timer arm)

## Known Limitations

1. **Code duplication**: `DispatchSocialOpDirect` (APBPlayerState.cpp) and `HandleSocialRequest`
   (APBWorldGameMode.cpp) both contain the same op→UGI-bridge dispatch chain. Future refactor
   should extract into a shared helper.
2. **RPC op validation**: RPCs check length bounds but don't validate op names against a whitelist.
   Unknown ops are safely ignored (return "unknown_op") but explicit validation would be better.
3. **Mail payload encoding**: Uses pipe `|` separator for To|Subject|Body. Fragile if fields
   contain pipes. Future improvement: separate RPC parameters or JSON encoding.
4. **Gate script**: Not yet created. The `run_m14_social_gate.ps1` script needs to be written
   to run the 2-client social probe (alice on host, bob on joining client) and validate
   `SOCIAL_PROBE_ALICE_OK` + `SOCIAL_PROBE_BOB_OK` markers.
