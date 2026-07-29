# M14 — `SocialStore` (file-backed clans.json / friends.json) — landed

Author: GPT-5.8 agent · 2026-07-20
Status: On-disk persistence layer DONE + unit-tested. Completes the "thin file wrapper"
item that both `m14_clan_service_note.md` and `m14_friends_service_note.md` flagged as
remaining. The M14 social **Domain** stack (Group/Clan/Friends cores + on-disk store) is
now complete; only UI (UMG social panel) and the UE bridge + 2-client probe remain.

## What landed

| File | Purpose |
|---|---|
| `Source/APBReloaded/Domain/APBSocialStore.h` | `SocialStore` API (Init + Save/Load clans & friends) |
| `Source/APBReloaded/Domain/APBSocialStore.cpp` | `std::filesystem`/`fstream` IO delegating to `SaveJson`/`LoadJson` |
| `tests/run_social_store_tests.cpp` | 5 test groups (~30 assertions), all green |
| `tests/build_and_run.ps1` | wired 9th suite `APBSocialStoreTests` (`$exe9`) |

Verified: `powershell -File tests\build_and_run.ps1` → all **9** suites `FAILS=0`, exit 0
(Domain, Persistence, Fidelity, Auth, Chat, Group, Clan, Friend, **SocialStore**).

## Design

`SocialStore` is a thin filesystem wrapper. It does **only** directory + file IO and
delegates ALL serialization to the pure `ClanService::SaveJson/LoadJson` and
`FriendsService::SaveJson/LoadJson` round-trips — so the Domain stays platform-free and
this unit stays trivial.

- **Deliberately SEPARATE from `JsonDomainStore`/`APBPersistence`.** Its own translation
  unit means the shared `$srcs` list (`APBPersistence.cpp`, linked into 4 test suites) is
  untouched → no re-link ripple, merge-friendly for parallel agents.
- Layout under the configured dir: `<dir>/clans.json`, `<dir>/friends.json`.
- `Init(dir)` creates the directory (`create_directories`) and activates the store.
  Empty dir → inactive/`false`. All Save/Load are guarded on `active_`.
- **Missing/empty file tolerance**: `LoadClans`/`LoadFriends` return `false` (fresh start,
  not an error) rather than throwing, so callers distinguish "nothing loaded" from a
  populated load. Writes use truncate mode (overwrite is clean, old content gone).

## API

```cpp
bool Init(const std::string& dir);          // create+activate; ""→false
bool IsActive() const;
std::string ClansPath() const;              // <dir>/clans.json
std::string FriendsPath() const;            // <dir>/friends.json
bool SaveClans(const ClanService&) const;
bool LoadClans(ClanService&) const;         // false if missing/empty/no clans
bool SaveFriends(const FriendsService&) const;
bool LoadFriends(FriendsService&) const;    // false if missing/empty
```

## Tests (`run_social_store_tests.cpp`)

Uses a `ScratchDir` RAII helper under `std::filesystem::temp_directory_path()` (auto
`remove_all` on scope exit). Suite links `APBSocialStore.cpp + APBClan.cpp + APBFriends.cpp`.
1. **Init + paths** — `Init("")`→false, inactive-store Save/Load→false, dir created, path layout.
2. **Missing-file tolerance** — Load before any write → false, services untouched.
3. **Clans round-trip** — create/invite/accept/motd → save → load fresh → members, leader,
   size, motd, faction, name/tag restored.
4. **Friends round-trip** — request/accept + ignore → save → load fresh → symmetric
   friendship + directional ignore restored; presence NOT persisted.
5. **Overwrite + idempotency** — second save replaces the file (old clan gone); save/load/save
   reproduces identical bytes.

## Integration seam (for the UE/store layer)

Wire `SocialStore` from the district/world server subsystem: `Init(<SavedDir>/Social)` at
startup, `LoadClans`/`LoadFriends` into the live `ClanService`/`FriendsService`, and call
`SaveClans`/`SaveFriends` on mutations (or a periodic flush). Presence is transient — fed by
the M7 W↔D relay via `FriendsService::SetOnline`, never persisted.

## Remaining M14 work (NOT done here)

1. **UMG social panel** — friends/clan/group UI (D10 deliverable) reading these services.
2. **UE bridge + 2-client probe** — expose group/clan/friend RPCs through the district GM and
   prove invite→accept + presence across two clients (the M14 acceptance gate).

Do NOT re-create `APBSocialStore.*` / `APBClan.*` / `APBFriends.*` — extend them.
