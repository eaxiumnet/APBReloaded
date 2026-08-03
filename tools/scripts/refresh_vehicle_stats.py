#!/usr/bin/env python3
"""Re-seed per-vehicle fidelity stats from Wayback-archived apbdb.com item pages.

apbdb.com is defunct. Its item pages are preserved by the Wayback Machine
(Common Crawl). Each page lists real stats: Max Speed (m/s) and Max Health.
The current vehicles.json carries uniform fallback defaults (21.0 / 800.0) that
the fidelity oracle flags (vehicle.catalog.apbdb). This script:

1. Queries the Wayback CDX index for apbdb.com/items/Vehicle_* snapshots.
2. Fetches each vehicle's closest 200 snapshot, parses Max Speed + Max Health.
3. Writes vehicles.json with per-vehicle max_speed/health (all other fields kept).
4. Slot rows (synthetic catalog rows, no item page) inherit their base vehicle.
5. accel stays a per-class handling default: apbdb publishes no acceleration stat.

Evidence: .omo/vehicle_stats_scrape/evidence.json (per-vehicle snapshot url,
timestamp, parsed values, parse source). Rows without an archived page keep their
previous values and are reported as unresolved.
"""
from __future__ import annotations

import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
VEHICLES = ROOT / "Content" / "Data" / "vehicles.json"
EVIDENCE = ROOT / ".omo" / "vehicle_stats_scrape" / "evidence.json"
UA = "APBReloaded-OfflineRecreate/1.0 (fidelity re-seed; apbdb.com archived by Internet Archive)"
CDX = "http://web.archive.org/cdx/search/cdx"
WAYBACK = "http://web.archive.org/web"


def http_get(url: str, retries: int = 3, timeout: int = 45) -> bytes:
    last: Exception | None = None
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=timeout) as resp:
                return resp.read()
        except Exception as exc:  # noqa: BLE001
            last = exc
            time.sleep(1.0 * (attempt + 1))
    raise RuntimeError(f"GET failed {url}: {last}")


def item_id_from_url(url: str) -> str:
    path = url.split("/items/", 1)[-1].rstrip("/")
    return path.split("?", 1)[0]


def cdx_latest(vehicle_ids: list[str]) -> dict[str, str]:
    q = urllib.parse.urlencode({
        "url": "apbdb.com/items/Vehicle_*",
        "output": "json",
        "fl": "original,timestamp,statuscode",
        "filter": "statuscode:200",
        "limit": "12000",
    })
    data = http_get(f"{CDX}?{q}")
    rows = json.loads(data.decode("utf-8", "replace"))
    best: dict[str, str] = {}
    for row in rows[1:]:
        if len(row) < 3 or row[2] != "200":
            continue
        item = item_id_from_url(row[0].strip())
        if not item:
            continue
        ts = row[1]
        # Prefer the newest snapshot per item; match exact id or id+_suffix (faction variants).
        for vid in vehicle_ids:
            if item == vid or item.startswith(vid + "_"):
                if vid not in best or ts > best[vid]:
                    best[vid] = ts
    return best


def parse_stats(html: str) -> dict[str, float]:
    text = re.sub(r"<[^>]+>", " ", html)
    text = re.sub(r"\s+", " ", text)
    stats: dict[str, float] = {}
    m = re.search(r"Max Speed\s*([\d.,]+)\s*m/s", text, re.IGNORECASE)
    if m:
        stats["max_speed"] = float(m.group(1).replace(",", ""))
    m = re.search(r"Max Health\s*([\d.,]+)", text, re.IGNORECASE)
    if m:
        stats["health"] = float(m.group(1).replace(",", ""))
    return stats


def slot_base(vid: str) -> str:
    return re.sub(r"_slot_\d+$", "", vid)


def main() -> int:
    vehicles = json.loads(VEHICLES.read_text(encoding="utf-8"))
    ids = [v["id"] for v in vehicles]
    print(f"vehicles={len(ids)}", flush=True)

    print("querying CDX index ...", flush=True)
    snaps = cdx_latest(ids)
    print(f"cdx snapshots={len(snaps)}", flush=True)

    evidence: dict[str, object] = {}
    stats: dict[str, dict[str, float]] = {}
    resolved = 0
    unresolved: list[str] = []
    fetch_fail: list[str] = []

    for idx, vid in enumerate(ids):
        ts = snaps.get(vid)
        if not ts:
            unresolved.append(vid)
            continue
        url = f"{WAYBACK}/{ts}id_/http://apbdb.com/items/{vid}"
        try:
            html = http_get(url).decode("utf-8", "replace")
        except Exception:
            fetch_fail.append(vid)
            continue
        parsed = parse_stats(html)
        if "max_speed" in parsed and "health" in parsed:
            stats[vid] = parsed
            evidence[vid] = {
                "id": vid,
                "snapshot": f"{WAYBACK}/{ts}/https://apbdb.com/items/{vid}",
                "timestamp": ts,
                "max_speed": parsed["max_speed"],
                "health": parsed["health"],
            }
            resolved += 1
        else:
            unresolved.append(vid)
        if idx % 25 == 0:
            print(f"  progress {idx}/{len(ids)} resolved={resolved}", flush=True)
        time.sleep(0.5)

    print(f"scraped resolved={resolved} unresolved={len(unresolved)} fetch_fail={len(fetch_fail)}", flush=True)

    updated = 0
    for v in vehicles:
        vid = v["id"]
        src = stats.get(vid)
        if src is None and "_slot_" in vid:
            src = stats.get(slot_base(vid))
        if src:
            v["max_speed"] = src["max_speed"]
            v["health"] = src["health"]
            v["stats_source"] = "apbdb (Wayback archive)"
            updated += 1

    VEHICLES.write_text(json.dumps(vehicles, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    EVIDENCE.parent.mkdir(parents=True, exist_ok=True)
    EVIDENCE.write_text(json.dumps({
        "schema_version": 1,
        "generated_by": "tools/scripts/refresh_vehicle_stats.py",
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "vehicles_total": len(vehicles),
        "rows_updated": updated,
        "pages_resolved": resolved,
        "unresolved": unresolved,
        "fetch_failures": fetch_fail,
        "per_vehicle": evidence,
    }, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"rows_updated={updated} evidence={EVIDENCE}", flush=True)
    return 0 if resolved > 0 else 1


if __name__ == "__main__":
    sys.exit(main())
