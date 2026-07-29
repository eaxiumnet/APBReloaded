# M14 — Domain `GroupService` (mission group / party) — landed early

Author: GPT-5.8 agent · 2026-07-20
Status: Domain core DONE + fully unit-tested. Complements the M7 `ChatService`
Group channel. UI / relay / persistence layers still open (see "Remaining" below).

## What landed

| File | Purpose |
|---|---|
| `Source/APBReloaded/Domain/APBGroup.h` | `GroupService` API + `Group`/`GroupMember`/`GroupResult` |
| `Source/APBReloaded/Domain/APBGroup.cpp` | Implementation (pure C++17, no UE/platform deps) |
| `tests/run_group_tests.cpp` | 11 test groups (~50 assertions), all green |
| `tests/build_and_run.ps1` | wired 6th suite `APBGroupTests` (`$exe6`) |

Verified: `powershell -File tests\build_and_run.ps1` → all 6 suites `FAILS=0`, exit 0
(Domain, Persistence, Fidelity, Auth, Chat, **Group**).

## Semantics (1:1 with APB)

- Up to **4 players** per group (`default_max_size=4`, matches `MissionScriptDef.group_max`).
- One **leader**; only the leader may invite / kick / disband / transfer / assign mission.
- **Invite forms a group**: inviting while solo auto-creates a group with the inviter as leader.
- **Accept clears** all of the invitee's other pending invites; a player is in ≤1 group.
- **Leave succession**: if the leader leaves and members remain, leadership passes to the
  earliest-joined remaining member; an emptied group is disbanded.
- **Kick** cannot target self (leader uses Leave/Disband/Transfer). **Disband** clears all
  memberships and purges the group's dangling invites.
- **Ready state** per member + `AllReady(group_id)` — the ready-up gate for the shared
  mission queue. `AssignMission`/`ClearMission` store a single `mission_id` (M11 hook).
- Group ids are deterministic `GRP-<n>` for reproducible tests.

## Integration seam (kept decoupled)

`GroupService` does NOT depend on `ChatService` (services stay independent Domain units).
The UE layer wires them: on group membership change, call
`ChatService::SetGroup(player, GroupService::GroupOf(player))` so the `ChatChannel::Group`
route matches exactly the group's members. Same pattern the Clan/Faction channels will use.

## Remaining M14 work (NOT done here)

1. **`ClanService`** — the existing `SocialService` clan (`APBSocial.h`) is a single-clan
   stub (create/invite only). M14 wants clan **ranks + promote/demote** and multi-clan.
   Recommend a dedicated `APBClan.{h,cpp}` mirroring this GroupService shape rather than
   growing the SocialService stub (avoids collision if another agent is in APBSocial.h).
2. **Persistence** — `clans.json` / `social.json` load/save (groups are ephemeral, no persist).
3. **UMG social panel** — friends/clan/group UI.
4. **World relay presence** — friends online/offline via the M7 W↔D relay.
5. **UE bridge + 2-client probe** — expose group RPCs through the district GM and prove
   invite→accept→shared-mission-queue across two clients (the M14 acceptance gate).

Do NOT re-create `APBGroup.*` — extend it. If you add clan ranks, keep the `GroupResult`
enum style and header/cpp split for consistency with Chat/Group/Ticket domains.
