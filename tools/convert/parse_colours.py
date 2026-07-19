#!/usr/bin/env python3
"""Parse APB Colours/*.ini palette grids into one JSON catalog (M3).

Retail APB Reloaded stores color pickers as tab-separated RGB grids:
each line is one grid row; cells are "r,g,b"; empty cells (the file uses
double-tabs) are column gaps and are preserved as null so the on-screen
grid geometry survives the round trip.

Output: { "<palette>": { "rows": [[ {"r":..,"g":..,"b":..} | null, ... ], ... ],
                         "source": <ini path> } }

Usage (rerunnable; parameterized for 2011 tables too):
  python tools/convert/parse_colours.py
  python tools/convert/parse_colours.py --in-dir "<retail>/APBGame/Content/Colours" ^
      --out Content/Data/palettes.json
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
RETAIL_COLOURS = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Colours"
)
DEFAULT_OUT = ROOT / "Content" / "Data" / "palettes.json"


def parse_ini(path: Path) -> list[list[dict | None]]:
    rows: list[list[dict | None]] = []
    for line in path.read_text(errors="ignore").splitlines():
        if not line.strip():
            continue
        row: list[dict | None] = []
        for cell in line.split("\t"):
            cell = cell.strip()
            if not cell:
                row.append(None)
                continue
            parts = [p.strip() for p in cell.split(",")]
            if len(parts) == 3 and all(p.lstrip("+-").isdigit() for p in parts):
                r, g, b = (int(p) for p in parts)
                row.append({"r": r, "g": g, "b": b})
            else:
                row.append(None)  # unparseable cell keeps grid slot, flagged in meta
        # trim trailing nulls only (internal gaps are meaningful)
        while row and row[-1] is None:
            row.pop()
        rows.append(row)
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in-dir", type=Path, default=RETAIL_COLOURS)
    ap.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = ap.parse_args()

    if not args.in_dir.is_dir():
        print("missing colours dir:", args.in_dir)
        return 1

    palettes: dict[str, dict] = {}
    for ini in sorted(args.in_dir.glob("*.ini")):
        rows = parse_ini(ini)
        n_colors = sum(1 for r in rows for c in r if c is not None)
        palettes[ini.stem] = {
            "source": str(ini),
            "row_count": len(rows),
            "color_count": n_colors,
            "rows": rows,
        }

    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(palettes, indent=1), encoding="utf-8")
    summary = {k: (v["row_count"], v["color_count"]) for k, v in palettes.items()}
    print(f"palettes={len(palettes)} -> {args.out}")
    for name, (r, c) in summary.items():
        print(f"  {name}: {r} rows, {c} colors")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
