"""Asset discovery + safe path resolution for the content studio backend.

Pure functions (no FastAPI) so they can be unit-tested directly. The weapon
catalog is derived from the on-disk extracted layout:

    WeaponsBase/<DesignFolder>/**/SkeletalMesh3/*LOD0.psk
                              /**/StaticMesh3/*LOD0.pskx
                              /**/Texture2D/*.tga

Each design folder is one "weapon"; its LOD0 meshes are the selectable parts.
"""

from __future__ import annotations

import os
import re
from pathlib import Path

from _alias_data import PRIMARY_MESH
from names import WeaponNameResolver, default_resolver

_MESH_EXTS = {".psk", ".pskx"}
_LOD_SUFFIX = re.compile(r"_LOD\d+$", re.IGNORECASE)


def strip_part_label(stem: str, folder: str) -> str:
    """Human label for a variant mesh: the part of the stem past the weapon name.

    ``Weapon_ATac_Bodyguard_LOD0`` under folder ``Weapon_ATac`` -> ``Bodyguard``;
    the bare weapon mesh (no suffix) -> ``Default``. A stem that does not share the
    folder prefix (differently-named mesh) is prettified whole.
    """
    core = _LOD_SUFFIX.sub("", stem)
    if core == folder:
        return "Default"
    prefix = f"{folder}_"
    if core.startswith(prefix):
        remainder = core[len(prefix):]
        return remainder.replace("_", " ") if remainder else "Default"
    return core.replace("_", " ")


def discover_skins(design: Path) -> list[dict]:
    """List selectable material-instance skins under a weapon design folder."""
    design = Path(design)
    skins = [
        {
            "id": f.relative_to(design).as_posix(),
            "label": f.name.removesuffix("_MAT_INST.props.txt").replace("_", " "),
        }
        for f in design.rglob("*_MAT_INST.props.txt")
        if f.is_file()
    ]
    return sorted(skins, key=lambda s: s["label"].casefold())


def _iter_lod0_meshes(folder: Path):
    """Yield LOD0 mesh files in a design folder (fallback: any mesh if no LOD0)."""
    all_meshes = [f for f in folder.rglob("*") if f.suffix.lower() in _MESH_EXTS]
    lod0 = [f for f in all_meshes if "LOD0" in f.name]
    return sorted(lod0 or all_meshes, key=lambda f: f.name)


def _select_primary(parts: list[dict], folder_name: str) -> str:
    override = PRIMARY_MESH.get(folder_name)
    if override:
        token = override.casefold()
        matched = [p for p in parts if token in p["name"].casefold()]
        if matched:
            return max(matched, key=lambda p: p["bytes"])["id"]
    return max(parts, key=lambda p: p["bytes"])["id"]


def build_weapon_catalog(
    base_dir: Path, resolver: WeaponNameResolver | None = None
) -> list[dict]:
    """Scan *base_dir* (WeaponsBase) and return one entry per design folder.

    Entry: {id, folder, display, name_confidence, sapbdb, primary, parts:[{id, name, bytes}]}.
    ``id``/part ``id`` are posix relpaths under *base_dir* (stable, URL-safe as a
    ``path=`` query value); ``display`` is the resolved real weapon name.
    """
    base = Path(base_dir).resolve()
    if not base.is_dir():
        return []
    resolver = resolver or default_resolver()

    weapons: list[dict] = []
    for folder in sorted(p for p in base.iterdir() if p.is_dir()):
        parts = []
        for f in _iter_lod0_meshes(folder):
            parts.append({
                "id": f.relative_to(base).as_posix(),
                "name": f.stem,
                "label": strip_part_label(f.stem, folder.name),
                "bytes": f.stat().st_size,
            })
        if not parts:
            continue
        primary = _select_primary(parts, folder.name)
        resolved = resolver.resolve(folder.name, Path(primary).stem)
        weapons.append({
            "id": folder.name,
            "folder": folder.name,
            "display": resolved.display,
            "name_confidence": resolved.confidence,
            "sapbdb": resolved.sapbdb,
            "primary": primary,
            "parts": parts,
            "skins": discover_skins(folder),
        })
    return weapons


def resolve_mesh_path(base_dir: Path, relpath: str) -> Path:
    """Resolve a catalog mesh id to an absolute path, rejecting traversal.

    Raises ValueError (bad path / not a mesh) or FileNotFoundError (missing).
    """
    base = Path(base_dir).resolve()
    target = (base / relpath).resolve()
    # containment check (target must be inside base)
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes asset root")
    if target.suffix.lower() not in _MESH_EXTS:
        raise ValueError(f"not a mesh file: {relpath}")
    if not target.is_file():
        raise FileNotFoundError(relpath)
    return target
