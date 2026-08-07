"""Tests for the ActorX .psk parser against a real extracted APB mesh."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from psk import (_rotate, bone_world_transforms,  # noqa: E402
                 parse_psk_file, parse_skeleton)

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


def test_rotate_matches_full_quat_product() -> None:
    """_rotate must equal q*(0,v)*q^-1; a sign bug once flipped the result."""
    q = (0.7071067811865476, 0.0, 0.0, 0.7071067811865476)  # 90 deg about X
    v = (6.866, 0.529, 0.0)
    # expected: (x, -z, y) under the rotation
    out = _rotate(v, q)
    assert abs(out[0] - 6.866) < 1e-6, f"x={out[0]}"
    assert abs(out[1] - 0.0) < 1e-6, f"y={out[1]}"
    assert abs(out[2] - 0.529) < 1e-6, f"z={out[2]}"

    # length must be preserved for any unit quat
    import random

    for _ in range(200):
        rq = [random.uniform(-1, 1) for _ in range(4)]
        n = (rq[0] ** 2 + rq[1] ** 2 + rq[2] ** 2 + rq[3] ** 2) ** 0.5
        rq = tuple(c / n for c in rq)
        rv = (random.uniform(-50, 50), random.uniform(-50, 50), random.uniform(-50, 50))
        out = _rotate(rv, rq)
        length = sum(c * c for c in rv) ** 0.5
        out_len = sum(c * c for c in out) ** 0.5
        assert abs(out_len - length) < 1e-4, f"length not preserved: {length} vs {out_len}"


def test_bone_world_transforms_bind_consistency() -> None:
    """World chain must reproduce the raw local transform for the root and
    satisfy world(child) = world(parent) + rotate(local, parent_world_q)."""
    skel_path = (
        REPO_ROOT
        / "Content/Extracted/CharactersBulk/F_Body_Base/F_Body_Base"
        / "SkeletalMesh3/F_Body_Base.psk"
    )
    skeleton = parse_skeleton(skel_path.read_bytes())
    worlds = bone_world_transforms(skeleton)
    assert len(worlds) == len(skeleton)
    # root: world == local
    root = skeleton[0]
    assert worlds[0][0] == root.position
    # spine below pelvis: world = pelvis_world + q_pelvis * local
    by_name = {b.name: (i, b) for i, b in enumerate(skeleton)}
    pelvis_i, pelvis = by_name["Bip01_Pelvis"]
    spine_i, spine = by_name["Bip01_Spine"]
    pelvis_pos, pelvis_q = worlds[pelvis_i]
    rotated = _rotate(spine.position, pelvis_q)
    expect = (pelvis_pos[0] + rotated[0], pelvis_pos[1] + rotated[1], pelvis_pos[2] + rotated[2])
    got = worlds[spine_i][0]
    assert all(abs(a - b) < 1e-3 for a, b in zip(expect, got)), f"spine world={got}"


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
