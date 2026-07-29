"""Extract weapon DisplayNames from retail APB's InventoryItemTypes.INT.

The retail localization table is the authoritative bridge from APB's SDD/apbdb
item-type taxonomy (e.g. ``Weapon_AssaultRifle_ATAC``) to human display names
(``ATAC 424``). The extracted JSON lets the content studio resolve real weapon
names WITHOUT the retail install being present at runtime.

Source (default): the retail Steam install's UTF-16 INT table.
Output: Content/Data/weapon_display_names.json  ->  { sAPBDB: DisplayName, ... }

Usage:
    python tools/scripts/extract_weapon_names.py
    python tools/scripts/extract_weapon_names.py --int "<path to InventoryItemTypes.INT>"
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
DEFAULT_INT = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
    r"\APBGame\Localization\INT\InventoryItemTypes.INT"
)
OUT_PATH = REPO_ROOT / "Content" / "Data" / "weapon_display_names.json"

# InventoryItemTypes_<sAPBDB>_DisplayName=<value>  where sAPBDB starts with Weapon_
_LINE = re.compile(r"^InventoryItemTypes_(Weapon_.+?)_DisplayName=(.*)$")


def _unwrap(value: str) -> str:
    """UE INT wraps values containing special chars in double quotes.

    Embedded quotes are left literal, so we strip exactly one outer pair:
        "ATAC 424 "Bodyguard""  ->  ATAC 424 "Bodyguard"
    """
    value = value.rstrip("\r\n")
    if len(value) >= 2 and value.startswith('"') and value.endswith('"'):
        return value[1:-1]
    return value


def extract(int_path: Path) -> dict[str, str]:
    """Return {sAPBDB: display_name} for every gun DisplayName line."""
    text = int_path.read_text(encoding="utf-16")
    names: dict[str, str] = {}
    for line in text.splitlines():
        m = _LINE.match(line)
        if not m:
            continue
        sapbdb, raw = m.group(1), m.group(2)
        display = _unwrap(raw).strip()
        if display:
            names[sapbdb] = display
    return names


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--int", dest="int_path", type=Path, default=DEFAULT_INT)
    ap.add_argument("--out", dest="out_path", type=Path, default=OUT_PATH)
    args = ap.parse_args(argv)

    if not args.int_path.is_file():
        print(f"ERROR: INT not found: {args.int_path}", file=sys.stderr)
        print("Pass --int <path to InventoryItemTypes.INT>", file=sys.stderr)
        return 1

    names = extract(args.int_path)
    if not names:
        print("ERROR: no weapon DisplayName lines matched", file=sys.stderr)
        return 2

    payload = {
        "_meta": {
            "source": str(args.int_path),
            "table": "InventoryItemType (SDD) -> InventoryItemTypes.INT",
            "encoding": "utf-16",
            "extracted_at": datetime.now(timezone.utc).isoformat(),
            "count": len(names),
            "note": "sAPBDB item-type taxonomy -> retail display name. Bridge for content-studio name resolution.",
        },
        "names": dict(sorted(names.items())),
    }
    args.out_path.parent.mkdir(parents=True, exist_ok=True)
    args.out_path.write_text(json.dumps(payload, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"OK: wrote {len(names)} weapon display names -> {args.out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
