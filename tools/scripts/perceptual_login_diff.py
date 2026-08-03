#!/usr/bin/env python3
"""Perceptual diff for the fidelity-oracle login screenshot row.

Reference is the 2011 login background still (Content/Movies/Login/
Login_BG_AI_still.png, 5120x3200) — the actual visual content the UE login
stage reproduces. Candidate is the captured UE frontend Login frame
(work/logs/fidelity/frontend_login_capture.png, windowed; client area cropped).

Contract mirrors frontend_screenshot_oracle.json: sample_stride 8,
pixel_tolerance 0.12, max_mean_absolute_error 0.18, max_changed_fraction 0.65.
Both normalized to 1600x900 (center-crop the 16:10 still to 16:9) before
sampling. Per-sample diff = max channel |ref-cand| / 255; a sample is changed
when diff > pixel_tolerance. PASS requires MAE <= max_mean_absolute_error and
changed_fraction <= max_changed_fraction. Result JSON written to OUT.
"""
from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image

REFERENCE = Path("Content/Movies/Login/Login_BG_AI_still.png")
CANDIDATE = Path("work/logs/fidelity/frontend_login_capture.png")
OUT = Path("work/logs/fidelity/frontend_login_diff.json")
W, H = 1600, 900
STRIDE = 8
PIXEL_TOL = 0.12
MAX_MAE = 0.18
MAX_CHANGED = 0.65


def load_reference(path: Path) -> Image.Image:
    img = Image.open(path).convert("RGB")
    # Center-crop 16:10 -> 16:9, then downscale to 1600x900.
    tw = int(img.height * W / H)
    left = (img.width - tw) // 2
    return img.crop((left, 0, left + tw, img.height)).resize((W, H), Image.LANCZOS)


def load_candidate(path: Path) -> Image.Image:
    img = Image.open(path).convert("RGB")
    # Windowed capture: crop the client area (borders left 8 / top 31) to 1600x900.
    client = img.crop((8, 31, 8 + W, 31 + H)) if img.width >= 8 + W and img.height >= 31 + H else img
    return client.resize((W, H), Image.LANCZOS)


def main() -> int:
    ref = load_reference(REFERENCE)
    cand = load_candidate(CANDIDATE)
    rp = ref.load()
    cp = cand.load()

    total = 0
    changed = 0
    mae_sum = 0.0
    for y in range(0, H, STRIDE):
        for x in range(0, W, STRIDE):
            rr, rg, rb = rp[x, y]
            cr, cg, cb = cp[x, y]
            d = max(abs(rr - cr), abs(rg - cg), abs(rb - cb)) / 255.0
            mae_sum += d
            if d > PIXEL_TOL:
                changed += 1
            total += 1

    mae = mae_sum / total
    changed_fraction = changed / total
    ok = mae <= MAX_MAE and changed_fraction <= MAX_CHANGED

    result = {
        "schema_version": 1,
        "comparison": "perceptual_image_diff",
        "reference": str(REFERENCE),
        "reference_note": "2011 login background still (5120x3200, center-cropped to 16:9)",
        "candidate": str(CANDIDATE),
        "candidate_note": "UE frontend Login hold capture, client area cropped",
        "normalized_resolution": {"width": W, "height": H},
        "sample_stride": STRIDE,
        "pixel_tolerance": PIXEL_TOL,
        "max_mean_absolute_error": MAX_MAE,
        "max_changed_fraction": MAX_CHANGED,
        "samples": total,
        "changed_samples": changed,
        "mean_absolute_error": round(mae, 4),
        "changed_fraction": round(changed_fraction, 4),
        "result": "pass" if ok else "fail",
    }
    OUT.write_text(json.dumps(result, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"DIFF mae={mae:.4f} changed={changed_fraction:.4f} result={'PASS' if ok else 'FAIL'} samples={total}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
