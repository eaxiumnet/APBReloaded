# APB:Db data source

Community database: [https://apbdb.com/](https://apbdb.com/)

> Items, progression, districts, missions, heat/notoriety, contacts (lore), and more — kept up to date for the community.

## API

The site is a React SPA. JSON lives on:

| Purpose | URL |
|--------|-----|
| Base | `https://api.apbdb.com/beacon` |
| Version | `GET /version` |
| Weapons list | `GET /items?cat=Weapon&page=N` (792 items / 32 pages) |
| Vehicles list | `GET /items?cat=Vehicle&page=N` |
| Item detail (gun stats) | `GET /items/{sAPBDB}` e.g. `/items/Weapon_Rifle_Oscar` |
| Ammo | `GET /items/ammo` |
| Heat / notoriety / prestige | `GET /heat` |
| Districts | `GET /districts` |
| Missions | `GET /missions?page=1` |
| Contacts (lore) | `GET /contacts` |
| Roles / medals | `GET /roles` |
| Slash commands | `GET /commands` |

CDN icons: `https://cdn.apbdb.com/img/...`

## Weapon stat path

In item detail JSON:

```
detail.eWeaponTypeLink.eWeaponType_0
  fHealthDamage, fStaminaDamage
  fHardDamageModifier, fSoftDamageModifier
  nMagazineCapacity, nAmmoPoolCapacity
  fFireInterval  → RPM = 60 / fFireInterval
  fBurstInterval, nBurstShots
  fReloadTime, fEquipTime
  fUIHardDamageLevel, fUISoftDamageLevel, fUIRateOfFire, fUIRangeLevel, fUIStunDamageLevel
```

## Sync into this project

```powershell
python D:\APBReloaded\tools\apbdb\sync_apbdb.py
python D:\APBReloaded\tools\apbdb\sync_apbdb.py --full-weapons   # all non-Armas (~404 details)
python D:\APBReloaded\tools\apbdb\sync_apbdb.py --all-weapons    # every weapon detail
```

Writes:

- `Content/Data/threat_table.json` — notoriety/prestige thresholds + multipliers from `/heat`
- `Content/Data/weapons.json` — weapons with combat stats (detail-fetched set)
- `Content/Data/weapons_catalog.json` — full catalog (+ stats when available)
- `Content/Data/vehicles.json` / `vehicles_catalog.json`
- `Content/Data/districts.json`, `missions.json`, `contacts_lore.json`
- `Content/Data/apbdb_meta.json` — versions + endpoint map
- `tools/apbdb/raw/*` — raw API payloads for offline use

## Heat thresholds (live sample)

Criminal notoriety (points → level): 0 / 150 / 750 / 1500 / 4000 / 7000  
Enforcer prestige: 0 / 1200 / 1500 / 2500 / 5500 / 10000  

Level 5 unlocks citywide opposing-faction PVP (`bPVPUnlockedToAllOpposingFaction`).

## Districts (joinable)

Financial, FinancialChaos, PGAsylum (Abington Towers), PGBeacon, PGCrate, Social (Breakwater Marina), Waterfront, FinancialRiot.
