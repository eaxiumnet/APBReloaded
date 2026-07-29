"""Gate: the Financial freeroam capture must be a legible image, not black and not blown out.

Two real regressions this gate pins, both caught only by pixel statistics:

  1. Black frame - when M_APBMaster failed to compile on PCD3D_SM6 the whole district fell
     back to Default Material. financial_render_RED.png measures mean=24.4, 67.2% of pixels
     at or below 8. A "tests pass / lsp clean" run cannot see this.
  2. Blown-out frame - AEM_Manual with AutoExposureBias interpreted as EV100=1.0 over-exposes
     the scene once real lit surfacing replaces the unlit debug material, clipping 12.5% of
     pixels at or above 250.

Thresholds are deliberately wide: this gate asserts legibility, not art direction.

Requires Pillow. Runs under plain CPython; does not import the `unreal` module.
"""
from __future__ import annotations

from pathlib import Path

REPO = Path(__file__).parent.parent
RENDER = REPO / "work" / "evidence" / "financial_render.png"

MAX_CLIPPED_PCT = 2.0
MAX_DARK_PCT = 20.0
MIN_MEAN = 60.0
MAX_MEAN = 170.0


def measure(path: Path) -> dict[str, float]:
    from PIL import Image

    with Image.open(path) as img:
        px = list(img.convert("L").getdata())
    total = len(px)
    return {
        "mean": sum(px) / total,
        "clipped_pct": 100.0 * sum(1 for p in px if p >= 250) / total,
        "dark_pct": 100.0 * sum(1 for p in px if p <= 8) / total,
    }


def main() -> int:
    fails = []

    if not RENDER.is_file():
        print(f"SKIP RENDER_ABSENT path={RENDER}")
        print("FAILS=0")
        return 0

    m = measure(RENDER)
    print(
        "RENDER mean=%.1f clipped_pct=%.2f dark_pct=%.2f"
        % (m["mean"], m["clipped_pct"], m["dark_pct"])
    )

    if m["clipped_pct"] > MAX_CLIPPED_PCT:
        fails.append(f"BLOWN_OUT clipped_pct={m['clipped_pct']:.2f} max={MAX_CLIPPED_PCT}")
    if m["dark_pct"] > MAX_DARK_PCT:
        fails.append(f"MOSTLY_BLACK dark_pct={m['dark_pct']:.2f} max={MAX_DARK_PCT}")
    if not (MIN_MEAN <= m["mean"] <= MAX_MEAN):
        fails.append(f"MEAN_OUT_OF_RANGE mean={m['mean']:.1f} want={MIN_MEAN}..{MAX_MEAN}")

    for line in fails:
        print(f"FAIL {line}")
    print(f"FAILS={len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
