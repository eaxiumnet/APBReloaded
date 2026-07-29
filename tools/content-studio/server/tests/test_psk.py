"""Tests for the ActorX .psk parser against a real extracted APB mesh."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from psk import parse_psk_file  # noqa: E402

# Real extracted mesh whose chunk layout was measured directly:
#   PNTS0000 x24 (stride 12) · VTXW0000 x24 (stride 16) · FACE0000 x12 (stride 12) · MATT0000 x1
REPO_ROOT = Path(__file__).resolve().parents[4]
MAGNUM = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_Armas_Magnum/Weapon_Armas_Magnum"
    / "SkeletalMesh3/Crm_Magnum_Clip_mk3_LOD0.psk"
)


def test_magnum_counts() -> None:
    mesh = parse_psk_file(MAGNUM)
    assert len(mesh.points) == 24, f"points={len(mesh.points)}"
    assert len(mesh.wedges) == 24, f"wedges={len(mesh.wedges)}"
    assert len(mesh.faces) == 12, f"faces={len(mesh.faces)}"
    assert mesh.triangle_count == 12
    assert mesh.materials == ["Crm_Magnum_Mk3_MAT"], f"materials={mesh.materials}"


def test_indices_in_range() -> None:
    mesh = parse_psk_file(MAGNUM)
    # every wedge references a valid point; every face references a valid wedge
    for pidx, u, v, _m in mesh.wedges:
        assert 0 <= pidx < len(mesh.points), f"wedge point idx {pidx} out of range"
        assert isinstance(u, float) and isinstance(v, float)
    for w0, w1, w2, _m in mesh.faces:
        for w in (w0, w1, w2):
            assert 0 <= w < len(mesh.wedges), f"face wedge idx {w} out of range"


def test_points_are_finite_vectors() -> None:
    import math

    mesh = parse_psk_file(MAGNUM)
    for x, y, z in mesh.points:
        assert all(math.isfinite(c) for c in (x, y, z)), "non-finite vertex"


# --- standalone runner (repo FAILS=N convention; no pytest required) ---
if __name__ == "__main__":
    fails = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"PASS: {name}")
            except Exception as exc:  # noqa: BLE001
                fails += 1
                print(f"FAIL: {name}: {exc}")
    print(f"FAILS={fails}")
    sys.exit(1 if fails else 0)
