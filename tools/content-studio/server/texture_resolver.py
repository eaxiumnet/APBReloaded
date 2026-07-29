from __future__ import annotations

import re
from pathlib import Path
from typing import TypedDict

from material_instance import Color, MaterialInstance, parse_material_instance

from names import _model_key, _normstr

# _DiffSpec is APB's packed diffuse+spec (RGB diffuse, A spec), a fallback baseColor
# source. Selection tiers (see _select_base_color): exact-core plain, then exact-core
# DiffSpec, then sole plain, then sole DiffSpec -- an exact-core DiffSpec outranks a
# non-exact plain diffuse.
_DIFFUSE_SUFFIX = re.compile(r"_(?:Diff|Diffuse)\.tga$", re.IGNORECASE)
_DIFFSPEC_SUFFIX = re.compile(r"_DiffSpec\.tga$", re.IGNORECASE)
_NORMAL_SUFFIX = re.compile(r"_(?:Norm|NRM|Normal)\.tga$", re.IGNORECASE)
_SPECULAR_SUFFIX = re.compile(r"_(?:Spec|SpecMask|RefMask)\.tga$", re.IGNORECASE)
_OPACITY_SUFFIX = re.compile(r"_Opac\.tga$", re.IGNORECASE)
_MASK1_SUFFIX = re.compile(r"_Mask1\.tga$", re.IGNORECASE)
_MASK2_SUFFIX = re.compile(r"_Mask2\.tga$", re.IGNORECASE)
_EMISSIVE_SUFFIX = re.compile(r"_(?:Emis|Emissive)\.tga$", re.IGNORECASE)


class TextureResolution(TypedDict, total=False):
    baseColor: Path
    normal: Path
    specular: Path
    opacity: Path
    mask1: Path
    mask2: Path
    emissive: Path
    skin_colors: dict[str, Color]
    skin_scalars: dict[str, float]
    skin_stencil: Path


def _find_design_folder(psk: Path) -> Path | None:
    """The weapon design folder is the direct child of ``WeaponsBase`` on the path."""
    for ancestor in psk.parents:
        if ancestor.parent is not None and ancestor.parent.name.casefold() == "weaponsbase":
            return ancestor
    return None


def _owned_texture_dir(design: Path) -> Path | None:
    """Pick the single ``Texture2D`` whose parent identifies THIS weapon.

    A design folder holds many ``Texture2D`` dirs (shared props, skins, water, plus
    per-weapon ones). We only accept a texture dir whose immediate parent folder
    matches the design folder by class-aware model-key (or, when the model-key is
    empty because the whole name is a class token like ``MachinePistol``, by
    normalized string). If zero or 2+ match we return None: staying grey is correct,
    borrowing a neighbouring weapon's textures is a mislabel.
    """
    design_model = _model_key(design.name)
    design_norm = _normstr(design.name)
    matches = []
    for texture_dir in design.rglob("Texture2D"):
        if not texture_dir.is_dir():
            continue
        parent = texture_dir.parent.name
        parent_model = _model_key(parent)
        if (design_model and parent_model == design_model) or _normstr(parent) == design_norm:
            matches.append(texture_dir.resolve())
    unique = sorted(set(matches))
    return unique[0] if len(unique) == 1 else None


def _resolve_skin(design: Path, skin: str) -> Path | None:
    """Resolve a material-instance skin ID, returning None when it no longer exists."""
    target = (design / skin).resolve()
    try:
        target.relative_to(design.resolve())
    except ValueError as exc:
        raise ValueError("skin path escapes weapon design") from exc
    if not target.name.casefold().endswith("_mat_inst.props.txt"):
        raise ValueError(f"not a material-instance skin: {skin}")
    if not target.is_file():
        return None
    return target


def _find_stencil(design: Path, material: MaterialInstance) -> Path | None:
    for parameter, texture in material.textures.items():
        if "pattern" not in parameter.casefold() or texture.casefold() == "none":
            continue
        name = texture.rsplit(".", maxsplit=1)[-1]
        candidate = design / "WeaponSkins" / "Texture2D" / f"{name}.tga"
        if candidate.is_file():
            return candidate.resolve()
    return None


def _strip_weapon_prefix(name: str) -> str:
    return name[len("weapon_"):] if name.startswith("weapon_") else name


def _core(name: str, suffix_re: re.Pattern[str]) -> str:
    return _strip_weapon_prefix(suffix_re.sub("", name).casefold())


def _pick_normal(normals: list[Path], stem_core: str, exact: bool) -> Path | None:
    if exact:
        same_core = [p for p in normals if _core(p.name, _NORMAL_SUFFIX) == stem_core]
        return same_core[0] if len(same_core) == 1 else None
    return normals[0] if len(normals) == 1 else None


def _pick_specular(specs: list[Path], stem_core: str, exact: bool) -> Path | None:
    if exact:
        same_core = [p for p in specs if _core(p.name, _SPECULAR_SUFFIX) == stem_core]
        return same_core[0] if len(same_core) == 1 else None
    return specs[0] if len(specs) == 1 else None


def _pick_opacity(opacities: list[Path], stem_core: str, exact: bool) -> Path | None:
    if exact:
        same_core = [p for p in opacities if _core(p.name, _OPACITY_SUFFIX) == stem_core]
        return same_core[0] if len(same_core) == 1 else None
    return opacities[0] if len(opacities) == 1 else None


def _select_base_color(
    diffuse: list[Path], diffspec: list[Path], stem_core: str
) -> tuple[Path, bool] | None:
    """Collision-safe baseColor pick, or None when the dir is ambiguous.

    Strict precedence: exact-core plain -> exact-core DiffSpec -> sole plain ->
    sole DiffSpec. 2+ candidates at the winning tier => omit rather than guess.
    Returns (path, is_exact) so the normal is paired with matching strictness.
    """
    for pool, suffix_re in ((diffuse, _DIFFUSE_SUFFIX), (diffspec, _DIFFSPEC_SUFFIX)):
        exact = [p for p in pool if _core(p.name, suffix_re) == stem_core]
        if exact:
            return (exact[0], True) if len(exact) == 1 else None
    for pool in (diffuse, diffspec):
        if len(pool) == 1:
            return pool[0], False
    return None


def find_default_textures(psk_path: Path, skin: str | None = None) -> TextureResolution:
    psk = psk_path.resolve()
    design = _find_design_folder(psk)

    if skin and design is not None:
        default = find_default_textures(psk)
        material_path = _resolve_skin(design, skin)
        if material_path is None:
            return default
        material = parse_material_instance(material_path)
        default["skin_colors"] = material.vectors
        default["skin_scalars"] = material.scalars
        stencil = _find_stencil(design, material)
        if stencil is not None:
            default["skin_stencil"] = stencil
        return default

    texture_dir = _owned_texture_dir(design) if design is not None else None
    if texture_dir is None:
        return {}

    files = sorted(
        (p for p in texture_dir.iterdir() if p.is_file() and p.suffix.casefold() == ".tga"),
        key=lambda p: p.name.casefold(),
    )
    stem_core = _strip_weapon_prefix(re.sub(r"_LOD\d+$", "", psk.stem).casefold())
    all_normal = [p for p in files if _NORMAL_SUFFIX.search(p.name)]
    all_diffuse = [p for p in files if _DIFFUSE_SUFFIX.search(p.name)]
    all_diffspec = [p for p in files if _DIFFSPEC_SUFFIX.search(p.name)]
    all_specular = [p for p in files if _SPECULAR_SUFFIX.search(p.name)]
    all_opacity = [p for p in files if _OPACITY_SUFFIX.search(p.name)]
    all_mask1 = [p for p in files if _MASK1_SUFFIX.search(p.name)]
    all_mask2 = [p for p in files if _MASK2_SUFFIX.search(p.name)]
    all_emissive = [p for p in files if _EMISSIVE_SUFFIX.search(p.name)]

    selected = _select_base_color(all_diffuse, all_diffspec, stem_core)
    if selected is None:
        return {}
    base_color, is_exact = selected
    normal = _pick_normal(all_normal, stem_core, is_exact)
    specular = _pick_specular(all_specular, stem_core, is_exact)
    opacity = _pick_opacity(all_opacity, stem_core, is_exact)
    mask1 = all_mask1[0] if len(all_mask1) == 1 else None
    mask2 = all_mask2[0] if len(all_mask2) == 1 else None
    emissive = all_emissive[0] if len(all_emissive) == 1 else None

    textures: TextureResolution = {"baseColor": base_color.resolve()}
    if normal is not None:
        textures["normal"] = normal.resolve()
    if specular is not None:
        textures["specular"] = specular.resolve()
    if opacity is not None:
        textures["opacity"] = opacity.resolve()
    if mask1 is not None:
        textures["mask1"] = mask1.resolve()
    if mask2 is not None:
        textures["mask2"] = mask2.resolve()
    if emissive is not None:
        textures["emissive"] = emissive.resolve()
    return textures
