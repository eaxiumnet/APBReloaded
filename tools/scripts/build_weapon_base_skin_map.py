#!/usr/bin/env python3
"""Build the weapon BASE<->SKIN family map from the apbdb.com beacon API.

APB retail ships a limited set of base weapon MESHES; the 792 catalog weapons
are mostly SKINS/reskins/presets layered on those bases (confirmed on disk: a
shared WeaponSkins.upk holds the skin textures). The authoritative mesh key is
apbdb's `detail.eWeaponTypeLink.sAPBDB` (the UE3 weapon-class name, e.g.
"AssaultRifle_NTEC") - present on EVERY weapon's detail and NOT reliably
derivable from the slug (e.g. Weapon_SMG_PDW57 -> mesh key "SMG_PDW57_Base").

Grouping by that key gives the exact mesh-level dedup the extraction/import
pipeline needs so we rip each base mesh once and treat the rest as skins.

Output: work/weapon_base_skin_map.json  (+ resumable cache in work/apbdb_cache/)
Read-only against apbdb; writes only under work/. No engine/editor needed.
"""
from __future__ import annotations

import json
import sys
import time
import urllib.error
import urllib.request
from pathlib import Path

BASE = "https://api.apbdb.com/beacon"
HEADERS = {
    "Origin": "https://apbdb.com",
    "Referer": "https://apbdb.com/",
    "User-Agent": "Mozilla/5.0",
}
WORK = Path(r"D:\APBReloaded\work")
CACHE = WORK / "apbdb_cache"
OUT = WORK / "weapon_base_skin_map.json"


def _fetch(url: str, retries: int = 4) -> object:
    last: Exception | None = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers=HEADERS)
            with urllib.request.urlopen(req, timeout=30) as resp:
                return json.load(resp)
        except urllib.error.HTTPError as exc:
            if exc.code == 404:
                return None
            last = exc
        except Exception as exc:  # noqa: BLE001 - network resilience
            last = exc
        time.sleep(0.5 * (attempt + 1))
    raise RuntimeError(f"GET failed after {retries}: {url} :: {last}")


def _cached(cache_key: str, url: str) -> object:
    fp = CACHE / f"{cache_key}.json"
    if fp.exists():
        try:
            txt = fp.read_text(encoding="utf-8")
            if txt.strip():
                return json.loads(txt)
        except json.JSONDecodeError:
            pass  # corrupt cache -> refetch
    data = _fetch(url)
    fp.write_text(json.dumps(data), encoding="utf-8")
    return data


def enumerate_ids() -> tuple[list[str], int | None]:
    ids: list[str] = []
    total: int | None = None
    page = 1
    while True:
        data = _cached(
            f"list_weapon_p{page}",
            f"{BASE}/items?cat=Weapon&limit=200&page={page}",
        )
        if not isinstance(data, dict):
            raise RuntimeError(f"list page {page} bad payload")
        total = data.get("total", total)
        ids += [i["sAPBDB"] for i in data.get("items", [])]
        if page >= int(data.get("totalPages", page)):
            break
        page += 1
    seen: set[str] = set()
    return [x for x in ids if not (x in seen or seen.add(x))], total


def main() -> int:
    CACHE.mkdir(parents=True, exist_ok=True)

    ids, total = enumerate_ids()
    print(f"enumerated {len(ids)} weapon ids (api total={total})")

    # per-weapon detail (cached, resumable) -> authoritative mesh key
    records: dict[str, dict] = {}
    misses = 0
    for n, wid in enumerate(ids, 1):
        det = _cached(f"item_{wid}", f"{BASE}/items/{wid}")
        d = det.get("detail") if isinstance(det, dict) else None
        mesh_key = (d or {}).get("eWeaponTypeLink", {}).get("sAPBDB")
        if not mesh_key:
            mesh_key = f"__UNLINKED__{wid}"
            misses += 1
        records[wid] = {
            "mesh_key": mesh_key,
            "bPreset": (d or {}).get("bPreset"),
            "bIsSkinnable": (d or {}).get("bIsSkinnable"),
            "bIsArmas": det.get("bIsArmas") if isinstance(det, dict) else None,
            "display_name": det.get("sDisplayName") if isinstance(det, dict) else None,
            "infracategory": det.get("infracategory") if isinstance(det, dict) else None,
            "hud_image": det.get("eHUDImage") if isinstance(det, dict) else None,
        }
        if n % 100 == 0:
            print(f"  detail {n}/{len(ids)}")

    # group by mesh key; base = bPreset==0 (fallback shortest slug)
    groups: dict[str, list[str]] = {}
    for wid, r in records.items():
        groups.setdefault(r["mesh_key"], []).append(wid)

    families: dict[str, dict] = {}
    weapon_to_base: dict[str, dict] = {}
    for mesh_key, members in groups.items():
        members = sorted(members)
        bases = [m for m in members if records[m]["bPreset"] == 0]
        base = min(bases or members, key=lambda s: (len(s), s))
        hud = records[base]["hud_image"]
        families[mesh_key] = {
            "base": base,
            "base_display_name": records[base]["display_name"],
            "member_count": len(members),
            "members": members,
        }
        for m in members:
            # cosmetic reskin heuristic: same mesh, but its own HUD icon differs
            weapon_to_base[m] = {
                "base": base,
                "mesh_key": mesh_key,
                "is_skin": m != base,
                "is_armas": records[m]["bIsArmas"],
                "cosmetic": bool(
                    m != base
                    and records[m]["hud_image"]
                    and records[m]["hud_image"] != hud
                ),
            }

    out = {
        "generated": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "source": BASE,
        "grouping_key": "detail.eWeaponTypeLink.sAPBDB",
        "total_weapons": len(ids),
        "api_total": total,
        "mesh_family_count": len(families),
        "unlinked": misses,
        "families": dict(sorted(families.items())),
        "weapon_to_base": weapon_to_base,
    }
    OUT.write_text(json.dumps(out, indent=2), encoding="utf-8")

    covered = len(weapon_to_base)
    skins = sum(1 for v in weapon_to_base.values() if v["is_skin"])
    print(f"WROTE {OUT}")
    print(f"coverage: {covered}/{len(ids)} weapons mapped")
    print(f"mesh families(bases)={len(families)}  skins={skins}  unlinked={misses}")
    if covered != len(ids) or covered == 0:
        print("FATAL: incomplete coverage", file=sys.stderr)
        return 3
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
