from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from assets import discover_skins  # noqa: E402


def _make_design(tmp_path: Path, skin_files: list[str]) -> Path:
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    tex = design / "WeaponSkins" / "Texture2D"
    tex.mkdir(parents=True)
    for name in skin_files:
        (tex / name).write_bytes(b"\x00")
    return design


def test_discovers_skins_as_id_label_pairs(tmp_path: Path) -> None:
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    materials = design / "Weapon_X" / "MaterialInstanceConstant"
    materials.mkdir(parents=True)
    for name in ("Camo_01", "Biohazard"):
        (materials / f"{name}_MAT_INST.props.txt").write_text("Parent = Material3'Base'")
    skins = discover_skins(design)
    ids = {s["id"] for s in skins}
    assert ids == {
        "Weapon_X/MaterialInstanceConstant/Camo_01_MAT_INST.props.txt",
        "Weapon_X/MaterialInstanceConstant/Biohazard_MAT_INST.props.txt",
    }
    labels = {s["id"]: s["label"] for s in skins}
    assert labels["Weapon_X/MaterialInstanceConstant/Camo_01_MAT_INST.props.txt"] == "Camo 01"
    assert labels["Weapon_X/MaterialInstanceConstant/Biohazard_MAT_INST.props.txt"] == "Biohazard"


def test_discovers_distinct_material_instance_skins(tmp_path: Path) -> None:
    # Given
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    materials = design / "Weapon_X" / "MaterialInstanceConstant"
    materials.mkdir(parents=True)
    for name in ("Weapon_X_Bloodrose", "Weapon_X_BloodroseAlt"):
        (materials / f"{name}_MAT_INST.props.txt").write_text("Parent = Material3'Base'")
    # When
    skins = discover_skins(design)
    # Then
    assert {skin["id"] for skin in skins} == {
        "Weapon_X/MaterialInstanceConstant/Weapon_X_Bloodrose_MAT_INST.props.txt",
        "Weapon_X/MaterialInstanceConstant/Weapon_X_BloodroseAlt_MAT_INST.props.txt",
    }


def test_skins_sorted_by_label(tmp_path: Path) -> None:
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    materials = design / "MaterialInstanceConstant"
    materials.mkdir(parents=True)
    for name in ("Zebra", "Apple", "Mango"):
        (materials / f"{name}_MAT_INST.props.txt").write_text("Parent = Material3'Base'")
    skins = discover_skins(design)
    assert [s["label"] for s in skins] == ["Apple", "Mango", "Zebra"]


def test_only_material_instance_files_are_skins(tmp_path: Path) -> None:
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    materials = design / "MaterialInstanceConstant"
    materials.mkdir(parents=True)
    (materials / "Camo_01_MAT_INST.props.txt").write_text("Parent = Material3'Base'")
    (materials / "Weapon_SuperShader_MASTER_MAT.mat").write_bytes(b"\x00")
    (materials / "notes.props.txt").write_bytes(b"\x00")
    skins = discover_skins(design)
    assert [s["id"] for s in skins] == ["MaterialInstanceConstant/Camo_01_MAT_INST.props.txt"]


def test_no_weaponskins_dir_returns_empty(tmp_path: Path) -> None:
    design = tmp_path / "WeaponsBase" / "Weapon_NoSkins"
    (design / "Weapon_NoSkins" / "SkeletalMesh3").mkdir(parents=True)
    assert discover_skins(design) == []


REPO_ROOT = Path(__file__).resolve().parents[4]
ASSAULT_RIFLE = REPO_ROOT / "Content/Extracted/WeaponsBase/Weapon_AssaultRifle"


def test_real_assault_rifle_has_skins() -> None:
    skins = discover_skins(ASSAULT_RIFLE)
    assert len(skins) > 50
    assert all(s["id"].endswith("_MAT_INST.props.txt") for s in skins)
    assert all(s["label"] for s in skins)
