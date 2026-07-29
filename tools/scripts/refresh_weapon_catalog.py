"""Manually refresh weapon display names from the live apbdb.com beacon API.

The tool ships a baked Content/Data/weapons_catalog.json (the authoritative name
source). This script re-fetches the current apbdb weapon list so a maintainer can
diff/update that catalog when APB ships new weapons. It is NOT run at server time.

It writes a side file (Content/Data/apbdb_weapon_names.refresh.json) mapping the
internal name (sAPBDB) -> clean display name, plus a small diff summary against the
baked catalog. It never overwrites weapons_catalog.json.

Usage (from repo root):
    python tools/scripts/refresh_weapon_catalog.py
"""

from __future__ import annotations

import json
import re
import sys
import time
import urllib.request
from pathlib import Path

API_BASE = "https://api.apbdb.com/beacon"
REPO_ROOT = Path(__file__).resolve().parents[2]
DATA = REPO_ROOT / "Content" / "Data"
BAKED_CATALOG = DATA / "weapons_catalog.json"
OUT = DATA / "apbdb_weapon_names.refresh.json"

_COLOR = re.compile(r"<Color:[^>]*>(.*?)</Color>", re.IGNORECASE | re.DOTALL)


def strip_color(name: str) -> str:
    return _COLOR.sub(r"\1", name or "").strip()


def _get_json(url: str) -> dict:
    req = urllib.request.Request(url, headers={"User-Agent": "apb-content-studio/1.0"})
    with urllib.request.urlopen(req, timeout=30) as resp:
        return json.loads(resp.read().decode("utf-8"))


def fetch_all_weapons() -> dict[str, str]:
    result: dict[str, str] = {}
    page = 1
    while True:
        data = _get_json(f"{API_BASE}/items/?cat=Weapon&limit=500&page={page}")
        for item in data.get("items", []):
            internal = item.get("sAPBDB") or item.get("id")
            display = strip_color(item.get("sDisplayName") or item.get("name") or "")
            if internal and display:
                result[internal] = display
        total_pages = int(data.get("totalPages") or 1)
        if page >= total_pages:
            break
        page += 1
        time.sleep(0.25)
    return result


def _baked_names() -> dict[str, str]:
    if not BAKED_CATALOG.is_file():
        return {}
    doc = json.loads(BAKED_CATALOG.read_text(encoding="utf-8"))
    return {w["id"]: w.get("name", "") for w in doc.get("items", []) if w.get("id")}


def main() -> int:
    try:
        fresh = fetch_all_weapons()
    except Exception as exc:  # network/endpoint failure -> report, don't crash silently
        print(f"ERROR fetching apbdb: {exc}", file=sys.stderr)
        return 1

    baked = _baked_names()
    added = sorted(k for k in fresh if k not in baked)
    changed = sorted(k for k in fresh if k in baked and baked[k] != fresh[k])

    OUT.write_text(json.dumps(fresh, indent=2, ensure_ascii=False), encoding="utf-8")
    print(f"fetched {len(fresh)} weapons -> {OUT}")
    print(f"baked catalog has {len(baked)} weapon ids")
    print(f"new ids not in baked catalog: {len(added)}")
    for k in added[:20]:
        print(f"  + {k} = {fresh[k]}")
    print(f"display-name changes vs baked: {len(changed)}")
    for k in changed[:20]:
        print(f"  ~ {k}: {baked[k]!r} -> {fresh[k]!r}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
