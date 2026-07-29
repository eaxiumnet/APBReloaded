# M15 — Economy & Progression (Domain) — Handoff Note

**Author:** Qoder  **Date:** 2026-07-20  **Milestone:** M15 (brief #14)
**Status:** Progression/economy Domain layer COMPLETE + proven; `CharacterProgress`
persistence COMPLETE + proven (see "Persistence"); mission-completion rewards WIRED into the
gameplay loop; progression now exposed through `DomainSnapshot` (see "DomainSnapshot exposure").
UE-side replication/UMG remains.

## What landed

Pure-C++17 Domain service (no UE headers), unit-tested via `tests/build_and_run.ps1`.

- `Source/APBReloaded/Domain/APBProgression.h` / `.cpp`
- `tests/run_progression_tests.cpp` → wired as `$exe15` (`APBProgressionTests`).
- All 15 domain suites green (`FAILS=0`), including a parse of the real
  `Content/Data/contacts_lore.json` (472 lines) + `roles.json`.

## Relationship to existing work
- **Threat tiers / opposition / reward multipliers already live in `APBThreat.h`
  (`ThreatSystem`).** This service does NOT re-derive them — `ComputeMissionReward` *consumes*
  `ThreatTier.reward_multiplier` as an input. Keep that single source of truth.
- **Auction 5% fee (M12) is the existing cash sink.** `TrySpend` here is the generic
  balance-debit primitive for other sinks (kiosk purchases, vehicle spawns, respec).

## API surface (`namespace apb`)

### `ProgressionCatalog` (reference data)
- `LoadContactsFromText/File` (`contacts_lore.json`), `LoadRolesFromText/File` (`roles.json`).
  Additive, merge by id. Reuses `apb::JsonSplitObjects/JsonGetString` from `APBCatalog.cpp`
  (must be linked).
- `FindContact/FindRole`, `ContactsInDistrict(district)`, `ContactCount/RoleCount`.
- `ContactDef{id,title,district}` — `district` derived from the id prefix via
  `DistrictFromContactId` ("Financial_C1" -> "Financial"). `RoleDef{id,name}`.

### `LevelLadder` (contact standing -> level)
- `DefaultContactLadder()` — levels 0..15, cumulative thresholds 0/1000/3000/6000/...
  (increment grows by 1000 each level). **Tunable recreation default** — the seeded apbdb
  data does not publish per-level standing tables; retune when a retail table is recovered.
- `LevelFor(standing)`, `StandingForLevel(level)` (clamped), `MaxLevel()`.

### `CharacterProgress` (server-authoritative per-character state)
- Maps `contact_standing` + `role_xp`. `AddContactStanding/AddRoleXp` (ignore negatives,
  return new total), `ContactStanding/RoleXp`, `ContactLevel(id, ladder)`.
- Mirrors the replicated `AAPBPlayerState` progression fields — feed these from the world
  authority and replicate down.

### Rewards + unlocks
- `ComputeMissionReward(base_cash, base_standing, base_role_xp, threat_reward_multiplier)`
  → cash + standing scaled by the threat multiplier (round-nearest, never negative);
  **role XP is flat, NOT threat-scaled** (matches APB: role progression is per-action).
- `ContactUnlock{contact_id, required_level, item_id}`, `IsUnlocked(...)`,
  `UnlockedItems(...)` — contact-level-gated item availability for kiosks/Armas.
- `TrySpend(balance&, cost)` — generic cash-sink primitive (rejects negative cost /
  insufficient funds, debits on success).

## Integration points (for UE-side agents)
- On mission complete: **WIRED (Qoder)** — `WorldService::AdvanceMission` calls
  `ApplyMissionCompletionReward()` → `ComputeMissionReward(base_cash, base_standing, 0,
  threat.CurrentTier().reward_multiplier)` → adds cash to `character->cash` + standing via
  `AddContactStanding(mission->contact_id, …)`, then `PersistCharacter()`. Base payout is a
  tunable default scaled by stage count (no per-mission reward table parsed yet). Role/weapon
  XP is intentionally NOT granted here (APB awards it per weapon action/kill) — still a
  follow-up needing an equipped-weapon→role mapping on `OnHostileKill`/`FireWeapon`. UE side
  still needs to replicate the resulting standing/cash to `AAPBPlayerState`.
- Contact/kiosk UMG: gate purchasable items with `UnlockedItems` against the character's
  standing; charge with `TrySpend`.
- Persist `CharacterProgress` (contact_standing + role_xp maps) — **DONE**, see
  "Persistence" below.

## Persistence (Qoder, 2026-07-20)
`CharacterProgress` now round-trips through `JsonDomainStore` (`Domain/APBPersistence.{h,cpp}`)
via a per-character sidecar file, alongside accounts/characters/auction/mail:
- File: `characters/<account>_<slot>_progress.json` (account `Sanitize`d, same scheme as the
  character file). Kept separate from the character file so `SaveCharacter`'s signature is
  untouched (merge-friendly for parallel agents).
- API: `ProgressPath(account, slot)`, `HasProgress(account, slot)`,
  `SaveProgress(account, slot, const CharacterProgress&)`,
  `LoadProgress(account, slot, CharacterProgress&)`.
- Deterministic output: both `contact_standing` and `role_xp` are emitted sorted-by-key.
- Tolerate-missing: `LoadProgress` returns `false` on an absent/empty file and leaves the
  passed `CharacterProgress` untouched (fresh character = empty progress), matching the other
  loaders. `SaveProgress` is a safe no-op when the store is inactive.
- Linkage: `APBProgression.cpp` was added to `$srcs` in `tests/build_and_run.ps1` (it now
  links into $exe/$exe2/$exe3/$exe4). Its only extra dep, `APBCatalog.cpp`, was already there.
- Tests: `run_persistence_tests.cpp` "Instance E" — save→fresh-`JsonDomainStore`-load parity,
  unknown-key defaults-to-0, and tolerate-missing (unsaved slot → false, leaves empty). All
  17 suites `FAILS=0`.
- Wired in `WorldService` (Qoder): it owns a `CharacterProgress progress` member;
  `PersistCharacter`→`SaveProgress`, `TryLoadPersistedCharacter`→reset+`LoadProgress`
  (tolerate-missing), and `CreateCharacter`/`LogoutAccount` reset it. So progression
  round-trips automatically through the live service (proven by `run_persistence_tests.cpp`
  Instance A mutate → Instance B re-login restore). Still caller's job: the UE
  `FAPBPlayerService` bridge exercising this + replicating to `AAPBPlayerState`, and charging
  role/standing rewards from `ComputeMissionReward` on mission-complete.

## DomainSnapshot exposure (Qoder, 2026-07-21)
`CaptureSnapshot()` now surfaces progression so the UE HUD/`AAPBPlayerState` can reflect it
(previously the awarded standing/role XP was invisible to the single Domain→UI bridge):
- New `DomainSnapshot` fields: `contact_standings` + `role_xp` (both `std::vector<SnapshotProgressEntry{id,value}>`,
  only entries `> 0`, emitted **id-sorted** for deterministic sync), plus convenience
  `active_contact_id` / `active_contact_standing` / `active_contact_level` derived from the
  current mission's `contact_id` (level via `LevelLadder::DefaultContactLadder()`).
- Pure/deterministic — no new deps beyond `<algorithm>` for the sort. UE side maps the two
  vectors to `TArray<F...>` USTRUCTs on `FAPBDomainSnapshotUE` and replicates via
  `ApplyDomainSnapshot`; still the UE bridge's job (uncompilable by the Domain harness).
- Tests: `run_domain_tests.cpp` `TestDomainSnapshotParity` now asserts the two vectors are
  populated, id-sorted, value-correct, and `active_contact_id == mission->contact_id`.
  All 17 suites `FAILS=0`.

## NOT done here (deliberately out of Domain scope)
- Retail per-level standing table + per-contact unlock lists (ROLE unlocks, item reward
  tables) from `PlayerRoles.INT`/`ContactLevels.INT` — the ladder is a tunable default and
  the unlock list is caller-supplied until those INTs are parsed into `Content\Data\`.
- Contact UMG, kiosk UI, role-reward VFX/notifications.

## Build/verify
```
powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1
```
`$exe15 = APBProgressionTests` compiles `APBProgression.cpp + APBCatalog.cpp + run_progression_tests.cpp`.
