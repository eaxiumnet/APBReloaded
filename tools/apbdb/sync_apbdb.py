#!/usr/bin/env python3
"""
Sync gameplay data from the public APB:Db API (https://apbdb.com / https://api.apbdb.com/beacon)
into D:/APBReloaded/Content/Data and tools/apbdb/raw.

Usage:
  python tools/apbdb/sync_apbdb.py                  # catalogs + heat + districts + core weapon details
  python tools/apbdb/sync_apbdb.py --full-weapons   # detail-fetch all non-Armas weapons (~404)
  python tools/apbdb/sync_apbdb.py --all-weapons    # detail-fetch every weapon (~792)
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

API = "https://api.apbdb.com/beacon"
SITE = "https://apbdb.com"
UA = "APBReloaded-OfflineRecreate/1.0 (private; data from apbdb.com community API)"

ROOT = Path(__file__).resolve().parents[2]
RAW = Path(__file__).resolve().parent / "raw"
CONTENT = ROOT / "Content" / "Data"


def http_get_json(path: str, params: dict[str, Any] | None = None, retries: int = 4) -> Any:
    q = f"?{urllib.parse.urlencode(params)}" if params else ""
    url = f"{API}{path}{q}"
    last: Exception | None = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"Accept": "application/json", "User-Agent": UA})
            with urllib.request.urlopen(req, timeout=90) as resp:
                return json.loads(resp.read().decode("utf-8"))
        except Exception as e:  # noqa: BLE001
            last = e
            time.sleep(0.6 * (attempt + 1))
    raise RuntimeError(f"GET failed {url}: {last}")


def write_json(path: Path, obj: Any) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(obj, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"wrote {path} ({path.stat().st_size} bytes)")


def strip_color(name: str) -> str:
    return re.sub(r"<[^>]+>", "", name or "").strip()


def paginate_items(cat: str) -> list[dict[str, Any]]:
    page = 1
    items: list[dict[str, Any]] = []
    total_pages = 1
    while page <= total_pages:
        data = http_get_json("/items", {"cat": cat, "page": page})
        if page == 1:
            total_pages = int(data.get("totalPages") or 1)
            print(f"{cat}: total={data.get('total')} pages={total_pages}")
        items.extend(data.get("items") or [])
        page += 1
        time.sleep(0.05)
    return items


def weapon_type_stats(detail: dict[str, Any]) -> dict[str, Any]:
    link = (detail.get("detail") or {}).get("eWeaponTypeLink") or {}
    wt = link.get("eWeaponType_0") or {}
    if not isinstance(wt, dict):
        return {}
    fire_interval = float(wt.get("fFireInterval") or 0.0)
    rpm = (60.0 / fire_interval) if fire_interval > 0 else 0.0
    # UI rate of fire is a 0-10 bar, not RPM
    projectile = wt.get("eWeaponProjectile") or {}
    max_range = 100.0
    if isinstance(projectile, dict):
        # keep placeholder; projectile may be id only
        pass
    return {
        "weapon_type_id": wt.get("sAPBDB"),
        "damage": float(wt.get("fHealthDamage") or 0.0),
        "stamina_damage": float(wt.get("fStaminaDamage") or 0.0),
        "hard_damage_mod": float(wt.get("fHardDamageModifier") or 0.0),
        "soft_damage_mod": float(wt.get("fSoftDamageModifier") or 0.0),
        "clip": int(wt.get("nMagazineCapacity") or 0),
        "ammo_pool": int(wt.get("nAmmoPoolCapacity") or 0),
        "fire_interval": fire_interval,
        "rpm": round(rpm, 1) if rpm else 0.0,
        "burst_shots": int(wt.get("nBurstShots") or 0),
        "burst_interval": float(wt.get("fBurstInterval") or 0.0),
        "reload_time": float(wt.get("fReloadTime") or 0.0),
        "equip_time": float(wt.get("fEquipTime") or 0.0),
        "ui_hard": float(wt.get("fUIHardDamageLevel") or 0.0),
        "ui_soft": float(wt.get("fUISoftDamageLevel") or 0.0),
        "ui_rof": float(wt.get("fUIRateOfFire") or 0.0),
        "ui_stun": float(wt.get("fUIStunDamageLevel") or 0.0),
        "ui_range": float(wt.get("fUIRangeLevel") or 0.0),
        "less_lethal": bool(wt.get("bLessLethal")),
        "max_range": max_range,
    }


def normalize_weapon(list_item: dict[str, Any], detail: dict[str, Any] | None = None) -> dict[str, Any]:
    stats = weapon_type_stats(detail) if detail else {}
    price = list_item.get("eDefaultPrice") or {}
    cash = price.get("nCostAPBCash") if isinstance(price, dict) else None
    out: dict[str, Any] = {
        "id": list_item.get("sAPBDB"),
        "name": strip_color(list_item.get("sDisplayName") or ""),
        "category": list_item.get("category") or "Weapon",
        "subcategory": list_item.get("subcategory"),
        "infracategory": list_item.get("infracategory"),
        "criminal": bool(list_item.get("bCriminal")),
        "enforcer": bool(list_item.get("bEnforcer")),
        "min_rating": int(list_item.get("nMinRating") or 0),
        "is_armas": bool(list_item.get("bIsArmas")),
        "creator": list_item.get("sCreatorName"),
        "cash_price": cash,
        "hud_image": list_item.get("eHUDImage"),
        "source": f"{SITE}/items/{list_item.get('sAPBDB')}",
    }
    out.update(stats)
    if detail:
        d = detail.get("detail") or {}
        out["description"] = d.get("sDescription")
    return out


def bot_count_for_multiplier(mult: float) -> int:
    # Domain ThreatSystem uses bot_count; map reward multiplier to opposition pressure.
    if mult <= 0.5:
        return 2
    if mult <= 0.9:
        return 3
    if mult <= 1.0:
        return 4
    if mult <= 1.25:
        return 5
    if mult <= 1.5:
        return 6
    if mult <= 1.75:
        return 7
    return 9


def build_threat_table(heat: list[dict[str, Any]]) -> dict[str, Any]:
    noto = []
    prest = []
    for h in heat:
        name = h.get("sAPBDB") or ""
        if name.startswith("NotorietyLevel"):
            lvl = int(h.get("nLevel") or 0)
            mult = float(h.get("fRewardMultiplier") or 1.0)
            noto.append(
                {
                    "level": lvl,
                    "name": name,
                    "threshold": float(h.get("fThreshold") or 0.0),
                    "reward_multiplier": mult,
                    "opposition_multiplier": mult if mult > 0 else 0.6,
                    "bot_count": bot_count_for_multiplier(mult),
                    "citywide_pvp": bool(h.get("bPVPUnlockedToAllOpposingFaction")),
                    "dispatch_mission": bool(h.get("bDispatchMission")),
                    "description": h.get("sDescription"),
                    "sAPBDB": name,
                }
            )
        elif name.startswith("PrestigeLevel"):
            lvl = int(h.get("nLevel") or 0)
            mult = float(h.get("fRewardMultiplier") or 1.0)
            prest.append(
                {
                    "level": lvl,
                    "name": name,
                    "threshold": float(h.get("fThreshold") or 0.0),
                    "reward_multiplier": mult,
                    "opposition_multiplier": mult if mult > 0 else 0.6,
                    "bot_count": bot_count_for_multiplier(mult),
                    "citywide_pvp": bool(h.get("bPVPUnlockedToAllOpposingFaction")),
                    "dispatch_mission": bool(h.get("bDispatchMission")),
                    "description": h.get("sDescription"),
                    "sAPBDB": name,
                }
            )
    noto.sort(key=lambda x: x["level"])
    prest.sort(key=lambda x: x["level"])
    return {
        "source": f"{API}/heat",
        "fetched_at": datetime.now(timezone.utc).isoformat(),
        "criminal_notoriety": noto,
        "enforcer_prestige": prest,
        "apbdb_heat_raw": heat,
    }


def map_for_district(sapbdb: str) -> str:
    return f"Lvl_APB_{sapbdb}_Freeroam"


def build_districts(raw: list[dict[str, Any]]) -> list[dict[str, Any]]:
    skip = {"None", "LCTestMap", "SimpleGameplayLevel", "SimpleUILevel", "UIDistrict"}
    out = []
    for d in raw:
        sid = d.get("sAPBDB") or ""
        if not sid or sid in skip or str(d.get("sDisplayName", "")).startswith("DNT"):
            continue
        out.append(
            {
                "id": sid,
                "numeric_id": int(d.get("id") if d.get("id") is not None else d.get("eDistrict") or 0),
                "name": d.get("sDisplayName"),
                "map": map_for_district(sid),
                "source": f"{SITE}/districts",
                "joinable": True,
                "max_players": 64,
                "audio_banks": d.get("sAudioBanks"),
            }
        )
    return out


def normalize_vehicle(item: dict[str, Any], detail: dict[str, Any] | None = None) -> dict[str, Any]:
    out: dict[str, Any] = {
        "id": item.get("sAPBDB"),
        "name": strip_color(item.get("sDisplayName") or ""),
        "category": "Vehicle",
        "subcategory": item.get("subcategory"),
        "infracategory": item.get("infracategory"),
        "criminal": bool(item.get("bCriminal")),
        "enforcer": bool(item.get("bEnforcer")),
        "min_rating": int(item.get("nMinRating") or 0),
        "is_armas": bool(item.get("bIsArmas")),
        "source": f"{SITE}/items/{item.get('sAPBDB')}",
    }
    if detail:
        # Vehicle stats live under nested type links; keep description when present.
        d = detail.get("detail") or {}
        out["description"] = d.get("sDescription")
        # Best-effort numeric fields if present on any nested blob
        blob = json.dumps(detail)
        for key, out_key, cast in (
            ("fMaxSpeed", "max_speed", float),
            ("fTopSpeed", "max_speed", float),
            ("fAcceleration", "accel", float),
            ("nHealth", "health", float),
            ("fHealth", "health", float),
        ):
            m = re.search(rf'"{key}"\s*:\s*([0-9.]+)', blob)
            if m and out_key not in out:
                out[out_key] = cast(m.group(1))
    # Domain defaults when missing
    out.setdefault("max_speed", 21.0)
    out.setdefault("accel", 14.0)
    out.setdefault("health", 800.0)
    return out


def normalize_mission(m: dict[str, Any]) -> dict[str, Any]:
    # Mission schema varies; keep stable Domain-facing fields.
    stages = []
    for i, st in enumerate(m.get("aStages") or m.get("stages") or []):
        if isinstance(st, dict):
            stages.append(
                {
                    "index": i,
                    "type": st.get("sType") or st.get("type") or "objective",
                    "name": st.get("sDisplayName") or st.get("name") or f"Stage {i + 1}",
                    "target_progress": float(st.get("fTarget") or st.get("target_progress") or 1.0),
                }
            )
    if not stages:
        # Generic multi-stage opposition shell used by freeroam probes
        for i, t in enumerate(["contact", "travel", "objective", "defend", "extract"]):
            stages.append({"index": i, "type": t, "name": f"Stage {i + 1}", "target_progress": 1.0 if t != "defend" else 5.0})
    return {
        "id": m.get("sAPBDB") or m.get("id"),
        "title": m.get("sDisplayName") or m.get("title") or m.get("sAPBDB"),
        "faction": m.get("eFaction") if m.get("eFaction") is not None else m.get("faction", 0),
        "group_min": int(m.get("nMinGroupSize") or m.get("group_min") or 1),
        "group_max": int(m.get("nMaxGroupSize") or m.get("group_max") or 4),
        "takeout_count": int(m.get("nTakeoutCount") or m.get("takeout_count") or 8),
        "owning_side_bias": float(m.get("fOwningSideBias") or m.get("owning_side_bias") or 1.0),
        "opposition_on_takeouts": bool(m.get("bOppositionOnTakeouts", True)),
        "description": m.get("sDescription"),
        "stages": stages,
        "source": f"{SITE}/missions/{m.get('sAPBDB')}",
    }


def fetch_weapon_details(
    catalog: list[dict[str, Any]],
    *,
    mode: str,
    limit: int | None = None,
) -> dict[str, dict[str, Any]]:
    chosen = []
    for it in catalog:
        if mode == "core":
            # Prefer non-Armas; skip heavy skins
            if it.get("bIsArmas"):
                continue
            chosen.append(it)
        elif mode == "full":
            if it.get("bIsArmas"):
                continue
            chosen.append(it)
        else:  # all
            chosen.append(it)
    if mode == "core":
        # representative set: one per infracategory + known staples
        by_infra: dict[str, list] = {}
        for it in chosen:
            by_infra.setdefault(it.get("infracategory") or "?", []).append(it)
        staples = {
            "Weapon_Rifle_Oscar",
            "Weapon_Pistol_SASPDW",
            "Weapon_SMG_OCA",
            "Weapon_Pistol_FBW",
            "Weapon_AssaultRifle_FAR_Base",
            "Weapon_Shotgun_CSG_Joker",
            "Weapon_SniperRifle_HVR243_Joker",
            "Weapon_AssaultRifle_NTEC-5",
            "Weapon_SMG_N-HVR",
        }
        pick = []
        seen = set()
        for sid in staples:
            for it in catalog:
                if it.get("sAPBDB") == sid and sid not in seen:
                    pick.append(it)
                    seen.add(sid)
        for infra, arr in by_infra.items():
            # up to 3 non-armas per infracategory for variety
            for it in arr[:3]:
                sid = it.get("sAPBDB")
                if sid not in seen:
                    pick.append(it)
                    seen.add(sid)
        chosen = pick
    if limit:
        chosen = chosen[:limit]

    details: dict[str, dict[str, Any]] = {}
    print(f"detail-fetching {len(chosen)} weapons (mode={mode})...")
    for i, it in enumerate(chosen, 1):
        sid = it.get("sAPBDB")
        if not sid:
            continue
        try:
            details[sid] = http_get_json(f"/items/{sid}")
        except Exception as e:  # noqa: BLE001
            print(f"  fail {sid}: {e}")
        if i % 25 == 0:
            print(f"  {i}/{len(chosen)}")
            time.sleep(0.15)
        else:
            time.sleep(0.04)
    return details


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--full-weapons", action="store_true", help="Detail all non-Armas weapons")
    ap.add_argument("--all-weapons", action="store_true", help="Detail every weapon")
    ap.add_argument("--limit", type=int, default=None)
    ap.add_argument("--skip-details", action="store_true")
    args = ap.parse_args()

    RAW.mkdir(parents=True, exist_ok=True)
    CONTENT.mkdir(parents=True, exist_ok=True)

    version = http_get_json("/version")
    write_json(RAW / "version.json", version)

    heat = http_get_json("/heat")
    write_json(RAW / "heat.json", heat)
    threat = build_threat_table(heat)
    write_json(CONTENT / "threat_table.json", threat)

    districts_raw = http_get_json("/districts")
    write_json(RAW / "districts.json", districts_raw)
    write_json(CONTENT / "districts.json", build_districts(districts_raw))

    ammo = http_get_json("/items/ammo")
    write_json(RAW / "ammo.json", ammo)

    weapons_catalog = paginate_items("Weapon")
    write_json(
        RAW / "weapons_catalog.json",
        {
            "source": f"{API}/items?cat=Weapon",
            "fetched_at": datetime.now(timezone.utc).isoformat(),
            "total": len(weapons_catalog),
            "items": weapons_catalog,
        },
    )

    vehicles_catalog = paginate_items("Vehicle")
    write_json(
        RAW / "vehicles_catalog.json",
        {
            "source": f"{API}/items?cat=Vehicle",
            "fetched_at": datetime.now(timezone.utc).isoformat(),
            "total": len(vehicles_catalog),
            "items": vehicles_catalog,
        },
    )

    missions_data = http_get_json("/missions", {"page": 1})
    write_json(RAW / "missions_page1.json", missions_data)
    mission_list = missions_data.get("missions") or missions_data if isinstance(missions_data, list) else missions_data.get("missions") or []
    # Keep a Domain-ready subset (first 40 real missions) without blowing file size
    norm_missions = [normalize_mission(m) for m in mission_list[:40]]
    write_json(CONTENT / "missions.json", norm_missions)

    contacts = http_get_json("/contacts")
    write_json(RAW / "contacts.json", contacts)
    # Lore index only (full bios live in raw/)
    lore = []
    for c in contacts if isinstance(contacts, list) else []:
        lore.append(
            {
                "id": c.get("sAPBDB") or c.get("id"),
                "title": c.get("sTitle") or c.get("sDisplayName"),
                "faction": c.get("eFaction"),
                "description": (c.get("sDescription") or "")[:500],
                "source": f"{SITE}/contacts",
            }
        )
    write_json(CONTENT / "contacts_lore.json", lore)

    mode = "core"
    if args.all_weapons:
        mode = "all"
    elif args.full_weapons:
        mode = "full"

    details: dict[str, dict[str, Any]] = {}
    if not args.skip_details:
        details = fetch_weapon_details(weapons_catalog, mode=mode, limit=args.limit)
        write_json(RAW / "weapon_details_cache.json", details)

    # Content weapons: detail-fetched first (real combat numbers), then list-only rows
    # with sensible defaults so Catalog::LoadWeaponsJson sees the full armory.
    INFRA_DEFAULTS: dict[str, dict[str, float | int]] = {
        "WeaponPrimaryAssaultRifle": {"damage": 145, "clip": 30, "rpm": 600, "max_range": 100},
        "WeaponPrimarySMG": {"damage": 90, "clip": 30, "rpm": 800, "max_range": 70},
        "WeaponPrimarySAR": {"damage": 160, "clip": 20, "rpm": 400, "max_range": 110},
        "WeaponPrimaryShotguns": {"damage": 35, "clip": 8, "rpm": 70, "max_range": 40},
        "WeaponPrimarySniperRifles": {"damage": 500, "clip": 5, "rpm": 45, "max_range": 200},
        "WeaponSecondary": {"damage": 160, "clip": 15, "rpm": 300, "max_range": 60},
        "WeaponPrimaryLMG": {"damage": 120, "clip": 75, "rpm": 650, "max_range": 100},
        "WeaponPrimaryLessThanLethal": {"damage": 40, "clip": 1, "rpm": 30, "max_range": 30},
        "WeaponPrimaryRocketLauncher": {"damage": 400, "clip": 1, "rpm": 20, "max_range": 120},
        "WeaponPrimaryGrenadeLaunchers": {"damage": 250, "clip": 1, "rpm": 25, "max_range": 80},
        "WeaponGrenade": {"damage": 200, "clip": 1, "rpm": 10, "max_range": 30},
    }

    detailed_rows: list[dict[str, Any]] = []
    light_rows: list[dict[str, Any]] = []
    for it in weapons_catalog:
        sid = it.get("sAPBDB")
        if sid in details:
            row = normalize_weapon(it, details[sid])
            detailed_rows.append(row)
        else:
            row = normalize_weapon(it, None)
            defs = INFRA_DEFAULTS.get(str(it.get("infracategory") or ""), {"damage": 100, "clip": 24, "rpm": 400, "max_range": 80})
            for k, v in defs.items():
                row.setdefault(k, v)
            light_rows.append(row)

    detailed_rows.sort(key=lambda r: (r.get("infracategory") or "", r.get("name") or ""))
    light_rows.sort(key=lambda r: (r.get("infracategory") or "", r.get("name") or ""))
    # Domain catalog: full armory (stats real when detail-fetched)
    write_json(CONTENT / "weapons.json", detailed_rows + light_rows)
    write_json(
        CONTENT / "weapons_catalog.json",
        {
            "source": f"{API}/items?cat=Weapon",
            "fetched_at": datetime.now(timezone.utc).isoformat(),
            "total": len(weapons_catalog),
            "with_stats": len(detailed_rows),
            "items": detailed_rows + light_rows,
        },
    )

    # Vehicles: list-level for now (detail optional)
    veh_rows = [normalize_vehicle(v) for v in vehicles_catalog if not v.get("bIsArmas")][:80]
    if not veh_rows:
        veh_rows = [normalize_vehicle(v) for v in vehicles_catalog[:80]]
    write_json(CONTENT / "vehicles.json", veh_rows)
    write_json(
        CONTENT / "vehicles_catalog.json",
        {
            "source": f"{API}/items?cat=Vehicle",
            "fetched_at": datetime.now(timezone.utc).isoformat(),
            "total": len(vehicles_catalog),
            "items": [normalize_vehicle(v) for v in vehicles_catalog],
        },
    )

    meta = {
        "source": SITE,
        "api": API,
        "cdn": "https://cdn.apbdb.com/img",
        "engine": "5.8",
        "project": str(ROOT).replace("\\", "/"),
        "fetched_at": datetime.now(timezone.utc).isoformat(),
        "live_version": version.get("live"),
        "otw_version": version.get("otw"),
        "db_version": version.get("db"),
        "counts": {
            "weapons_catalog": len(weapons_catalog),
            "weapons_with_stats": len(detailed_rows),
            "vehicles_catalog": len(vehicles_catalog),
            "missions_normalized": len(norm_missions),
            "contacts_lore": len(lore),
            "districts": len(build_districts(districts_raw)),
            "heat_rows": len(heat) if isinstance(heat, list) else 0,
        },
        "endpoints": {
            "items": f"{API}/items?cat=Weapon|Vehicle&page=N",
            "item_detail": f"{API}/items/{{sAPBDB}}",
            "heat": f"{API}/heat",
            "districts": f"{API}/districts",
            "missions": f"{API}/missions",
            "contacts": f"{API}/contacts",
            "ammo": f"{API}/items/ammo",
            "roles": f"{API}/roles",
            "version": f"{API}/version",
        },
        "notes": [
            "APB:Db is a community SPA; data comes from api.apbdb.com/beacon (not scraped HTML).",
            "Weapon combat numbers live under detail.eWeaponTypeLink.eWeaponType_0.",
            "threat_table.json opposition_multiplier uses apbdb fRewardMultiplier as pressure proxy.",
            "Full contact bios and raw SDD-shaped rows are under tools/apbdb/raw/.",
        ],
    }
    write_json(CONTENT / "apbdb_meta.json", meta)
    write_json(Path(__file__).resolve().parent / "README.json", meta)

    print("sync complete")
    print(json.dumps(meta["counts"], indent=2))
    return 0


if __name__ == "__main__":
    sys.exit(main())
