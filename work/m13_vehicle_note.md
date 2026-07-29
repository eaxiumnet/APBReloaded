# M13 — Vehicle Systems (Domain) — Handoff Note

**Author:** Qoder  **Date:** 2026-07-20  **Milestone:** M13 (brief #12, D15)
**Status:** Domain layer COMPLETE + proven. UE-side work remains (see "NOT done here").

## What landed

Pure-C++17 Domain service (no UE headers), unit-tested via `tests/build_and_run.ps1`.

- `Source/APBReloaded/Domain/APBVehicle.h` / `.cpp`
- `tests/run_vehicle_tests.cpp` → wired as `$exe14` (`APBVehicleTests`).
- All 14 domain suites green (`FAILS=0`), including a parse of the real
  `Content/Data/vehicles.json` (8627-line apbdb-seeded array).

## API surface (`namespace apb`)

### `VehicleCatalog`
- `bool LoadFromText(const std::string& json)` / `bool LoadFromFile(const std::string& path)`
  — parses the shipped `vehicles.json` array. Additive: entries merge by `id`. Returns true
  if ≥1 vehicle parsed. Reuses `apb::JsonSplitObjects/JsonGetString/JsonGetNumber` from
  `APBCatalog.cpp` (must be linked) + a local bare-bool parser for `criminal`/`enforcer`.
- `const VehicleDef* Find(id)`, `int32_t Count()`.
- `std::vector<const VehicleDef*> AvailableTo(Faction, int32_t rating)` — kiosk offer list
  (faction-eligible AND rating >= min_rating).

### `VehicleDef` (data-backed only — nothing invented)
`id, name, subcategory, infracategory, vclass, criminal, enforcer, min_rating,
max_speed, accel, max_health`. `EligibleFor(Faction)` → uses criminal/enforcer flags.
`VehicleClassFromSubcategory`: `VehicleCar→Car`, `VehicleTruck→Truck`, `VehicleVan→Van`,
`*motor*`/`*bike*→Motorcycle`, else `Unknown` (case-insensitive substring).

### `CanSpawnVehicle(cat, vehicle_id, faction, rating)` — kiosk spawn gate
Returns `VehicleSpawnResult{ok, error, vehicle_id}`. Errors:
`unknown_vehicle` / `wrong_faction` / `rating_too_low`. Pure rule, no side effects.

### `VehicleInstance` — runtime damage/handling state machine
- `FromDef(def)` → health = max_health.
- `State()`: `health<=0 → Destroyed`; `frac<=0.25 → Critical` (on fire);
  `frac<=0.60 → Damaged`; else `Pristine`. Thresholds `damaged_frac`/`critical_frac` are
  **tunable recreation defaults** (not from apbdb data) — retune when reference values found.
- `SpeedFactor()`: Pristine 1.0 / Damaged 0.9 / Critical 0.7 / Destroyed 0.0
  (handling degradation multiplier for max_speed/accel).
- `ApplyDamage(amount)` clamps at 0 (returns remaining health); `Repair(amount)` clamps at
  max_health; `Alive()`.

## Integration points (for UE-side agents)
- District pawn/kiosk should call `CanSpawnVehicle` before spawning; feed `AvailableTo` into
  the spawn-kiosk offer UMG list.
- Server-authoritative damage: route weapon/collision damage through
  `VehicleInstance::ApplyDamage` on the world/district authority; replicate `State()` to
  drive VFX (smoke at Damaged, fire at Critical, explosion at Destroyed).
- `max_speed`/`accel` are the apbdb base stats; apply `SpeedFactor()` as the live multiplier.

## NOT done here (deliberately out of Domain scope)
- UE spawn kiosks + interaction, spawn placement, ownership/despawn lifecycle.
- Paint grids / deep customization (paint/parts/kits) from retail
  `Colours\StandardPaint.ini` / `PearlescentPaint.ini` — its own sub-milestone.
- Physics/driving model, collision, damage-state VFX/audio.
- Persistence of owned/customized vehicles across district rejoin (needs an
  `APBPersistence` block + owned-vehicle records — not yet added; kept out to avoid
  colliding with the contested persistence file).

## Build/verify
```
powershell -NoProfile -ExecutionPolicy Bypass -File D:\APBReloaded\tests\build_and_run.ps1
```
`$exe14 = APBVehicleTests` compiles `APBVehicle.cpp + APBCatalog.cpp + run_vehicle_tests.cpp`.
