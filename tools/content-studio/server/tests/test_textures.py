from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from texture_resolver import find_default_textures  # noqa: E402


def _make_pkg(tmp_path: Path, tex_files: list[str]) -> Path:
    pkg = tmp_path / "WeaponsBase" / "Weapon_X" / "Weapon_X"
    (pkg / "SkeletalMesh3").mkdir(parents=True)
    tex = pkg / "Texture2D"
    tex.mkdir()
    for name in tex_files:
        (tex / name).write_bytes(b"\x00")
    psk = pkg / "SkeletalMesh3" / "Weapon_X_LOD0.psk"
    psk.write_bytes(b"\x00")
    return psk


def test_selects_owned_dir_and_ignores_sibling_weapon(tmp_path: Path) -> None:
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    own = design / "Weapon_X" / "Texture2D"
    own.mkdir(parents=True)
    (own / "Weapon_X_Diffuse.tga").write_bytes(b"\x00")
    (own / "Weapon_X_Normal.tga").write_bytes(b"\x00")
    foreign = design / "Weapon_TommyGun" / "Texture2D"
    foreign.mkdir(parents=True)
    (foreign / "Weapon_TommyGun_Diff.tga").write_bytes(b"\x00")
    psk_dir = design / "Weapon_X" / "SkeletalMesh3"
    psk_dir.mkdir(parents=True)
    psk = psk_dir / "Weapon_X_LOD0.psk"
    psk.write_bytes(b"\x00")
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_Diffuse.tga"
    assert textures["normal"].name == "Weapon_X_Normal.tga"


def test_exact_stem_pairs_own_normal(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_Norm.tga", "Weapon_Xtra_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"
    assert textures["normal"].name == "Weapon_X_Norm.tga"


def test_exact_stem_never_borrows_foreign_normal(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_Xtra_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"
    assert "normal" not in textures, textures


def test_two_stem_normals_are_ambiguous_omit(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_Norm.tga", "Weapon_X_NRM.tga"])
    textures = find_default_textures(psk)
    assert "normal" not in textures, textures


def test_renamed_single_weapon_dir_pairs_via_fallback(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_Atac_Diff.tga", "Weapon_Atac_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_Atac_Diff.tga"
    assert textures["normal"].name == "Weapon_Atac_Norm.tga"

REPO_ROOT = Path(__file__).resolve().parents[4]
TOMMY_GUN = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_TommyGun/Weapon_TommyGun"
    / "SkeletalMesh3/Weapon_TommyGun_LOD0.psk"
)


def test_tommy_gun_default_textures() -> None:
    textures = find_default_textures(TOMMY_GUN)

    assert textures["baseColor"].name.casefold() == "weapon_tommygun_diff.tga"
    assert textures["normal"].name.casefold() == "weapon_tommygun_norm.tga"
    assert textures["baseColor"].is_absolute()
    assert textures["normal"].is_absolute()


def _make_pkg_with_skins(tmp_path: Path, skins: list[str]) -> Path:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_Norm.tga"])
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    skin_dir = design / "WeaponSkins" / "Texture2D"
    skin_dir.mkdir(parents=True)
    for name in skins:
        (skin_dir / name).write_bytes(b"\x00")
    return psk


def test_skin_material_keeps_default_base_color(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    material = design / "MaterialInstanceConstant" / "Camo_MAT_INST.props.txt"
    material.parent.mkdir()
    material.write_text("Parent = Material3'Base'")
    textures = find_default_textures(psk, skin="MaterialInstanceConstant/Camo_MAT_INST.props.txt")
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"
    assert textures["baseColor"].is_absolute()
    assert textures["normal"].name == "Weapon_X_Norm.tga"


def test_skin_material_keeps_base_and_resolves_colors_and_stencil(tmp_path: Path) -> None:
    # Given
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_Mask1.tga"])
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    materials = design / "Weapon_X" / "MaterialInstanceConstant"
    materials.mkdir(parents=True)
    material = materials / "Weapon_X_Bloodrose_MAT_INST.props.txt"
    material.write_text(
        "VectorParameterValues[0] = { ParameterValue = { R=.22 G=0 B=0 } ParameterName = Body Pattern Col A }\n"
        "TextureParameterValues[0] = { ParameterValue = Texture2D'Textures.Emblem' ParameterName = Pattern Body Adjustable }"
    )
    stencil = design / "WeaponSkins" / "Texture2D" / "Emblem.tga"
    stencil.parent.mkdir(parents=True)
    stencil.write_bytes(b"\x00")
    # When
    textures = find_default_textures(
        psk, skin="Weapon_X/MaterialInstanceConstant/Weapon_X_Bloodrose_MAT_INST.props.txt"
    )
    # Then
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"
    assert textures["skin_colors"]["Body Pattern Col A"][:3] == (0.22, 0.0, 0.0)
    assert textures["skin_stencil"] == stencil.resolve()


def test_missing_skin_material_degrades_to_default_textures(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga"])
    textures = find_default_textures(psk, skin="Weapon_X/Missing_MAT_INST.props.txt")
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"


def test_skin_none_keeps_default_diffuse(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    textures = find_default_textures(psk, skin=None)
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"


def test_skin_path_traversal_rejected(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    (tmp_path / "secret.tga").write_bytes(b"\x00")
    try:
        find_default_textures(psk, skin="../../../secret.tga")
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError for traversal skin path")


def test_missing_skin_material_degrades_to_default(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    textures = find_default_textures(psk, skin="MaterialInstanceConstant/DoesNotExist_MAT_INST.props.txt")
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"


def test_skin_non_material_instance_rejected(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    design = tmp_path / "WeaponsBase" / "Weapon_X"
    (design / "WeaponSkins" / "Texture2D" / "readme.txt").write_bytes(b"\x00")
    try:
        find_default_textures(psk, skin="WeaponSkins/Texture2D/readme.txt")
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError for non-material-instance skin path")


def test_skin_path_traversal_rejected(tmp_path: Path) -> None:
    psk = _make_pkg_with_skins(tmp_path, ["Camo_01.tga"])
    try:
        find_default_textures(psk, skin="../../../secret_MAT_INST.props.txt")
    except ValueError:
        pass
    else:
        raise AssertionError("expected ValueError for skin outside the weapon design")


def test_diffspec_used_when_no_plain_diffuse(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_DiffSpec.tga", "Weapon_X_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_DiffSpec.tga"
    assert textures["normal"].name == "Weapon_X_Norm.tga"


def test_plain_diffuse_beats_diffspec(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_DiffSpec.tga", "Weapon_X_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"


def test_exact_diffspec_beats_variant_plain_diffuse(tmp_path: Path) -> None:
    # precedence: exact plain -> exact DiffSpec -> sole plain -> sole DiffSpec
    psk = _make_pkg(
        tmp_path,
        ["Weapon_X_Joker_Diff.tga", "Weapon_X_DiffSpec.tga", "Weapon_X_Norm.tga"],
    )
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_DiffSpec.tga"
    assert textures["normal"].name == "Weapon_X_Norm.tga"


def test_two_nonstem_diffspec_is_ambiguous_omit(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Alpha_DiffSpec.tga", "Beta_DiffSpec.tga"])
    textures = find_default_textures(psk)
    assert "baseColor" not in textures, textures


def test_stem_diff_wins_over_variant_diff(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Diff.tga", "Weapon_X_Joker_Diff.tga", "Weapon_X_Norm.tga"])
    textures = find_default_textures(psk)
    assert textures["baseColor"].name == "Weapon_X_Diff.tga"
    assert textures["normal"].name == "Weapon_X_Norm.tga"


def test_variant_diff_without_stem_diff_stays_ambiguous(tmp_path: Path) -> None:
    psk = _make_pkg(tmp_path, ["Weapon_X_Joker_Diff.tga", "Weapon_X_CB_Diff.tga"])
    textures = find_default_textures(psk)
    assert "baseColor" not in textures, textures


KATANA = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_Katana/Weapon_Katana"
    / "SkeletalMesh3/Weapon_Katana_LOD0.psk"
)
FLARE_GUN = (
    REPO_ROOT
    / "Content/Extracted/WeaponsBase/Weapon_FlareGun/Weapon_FlareGun"
    / "SkeletalMesh3/Weapon_FlareGun_LOD0.psk"
)


def test_katana_diffspec_fallback() -> None:
    textures = find_default_textures(KATANA)
    assert textures["baseColor"].name.casefold() == "weapon_katana_diffspec.tga"
    assert textures["normal"].name.casefold() == "weapon_katana_norm.tga"


def test_flare_gun_picks_stem_diff_not_joker() -> None:
    textures = find_default_textures(FLARE_GUN)
    assert textures["baseColor"].name.casefold() == "weapon_flaregun_diff.tga"
    assert textures["normal"].name.casefold() == "weapon_flaregun_norm.tga"


def test_alig_skin_uses_its_referenced_variant_textures() -> None:
    mesh = (
        REPO_ROOT
        / "Content/Extracted/WeaponsBase/Weapon_Armas_LightMachineGun_762"
        / "Weapon_Armas_LightMachineGun_762/SkeletalMesh3/Weapon_LightMachineGun_762_LOD0.psk"
    )
    skin = (
        "Weapon_Armas_LightMachineGun_762/MaterialInstanceConstant/"
        "Weapon_LightMachineGun_762_Propaganda_MAT_INST.props.txt"
    )
    textures = find_default_textures(mesh, skin=skin)
    assert textures["baseColor"].name == "Weapon_LightMachineGun_556_Diff.tga"
    assert textures["normal"].name == "Weapon_LightMachineGun_556_Norm.tga"
