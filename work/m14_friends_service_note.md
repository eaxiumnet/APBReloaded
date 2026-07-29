# M14 — Domain `FriendsService` (friends + ignore/block + presence) — landed

Author: GPT-5.8 agent · 2026-07-20
Status: Domain core DONE + fully unit-tested. Completes the M14 **Domain** trio
(`GroupService`, `ClanService`, `FriendsService`). UI / relay wiring / file-store still open.

## What landed

| File | Purpose |
|---|---|
| `Source/APBReloaded/Domain/APBFriends.h` | `FriendsService` API + `FriendResult` (15 values) |
| `Source/APBReloaded/Domain/APBFriends.cpp` | Implementation + JSON persistence (pure C++17, zero deps) |
| `tests/run_friend_tests.cpp` | 11 test groups (~70 assertions), all green |
| `tests/build_and_run.ps1` | wired 8th suite `APBFriendTests` (`$exe8`) |

Verified: `powershell -File tests\build_and_run.ps1` → all **8** suites `FAILS=0`, exit 0
(Domain, Persistence, Fidelity, Auth, Chat, Group, Clan, **Friend**).

## Semantics (grounded 1:1 on retail INT strings)

- **Mutual, invite-based friendship**: `SendRequest` → `AcceptRequest`/`DeclineRequest`.
  A reciprocal request auto-accepts into a friendship. Removal is **mutual** (`RemoveFriend`
  drops both sides) — retail `<character>_removed_you_from_friend_list`.
- **Caps**: `max_friends` (default 50) on BOTH sender (`FriendsListFull`,
  `Your_friends_list_is_full`) and recipient (`TargetListFull`,
  `<character>_has_a_full_friend_list`). `max_ignores` (default 50) for the ignore list.
- **Guards**: no self, no duplicate invite (`AlreadyInvited`, `You_already_invited_<character>`),
  already-friends (`<character>_is_already_in_your_friends_list`).
- **Ignore (block) list** — retail "Ignore": `Ignore`/`Unignore`, cap, already-ignored.
- **Two mutual-exclusion invariants** (retail-exact):
  * cannot friend someone you ignore → `YouIgnoreTarget`
    (`You_are_currently_ignoring_<character>… remove from Ignore list first`)
  * cannot send to someone who ignores you → `TargetIgnoresYou`
    (`<character>_could_not_receive_your_friend_invite`)
  * cannot ignore an existing friend → `TargetIsFriend`
    (`You_are_currently_friends_with_<character>… remove from friends list first`)
  Adding to the ignore list also cancels any pending request between the two.
- **Presence** (`SetOnline`, `IsOnline`, `OnlineFriendsOf`) is **transient** session state fed
  by the M7 W↔D relay — NOT persisted.

## Persistence (pure string round-trip)

`SaveJson()` / `LoadJson(text)`. Schema `friends.json`:
```
{ "friends": [ { "player":"a", "list":["b","c"] } ],
  "ignores": [ { "player":"a", "list":["d"] } ] }
```
- Durable: friendships (stored/emitted symmetrically) + ignore lists. Pending requests +
  presence are transient and NOT persisted; `LoadJson` clears them.
- `LoadJson` rebuilds friendships symmetrically (defensive: adds both directions even if a
  file only listed one side). Idempotent (save/load/save reproduces the document).
- Self-contained JSON helpers (internal linkage) — same rationale as APBClan: no edit to the
  shared `$srcs` (APBPersistence.cpp), no re-link ripple across the other suites.

## Integration seam (kept decoupled)

`FriendsService` is a standalone Domain unit. The UE layer feeds presence from the M7 world
relay by calling `SetOnline(player, true/false)` on district enter/leave and login/logout,
then the social panel reads `OnlineFriendsOf`. Friends have no dedicated chat channel.

## Relationship to `APBSocial.h` `SocialService`

`SocialService::AddFriend` is a trivial append-only stub (no requests, no removal, no ignore,
no presence). `FriendsService` is the full replacement for the friends half of M14. Route
friend RPCs here. The `SocialService` MAIL parts (`MailService`) are unrelated — leave alone.

## Remaining M14 work (NOT done here)

The three social **Domain** cores (Group, Clan, Friends) are now complete + tested. Left:
1. **Thin file wrappers** — DONE: `SocialStore::SaveClans/LoadClans` (`clans.json`) and
   `SaveFriends/LoadFriends` (`friends.json`) call the respective `SaveJson`/`LoadJson`
   (`Source/APBReloaded/Domain/APBSocialStore.{h,cpp}`, see `work/m14_social_store_note.md`).
2. **UMG social panel** — friends/clan/group UI (D10 deliverable) reading these services.
3. **UE bridge + 2-client probe** — expose group/clan/friend RPCs through the district GM and
   prove invite→accept flows + presence across two clients (the M14 acceptance gate).

Do NOT re-create `APBFriends.*` / `APBClan.*` / `APBGroup.*` — extend them. Keep the
`*Result` enum + header/cpp split style for consistency across the Domain services.
