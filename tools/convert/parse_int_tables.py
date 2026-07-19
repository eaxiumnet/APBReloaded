#!/usr/bin/env python3
"""Parse APB localization .int tables (INI-style sections) into JSON (M3).

Handles both on-disk encodings found in the reference installs:
  - 2011 RTW build : plain ASCII/UTF-8, no BOM
  - retail Steam   : UTF-16LE with FF FE BOM
Encoding is sniffed from the BOM, never assumed.

Output: { "<section>": { "<key>": "<value>", ... }, ... } plus a "_meta"
block with source path/encoding/section counts.

Usage (parameterized — reusable for TaskObjectives.int, MissionTemplates.int, ...):
  python tools/convert/parse_int_tables.py --in <file.int> --out <file.json>
  python tools/convert/parse_int_tables.py --in X.int --out Y.json \
      --sections APBLoginScreen CharacterSelectScreen
  (no --sections => all sections)
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
INT_2011 = (
    ROOT
    / "2011 apb"
    / "APB All Points Bulletin"
    / "APB North America"
    / "APBGame"
    / "Localization"
    / "INT"
    / "APBUserInterface.int"
)
INT_RETAIL = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\APBUserInterface.int"
)
DEFAULT_SECTIONS = [
    "APBLoginScreen",
    "CharacterSelectScreen",
    "CharacterCreateScreen",
    "CharacterCustomisationScreens",
    "DistrictSelect_Action",
    "WorldSelectScreen",
    "Marketplace",
]


def load_text(path: Path) -> tuple[str, str]:
    b = path.read_bytes()
    if b[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return b.decode("utf-16"), "utf-16"
    return b.decode("utf-8", errors="replace"), "utf-8"


def parse_int(text: str) -> dict[str, dict[str, str]]:
    sections: dict[str, dict[str, str]] = {}
    current: dict[str, str] | None = None
    for raw in text.splitlines():
        line = raw.strip()
        if not line or line.startswith((";", "//")):
            continue
        if line.startswith("[") and line.endswith("]"):
            name = line[1:-1].strip()
            current = sections.setdefault(name, {})
            continue
        if current is not None and "=" in line:
            key, _, value = line.partition("=")
            current[key.strip()] = value.strip()
    return sections


def convert(in_path: Path, out_path: Path, wanted: list[str] | None) -> int:
    if not in_path.is_file():
        print("missing input:", in_path, file=sys.stderr)
        return 1
    text, enc = load_text(in_path)
    sections = parse_int(text)
    missing: list[str] = []
    if wanted:
        missing = [s for s in wanted if s not in sections]
        out_secs = {s: sections[s] for s in wanted if s in sections}
    else:
        out_secs = sections
    doc = {
        "_meta": {
            "source": str(in_path),
            "encoding": enc,
            "sections_total": len(sections),
            "sections_written": len(out_secs),
            "sections_missing": missing,
            "keys_written": sum(len(v) for v in out_secs.values()),
        }
    }
    doc.update(out_secs)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    out_path.write_text(json.dumps(doc, indent=1, ensure_ascii=False), encoding="utf-8")
    print(
        f"{in_path.name} [{enc}]: {len(out_secs)} sections, "
        f"{doc['_meta']['keys_written']} keys -> {out_path}"
        + (f" MISSING: {missing}" if missing else "")
    )
    return 0 if not missing else 2


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--in", dest="in_path", type=Path)
    ap.add_argument("--out", dest="out_path", type=Path)
    ap.add_argument("--sections", nargs="+", default=None,
                    help="Sections to keep (default: all, or the M3 UI set with --m3-defaults)")
    ap.add_argument("--m3-defaults", action="store_true",
                    help="Run both APBUserInterface.int files with the M3 section set")
    args = ap.parse_args()

    if args.m3_defaults or (args.in_path is None and args.out_path is None):
        rc = 0
        rc |= convert(INT_2011, ROOT / "Content" / "Data" / "ui_strings_2011.json", DEFAULT_SECTIONS)
        rc |= convert(INT_RETAIL, ROOT / "Content" / "Data" / "ui_strings_retail.json", DEFAULT_SECTIONS)
        return rc
    if not args.in_path or not args.out_path:
        ap.error("--in and --out are required together (or use --m3-defaults)")
    return convert(args.in_path, args.out_path, args.sections)


if __name__ == "__main__":
    raise SystemExit(main())
