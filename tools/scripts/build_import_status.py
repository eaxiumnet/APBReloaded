"""Generate work/IMPORT_STATUS.md — asset import/extraction status dashboard.

Read-only. Sources:
  - tools/import_ledger.json                  (manual tracking entries, plan D12)
  - Content/Imported/**/*.uasset              (what is actually in the project)
  - Content/Data/district_placements/*.json   (what district manifests still need)

Run:  python tools/scripts/build_import_status.py
"""
from __future__ import annotations

import json
from collections import Counter
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(r"D:\APBReloaded")
LEDGER = ROOT / "tools" / "import_ledger.json"
IMPORTED = ROOT / "Content" / "Imported"
PLACEMENTS = ROOT / "Content" / "Data" / "district_placements"
OUT = ROOT / "work" / "IMPORT_STATUS.md"

# Approximate streaming-block counts from the 2026-07-19 retail install inventory
# (exploration report). Only districts with a verified count are listed.
TARGET_BLOCKS = {"financial": 270, "waterfront": 268}


def imported_stats() -> tuple[set[str], Counter]:
    stems: set[str] = set()
    counts: Counter = Counter()
    if not IMPORTED.is_dir():
        return stems, counts
    for p in IMPORTED.rglob("*.uasset"):
        stems.add(p.stem)
        rel = p.relative_to(IMPORTED)
        if rel.parts[0] == "Districts" and len(rel.parts) > 2:
            counts[f"Districts/{rel.parts[1]}"] += 1
        else:
            counts[rel.parts[0] if len(rel.parts) > 1 else "(root)"] += 1
    return stems, counts


def manifest_rows(stems: set[str]) -> list[tuple[str, str, int, int]]:
    rows = []
    for mf in sorted(PLACEMENTS.glob("*.json")):
        if mf.stem.endswith("_bound"):
            continue
        data = json.loads(mf.read_text(encoding="utf-8"))
        placements = data.get("placements") or []
        hit = sum(
            1
            for pl in placements
            if (pl.get("mesh_id") or "") in stems
            or (pl.get("ue_path") or "").rstrip("/").split("/")[-1].split(".")[0] in stems
        )
        rows.append((mf.name, str(data.get("district_id") or "?"), len(placements), hit))
    return rows


def main() -> int:
    stems, counts = imported_stats()
    ledger = json.loads(LEDGER.read_text(encoding="utf-8")) if LEDGER.exists() else {"entries": []}
    rows = manifest_rows(stems)
    now = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    md: list[str] = [
        "# Import Status",
        "",
        f"Generated: {now} by `tools/scripts/build_import_status.py` (read-only).",
        "",
        "## Ledger entries (tools/import_ledger.json)",
        "",
        "| asset_key | source | status | dest | note |",
        "|---|---|---|---|---|",
    ]
    for e in ledger.get("entries", []):
        md.append(
            f"| `{e.get('asset_key')}` | {e.get('source')} | **{e.get('status')}** "
            f"| `{e.get('dest')}` | {e.get('note', '')} |"
        )

    md += [
        "",
        f"## Imported uassets on disk ({sum(counts.values())} total)",
        "",
        "| category | .uasset count |",
        "|---|---|",
    ]
    for cat, n in sorted(counts.items()):
        md.append(f"| {cat} | {n} |")

    md += [
        "",
        "## District placement manifest coverage",
        "",
        "Mesh references in each manifest that resolve to an imported .uasset:",
        "",
        "| manifest | district | placements | resolvable | coverage |",
        "|---|---|---|---|---|",
    ]
    gaps: list[str] = []
    manifests_per_district: Counter = Counter()
    for name, district, total, hit in rows:
        rate = (hit / total * 100.0) if total else 0.0
        md.append(f"| {name} | {district} | {total} | {hit} | {rate:.1f}% |")
        manifests_per_district[district.lower()] += 1
        if hit < total:
            gaps.append(f"- **{name}** ({district}): {total - hit} mesh refs unresolved → re-import those meshes")

    md += [
        "",
        "## District block coverage (vs retail streaming-block counts)",
        "",
        "A district is content-complete only when every streaming block has a manifest",
        "whose meshes all resolve. Retail block counts from the 2026-07-19 inventory.",
        "",
        "| district | manifests present | retail blocks (approx) | status |",
        "|---|---|---|---|",
    ]
    for district, target in TARGET_BLOCKS.items():
        present = manifests_per_district.get(district, 0)
        status = "✅ complete" if present >= target else f"⚠️ partial — {target - present} blocks to export"
        md.append(f"| {district} | {present} | ~{target} | {status} |")
        if present < target:
            gaps.append(
                f"- **{district}**: {present} of ~{target} block manifests present — "
                f"export remaining blocks via `tools/scripts/export_apb_level_parallel.py` (see roadmap M9/M10)"
            )

    md += ["", "## Remaining work (auto-derived)", ""]
    md += gaps if gaps else ["- None — all manifests resolve and block coverage is complete."]
    md += [
        "",
        "Legend: `extracted` = dumped from source packages, not yet imported · "
        "`imported` = .uasset exists · `bound` = referenced by a manifest · "
        "`manual` = must be rebuilt by hand.",
        "",
    ]

    OUT.write_text("\n".join(md), encoding="utf-8")
    print(f"wrote {OUT}")
    print(f"uassets={sum(counts.values())} manifests={len(rows)} gaps={len(gaps)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
