from __future__ import annotations

import sys
from pathlib import Path


SCRIPT = Path(r"D:\APBReloaded\tools\build_placement_bind_report.py")


def main() -> int:
    fails = []
    text = SCRIPT.read_text(encoding="utf-8")

    if '"Financial_Block09_realv2.json"' not in text:
        fails.append("MANIFEST_TARGET_MISSING name=Financial_Block09_realv2.json")

    if '"Financial_Block09.json"' in text:
        fails.append("STALE_MANIFEST_TARGET_PRESENT name=Financial_Block09.json")

    for line in fails:
        print(f"FAIL {line}")
    print(f"FAILS={len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
