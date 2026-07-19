#!/usr/bin/env python3
"""Expand clothing.json from apbdb Clothing category; expand vehicles.json to full fleet."""
from __future__ import annotations

import json
import re
import sys
import time
import urllib.parse
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

API = "https://api.apbdb.com/beacon"
SITE = "https://apbdb.com"
UA = "APBReloaded-OfflineRecreate/1.0"
ROOT = Path(__file__).resolve().parents[2]
CONTENT = ROOT / "Content" / "Data"
RAW = Path(__file__).resolve().parent / "raw"
RAW.mkdir(parents=True, exist_ok=True)


def http_get_json(path: str, params: dict | None = None, retries: int = 4):
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
            time.sleep(0.5 * (attempt + 1))
    raise RuntimeError(f"GET {url}: {last}")


def strip_color(name: str) -> str:
    return re.sub(r"<[^>]+>", "", name or "").strip()


def paginate(cat: str) -> list[dict]:
    page, total_pages, items = 1, 1, []
    while page <= total_pages:
        data = http_get_json("/items", {"cat": cat, "page": page})
        if page == 1:
            total_pages = int(data.get("totalPages") or 1)
            print(f"{cat}: total={data.get('total')} pages={total_pages}")
        items.extend(data.get("items") or [])
        page += 1
        time.sleep(0.04)
    return items


def guess_slot(item: dict) -> str:
    blob = " ".join(
        [
            str(item.get("sAPBDB") or ""),
            str(item.get("subcategory") or ""),
            str(item.get("infracategory") or ""),
            str(item.get("sDisplayName") or ""),
        ]
    ).lower()
    rules = [
        ("face", ["face", "mask", "glasses", "goggles", "visor", "eyewear", "balaclava"]),
        ("head", ["head", "hat", "cap", "helmet", "beanie", "hood", "bandana", "stovepipe", "fedora", "beret", "turban"]),
        ("hands", ["hand", "glove", "gauntlet", "wrist", "brace"]),
        ("feet", ["foot", "feet", "boot", "shoe", "sneaker", "sandal", "heel"]),
        ("legs", ["leg", "pant", "jean", "short", "skirt", "trouser", "lower"]),
        ("accessory", ["accessory", "watch", "chain", "earring", "necklace", "belt", "bag", "earpiece", "comm"]),
        ("torso", ["torso", "jacket", "shirt", "vest", "coat", "hoodie", "top", "armor", "upper", "chest", "body"]),
    ]
    for slot, keys in rules:
        if any(k in blob for k in keys):
            return slot
    return "torso"


def price_of(item: dict) -> int:
    p = item.get("eDefaultPrice") or {}
    if isinstance(p, dict) and p.get("nCostAPBCash") is not None:
        return int(p.get("nCostAPBCash") or 0)
    return 100


def main() -> int:
    clothing_raw = paginate("Clothing")
    (RAW / "clothing_catalog.json").write_text(
        json.dumps(
            {
                "source": f"{API}/items?cat=Clothing",
                "fetched_at": datetime.now(timezone.utc).isoformat(),
                "total": len(clothing_raw),
                "items": clothing_raw,
            },
            indent=2,
            ensure_ascii=False,
        )
        + "\n",
        encoding="utf-8",
    )

    clothing = []
    for it in clothing_raw:
        sid = it.get("sAPBDB")
        if not sid:
            continue
        clothing.append(
            {
                "id": sid,
                "name": strip_color(it.get("sDisplayName") or sid),
                "category": "Clothing",
                "slot": guess_slot(it),
                "subcategory": it.get("subcategory"),
                "infracategory": it.get("infracategory"),
                "criminal": bool(it.get("bCriminal")),
                "enforcer": bool(it.get("bEnforcer")),
                "armas_price": price_of(it),
                "armas_listed": 1,
                "is_armas": bool(it.get("bIsArmas")),
                "source": f"{SITE}/items/{sid}",
            }
        )
    have: dict[str, int] = {}
    for c in clothing:
        have[c["slot"]] = have.get(c["slot"], 0) + 1
    print("slot_counts", have)

    curated_path = CONTENT / "clothing.json"
    curated = json.loads(curated_path.read_text(encoding="utf-8")) if curated_path.exists() else []
    seen = {c["id"] for c in curated}
    merged = list(curated)
    for c in clothing:
        if c["id"] not in seen:
            merged.append(c)
            seen.add(c["id"])
    curated_path.write_text(json.dumps(merged, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"clothing.json entries={len(merged)}")

    # Vehicles: prefer existing catalog, else paginate
    veh_path = CONTENT / "vehicles_catalog.json"
    items: list[dict] = []
    if veh_path.exists():
        veh_cat = json.loads(veh_path.read_text(encoding="utf-8"))
        raw_items = veh_cat.get("items") if isinstance(veh_cat, dict) else veh_cat
        if isinstance(raw_items, list) and raw_items:
            items = raw_items
    if not items:
        vraw = paginate("Vehicle")
        for v in vraw:
            items.append(
                {
                    "id": v.get("sAPBDB"),
                    "name": strip_color(v.get("sDisplayName") or ""),
                    "category": "Vehicle",
                    "subcategory": v.get("subcategory"),
                    "infracategory": v.get("infracategory"),
                    "criminal": bool(v.get("bCriminal")),
                    "enforcer": bool(v.get("bEnforcer")),
                    "min_rating": int(v.get("nMinRating") or 0),
                    "is_armas": bool(v.get("bIsArmas")),
                    "source": f"{SITE}/items/{v.get('sAPBDB')}",
                    "max_speed": 21.0,
                    "accel": 14.0,
                    "health": 800.0,
                }
            )
        veh_path.write_text(
            json.dumps(
                {
                    "source": f"{API}/items?cat=Vehicle",
                    "fetched_at": datetime.now(timezone.utc).isoformat(),
                    "total": len(items),
                    "items": items,
                },
                indent=2,
                ensure_ascii=False,
            )
            + "\n",
            encoding="utf-8",
        )

    for v in items:
        v.setdefault("max_speed", 21.0)
        v.setdefault("accel", 14.0)
        v.setdefault("health", 800.0)
        if not v.get("id") and v.get("sAPBDB"):
            v["id"] = v["sAPBDB"]
        if not v.get("name") and v.get("sDisplayName"):
            v["name"] = strip_color(v["sDisplayName"])

    non_armas = [v for v in items if not v.get("is_armas") and v.get("id")]
    armas = [v for v in items if v.get("is_armas") and v.get("id")]
    full = non_armas + armas
    (CONTENT / "vehicles.json").write_text(json.dumps(full, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"vehicles.json entries={len(full)} non_armas={len(non_armas)}")

    meta_path = CONTENT / "apbdb_meta.json"
    meta = json.loads(meta_path.read_text(encoding="utf-8")) if meta_path.exists() else {}
    meta.setdefault("counts", {})
    meta["counts"]["clothing"] = len(merged)
    meta["counts"]["vehicles_domain"] = len(full)
    meta["clothing_expanded_at"] = datetime.now(timezone.utc).isoformat()
    meta_path.write_text(json.dumps(meta, indent=2) + "\n", encoding="utf-8")
    print("done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
