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

# Raw retail *.upk block-package counts from the 2026-07-19 install inventory.
# NOTE: these are NOT the streaming-block set. A district's playable geometry is
# defined by the LevelStreamingKismet entries baked into its *_MASTER.APB map
# (decoded by tools/scripts/decode_level_streaming.py into stream_chunks). The
# retail .upk count includes LOD variants, ArtProps sub-blocks, minimaps and other
# non-streamed packages, so it massively over-counts the real block set. These
# figures are shown for reference only and MUST NOT be treated as an export target.
RETAIL_UPK_BLOCKS = {"financial": 270, "waterfront": 268}


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


def manifest_rows(stems: set[str]) -> list[tuple[str, str, int, int, int, int]]:
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
        chunks = len(data.get("stream_chunks") or [])
        pkgs = len(data.get("source_packages") or [])
        rows.append(
            (mf.name, str(data.get("district_id") or "?"), len(placements), hit, chunks, pkgs)
        )
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
        "Each manifest is decoded from its district's `*_MASTER.APB` LevelStreamingKismet",
        "table (the authoritative streaming-block set). `resolvable` = mesh refs that map",
        "to an imported `.uasset`; `chunks` = distinct streamed packages placed in-world.",
        "",
        "| manifest | district | placements | resolvable | coverage | chunks | pkgs |",
        "|---|---|---|---|---|---|---|",
    ]
    gaps: list[str] = []
    manifests_per_district: Counter = Counter()
    chunks_per_district: Counter = Counter()
    for name, district, total, hit, chunks, pkgs in rows:
        rate = (hit / total * 100.0) if total else 0.0
        md.append(
            f"| {name} | {district} | {total} | {hit} | {rate:.1f}% | {chunks} | {pkgs} |"
        )
        manifests_per_district[district.lower()] += 1
        chunks_per_district[district.lower()] += chunks
        if hit < total:
            gaps.append(f"- **{name}** ({district}): {total - hit} mesh refs unresolved → re-import those meshes")

    md += [
        "",
        "## District streaming coverage",
        "",
        "A district's geometry is complete when every LevelStreamingKismet entry in its",
        "`*_MASTER.APB` has a decoded chunk whose meshes resolve. The `RETAIL_UPK_BLOCKS`",
        "figures below are the raw retail `.upk` package count (LODs + ArtProps + minimaps",
        "included) and are shown ONLY to contrast against the true streamed-chunk count —",
        "they are **not** an export target. See `decode_level_streaming.py`.",
        "",
        "| district | manifests | streamed chunks | retail .upk (ref only) |",
        "|---|---|---|---|",
    ]
    for district in sorted(chunks_per_district):
        present = manifests_per_district.get(district, 0)
        chunks = chunks_per_district.get(district, 0)
        ref = RETAIL_UPK_BLOCKS.get(district)
        ref_txt = f"~{ref}" if ref is not None else "n/a"
        md.append(f"| {district} | {present} | {chunks} | {ref_txt} |")

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
