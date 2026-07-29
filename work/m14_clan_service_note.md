# M14 — Domain `ClanService` (clans, ranks, permissions) — landed

Author: GPT-5.8 agent · 2026-07-20
Status: Domain core DONE + fully unit-tested. Sibling to `GroupService` (party) and
the M7 `ChatService` Clan channel. UI / relay / persistence layers still open.

## What landed

| File | Purpose |
|---|---|
| `Source/APBReloaded/Domain/APBClan.h` | `ClanService` API + `Clan`/`ClanRank`/`ClanMember`/`ClanResult` + `ClanPerm` bitmask |
| `Source/APBReloaded/Domain/APBClan.cpp` | Implementation (pure C++17, only depends on `APBTypes.h` for `Faction`) |
| `tests/run_clan_tests.cpp` | 12 test groups (109 assertions), all green |
| `tests/build_and_run.ps1` | wired 7th suite `APBClanTests` (`$exe7`) |

Verified: `powershell -File tests\build_and_run.ps1` → all **7** suites `FAILS=0`, exit 0
(Domain, Persistence, Fidelity, Auth, Chat, Group, **Clan**).

## Semantics (grounded 1:1 on retail INT strings)

- **Single leader**, implicitly holds `ClanPerm::All`. Seeded at rank 0.
- **Leader cannot `Leave()`** → returns `LeaderMustTransferOrDisband` (retail `ClanLeaveLeaderFail`).
  Must `TransferLeader` or `Disband` first.
- **Permission-gated invites** (`CannotTeamInvite_NoPermission`): `Invite`/`Kick`/`SetMotd`/
  `AddRank`/`SetMemberRank` require the matching `ClanPerm` on the actor's rank.
- **Single-faction clans** (`SendTeamInviteFailedRecipientWrongFaction`): `Invite` rejects
  `invitee_faction != clan.faction` with `WrongFaction`.
- **One clan per character** (`<character>_is_already_in_a_clan`): `player_clan_` map enforces it.
- **Name + tag uniqueness** is case-insensitive (`NameOrTagTaken`).
- **Accept takes most-recent still-valid invite** and clears ALL other pending invites (skips
  vanished clans); a joiner enters at the lowest rank.
- **Kick**: cannot target self or leader; a non-leader may only kick STRICTLY lower rank
  (higher `rank_index`). Leader (perms=All) can kick anyone but the leader.
- **TransferLeader**: new leader → rank 0, old leader stays as rank-0 member.
- **Disband** clears all memberships and purges the clan's dangling invites.
- **MOTD** per clan (`Clan_message_of_the_day`); **named ranks** (`ClanRank`).
- Default rank ladder: `Officer` (Invite|Kick|EditMotd) at index 0, `Member` (None) at index 1.
  `ManageRanks` is leader-only by default (only the All-holder has it) until a rank grants it.

## Integration seam (kept decoupled)

`ClanService` does NOT depend on `ChatService` (services stay independent Domain units).
The UE layer wires them: on clan membership change, call
`ChatService::SetClan(player, ClanService::ClanOf(player))` so the `ChatChannel::Clan`
route matches the clan's members exactly. Same pattern GroupService uses for `SetGroup`.

## Relationship to `APBSocial.h` `SocialService`

`SocialService` has a single-clan create/invite stub (no ranks, no multi-clan). `ClanService`
is the full replacement for the clan half of M14. When the UE bridge is built, route clan RPCs
to `ClanService`, not the `SocialService` stub. Leave the `SocialService` friends/mail parts
alone — those are still the friends/mail source.

## Persistence (landed)

`ClanService::SaveJson()` / `LoadJson(text)` — pure string round-trip, no filesystem
(keeps Domain pure like Chat/Group). Schema `clans.json`:
```
{ "next_join_seq": <n>,
  "clans": [ { "id","name","tag","faction","leader","motd",
               "ranks":   [ { "name","perms":<u32> } ],
               "members": [ { "player","rank":<idx>,"seq":<i64> } ] } ] }
```
- Durable state only: clans + ranks + members + `next_join_seq_`. Pending **invites are
  transient and intentionally NOT persisted**.
- `LoadJson` is authoritative: clears state, rebuilds the `player->clan` index from member
  lists, restores `next_join_seq_` so join ordering stays monotonic across reloads.
- Self-contained JSON helpers (internal linkage) mirror the `APBPersistence.cpp` reader, so
  APBClan stays a standalone unit — **no edit to the shared `$srcs` (APBPersistence.cpp)** and
  no forced re-link ripple across the other 6 suites.
- Round-trip is idempotent (save/load/save reproduces the document); escaped chars (`"`, `\`)
  round-trip. Covered by `run_clan_tests.cpp` Test 12 (22 assertions).
- File wrapper DONE: `SocialStore::SaveClans/LoadClans` read/write `<dir>/clans.json` via
  `SaveJson`/`LoadJson` (`Source/APBReloaded/Domain/APBSocialStore.{h,cpp}`, see
  `work/m14_social_store_note.md`).

## Remaining M14 work (NOT done here)

1. **Friends** — DONE: `FriendsService` (mutual requests, ignore/block, presence, JSON) —
   `Source/APBReloaded/Domain/APBFriends.{h,cpp}`, see `work/m14_friends_service_note.md`.
2. **Persistence** — DONE end to end: clan JSON serialize/deserialize (`SaveJson`/`LoadJson`)
   plus the `SocialStore` file wrapper for `clans.json`/`friends.json` (see
   `work/m14_social_store_note.md`). Groups are ephemeral (no persist).
3. **UMG social panel** — friends/clan/group UI (D10 deliverable).
4. **UE bridge + 2-client probe** — expose clan RPCs through the district GM and prove
   create → invite → accept → promote across two clients (the M14 acceptance gate).

Do NOT re-create `APBClan.*` or `APBGroup.*` — extend them. Keep the `ClanResult` enum style
and header/cpp split for consistency with Chat/Group/Ticket/Clan domains.
