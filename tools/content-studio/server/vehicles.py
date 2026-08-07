"""Vehicle mesh catalog, sockets, and part catalog for extracted retail assets."""

from __future__ import annotations

import math
import os
import re
from functools import lru_cache
from pathlib import Path

from material_instance import parse_material_instance
from psa import PsaKey
from psk import parse_skeleton_file

MESH_EXTENSIONS = {".psk", ".pskx"}

_BASE_PREFIX = "Baked_"
_PART_PREFIX = "V_"

_FIELD_LINE = re.compile(
    r"^\s*(SocketName|BoneName|RelativeLocation|RelativeRotation|RelativeScale)"
    r"\s*=\s*(.+?)\s*$"
)
_SOCKET_START = re.compile(r"^\s*SocketName\s*=")
_VEC3 = re.compile(r"X=([-\d.eE]+),\s*Y=([-\d.eE]+),\s*Z=([-\d.eE]+)")
_ROT3 = re.compile(r"Yaw=([-\d.eE]+),\s*Pitch=([-\d.eE]+),\s*Roll=([-\d.eE]+)")


def _meshes(folder: Path) -> list[Path]:
    return sorted(
        (path for path in folder.rglob("*") if path.is_file() and path.suffix.casefold() in MESH_EXTENSIONS),
        key=lambda path: ("EditorVehicle" not in path.name, "LOD_Base_Mesh" not in path.name, path.name.casefold()),
    )


def _display_name(folder: str) -> str:
    return folder.removeprefix("Baked_").replace("_", " ").strip()


@lru_cache(maxsize=2)
def _build_part_catalog_cached(base: Path) -> tuple:
    """Cache the part catalog as an immutable tuple of dicts (lru_cache-safe)."""
    return tuple(_build_part_catalog_uncached(base))


def _build_part_catalog_uncached(base: Path) -> list[dict]:
    """Scan part folders into wheel/bumper/hood families (see build_part_catalog)."""
    base = Path(base).resolve()
    if not base.is_dir():
        return []
    families: dict[tuple[str, str, str], list[dict]] = {}
    canonical: dict[tuple[str, str], tuple[str, str]] = {}
    slot_casing: dict[tuple[str, str, str], str] = {}
    for folder in base.iterdir():
        if not folder.is_dir():
            continue
        prefix, vehicle_class, family, slot = _family_of(folder.name)
        if not prefix or not vehicle_class or not family or not slot:
            continue
        sm3 = folder / "SkeletalMesh3"
        part_mesh = sm3 / "PartMesh.psk"
        if not part_mesh.is_file():
            # Some retail part packages keep the source mesh name instead of
            # the PartMesh rename (e.g. every V_A_PerformanceSaloon_Wheel_*
            # stores SkeletalMesh_N.psk). Fall back to that single mesh so the
            # wheel variants are not silently dropped from the catalog.
            part_mesh = next(iter(sorted(sm3.glob("*.psk"))), None)
        if part_mesh is None or not part_mesh.is_file():
            continue
        # The canonical spelling belongs to the FAMILY (class+family), not the
        # slot: a lowercase-only slot (e.g. v_c_perf_DoorsInterior_1) must not
        # re-case the whole family's base away from the uppercase primary.
        family_key = (vehicle_class.casefold(), family.casefold())
        if family_key not in canonical or prefix == "V":
            canonical[family_key] = (vehicle_class, family)
        key = (vehicle_class.casefold(), family.casefold(), slot.casefold())
        if key not in slot_casing or prefix == "V":
            slot_casing[key] = slot
        variant = {
            "id": folder.name,
            "label": folder.name,
            "mesh": part_mesh.relative_to(base).as_posix(),
        }
        families.setdefault(key, []).append(variant)
    catalog = []
    for key, variants in sorted(families.items()):
        vehicle_class, family = canonical[key[:2]]
        slot = slot_casing[key]
        seen: set[str] = set()
        unique_variants = []
        for variant in sorted(variants, key=lambda item: item["id"].casefold()):
            if variant["id"].casefold() in seen:
                continue
            seen.add(variant["id"].casefold())
            unique_variants.append(variant)
        catalog.append({
            "family": family,
            "base": f"{_BASE_PREFIX}{vehicle_class}_{family}",
            "slot": slot,
            "display": f"{family} {slot.replace('_', ' ')}",
            "variants": unique_variants,
        })
    return catalog


@lru_cache(maxsize=2)
def _build_vehicle_catalog_cached(base: Path) -> tuple:
    """Cache the catalog as an immutable tuple of dicts (lru_cache-safe).

    Scanning 100+ vehicle folders with rglob takes ~4s cold; the catalog is
    read-only extracted data, so repeat requests serve the cached copy.
    """
    vehicles: list[dict] = []
    for folder in sorted((path for path in base.iterdir() if path.is_dir()), key=lambda path: path.name.casefold()):
        meshes = _meshes(folder)
        if not meshes:
            continue
        if not any(path.name in {"EditorVehicle.psk", "LOD_Base_Mesh.psk"} for path in meshes):
            continue
        parts = [
            {
                "id": path.relative_to(base).as_posix(),
                "name": path.stem,
                "label": path.stem.replace("_", " "),
                "bytes": path.stat().st_size,
            }
            for path in meshes
        ]
        vehicles.append({
            "id": folder.name,
            "display": _display_name(folder.name),
            "primary": parts[0]["id"],
            "parts": parts,
        })
    return tuple(vehicles)


def build_vehicle_catalog(base_dir: Path) -> list[dict]:
    base = Path(base_dir).resolve()
    if not base.is_dir():
        return []
    return list(_build_vehicle_catalog_cached(base))


def resolve_vehicle_mesh(base_dir: Path, relpath: str) -> Path:
    base = Path(base_dir).resolve()
    target = (base / relpath).resolve()
    if target != base and not str(target).startswith(str(base) + os.sep):
        raise ValueError("path escapes vehicle root")
    if target.suffix.casefold() not in MESH_EXTENSIONS:
        raise ValueError(f"not a vehicle mesh: {relpath}")
    if not target.is_file():
        raise FileNotFoundError(relpath)
    return target


def _texture_map(mesh_path: Path) -> dict[str, Path]:
    textures: dict[str, Path] = {}
    family = next(
        (ancestor for ancestor in mesh_path.parents if (ancestor / "Texture2D").is_dir()),
        mesh_path.parent.parent,
    )
    for path in family.rglob("*.tga"):
        textures.setdefault(path.stem.casefold(), path.resolve())
    return textures


def find_vehicle_textures(mesh_path: Path) -> dict[str, Path]:
    textures = _texture_map(mesh_path)
    resolved: dict[str, Path] = {}
    for key, suffixes in {
        "baseColor": ("exteriordiffuse", "diffuse"),
        "normal": ("exteriornormal", "normal"),
        "emissive": ("exterioremissive", "emissive"),
        "opacity": ("opacitymask", "opacity"),
    }.items():
        match = next((path for stem, path in textures.items() if any(stem == suffix for suffix in suffixes)), None)
        if match is not None:
            resolved[key] = match
    return resolved


def vehicle_materials(mesh_path: Path) -> tuple[dict[str, dict[str, Path]], dict[str, dict]]:
    family = next(
        (ancestor for ancestor in mesh_path.parents if (ancestor / "Texture2D").is_dir()),
        mesh_path.parent.parent,
    )
    textures = _texture_map(mesh_path)
    material_textures: dict[str, dict[str, Path]] = {}
    material_settings: dict[str, dict] = {}
    for props in family.rglob("MaterialInstanceConstant/*.props.txt"):
        name = props.stem.removesuffix(".props")
        instance = parse_material_instance(props)
        texture_set: dict[str, Path] = {}
        for parameter, value in instance.textures.items():
            if value.casefold() == "none":
                continue
            stem = value.rsplit(".", 1)[-1].casefold()
            path = textures.get(stem)
            if path is None:
                continue
            parameter_key = parameter.casefold()
            if parameter_key == "diffuse":
                texture_set["baseColor"] = path
            elif parameter_key == "normal":
                texture_set["normal"] = path
            elif parameter_key == "emissive":
                texture_set["emissive"] = path
        if name.casefold() == "wheelmat":
            mask = textures.get("defaultwheel_opacitymask")
            if mask is not None:
                texture_set["opacity"] = mask
            material_settings[name] = {"alpha_mode": "MASK", "alpha_cutoff": 0.333}
        elif name.casefold() == "glassmat":
            material_settings[name] = {
                "alpha_mode": "BLEND",
                "base_color_factor": [0.0, 0.047776, 0.3, 0.3],
                "double_sided": True,
            }
        material_textures[name] = texture_set
    return material_textures, material_settings


def _quat_multiply(a: tuple[float, float, float, float], b: tuple[float, float, float, float]) -> tuple[float, float, float, float]:
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return (
        aw * bx + ax * bw + ay * bz - az * by,
        aw * by - ax * bz + ay * bw + az * bx,
        aw * bz + ax * by - ay * bx + az * bw,
        aw * bw - ax * bx - ay * by - az * bz,
    )


def _rotate(quat: tuple[float, float, float, float], v: tuple[float, float, float]) -> tuple[float, float, float]:
    qx, qy, qz, qw = quat
    x, y, z = v
    ux, uy, uz = qx, qy, qz
    dot = ux * x + uy * y + uz * z
    cross = (uy * z - uz * y, uz * x - ux * z, ux * y - uy * x)
    return (
        x + 2.0 * (qw * cross[0] + dot * ux),
        y + 2.0 * (qw * cross[1] + dot * uy),
        z + 2.0 * (qw * cross[2] + dot * uz),
    )


def _rotator_to_quat(yaw: float, pitch: float, roll: float) -> tuple[float, float, float, float]:
    # Unreal rotator units: 65536 = 360 degrees; UE order yaw(Z) -> pitch(Y) -> roll(X)
    to_rad = math.tau / 65536.0
    cy, sy = math.cos(yaw * to_rad / 2), math.sin(yaw * to_rad / 2)
    cp, sp = math.cos(pitch * to_rad / 2), math.sin(pitch * to_rad / 2)
    cr, sr = math.cos(roll * to_rad / 2), math.sin(roll * to_rad / 2)
    return (
        cr * sp * sy - sr * cp * cy,
        -cr * sp * cy - sr * cp * sy,
        cr * cp * sy - sr * sp * cy,
        cr * cp * cy + sr * sp * sy,
    )


def _bone_world_transforms(mesh_path: Path) -> list[tuple[tuple[float, float, float], tuple[float, float, float, float]]]:
    """Bind-pose world transform (position, quat) per bone index."""
    bones = parse_skeleton_file(mesh_path)
    world: list[tuple[tuple[float, float, float], tuple[float, float, float, float]]] = []
    for bone in bones:
        if bone.parent == 0 and bone is bones[0]:
            world.append((bone.position, bone.quat))
        elif 0 <= bone.parent < len(world):
            p_pos, p_quat = world[bone.parent]
            pos = tuple(p_pos[i] + _rotate(p_quat, bone.position)[i] for i in range(3))
            world.append((pos, _quat_multiply(p_quat, bone.quat)))
        else:
            world.append((bone.position, bone.quat))
    return world


def parse_vehicle_sockets(props_path: Path) -> list[dict]:
    """Parse the Sockets[] blocks of a SkeletalMesh3 .props.txt dump.

    Socket blocks are indent-nested inside ``Sockets[N] = { ... }``; each socket
    is split on its ``SocketName =`` line and the following five field lines.
    """
    text = props_path.read_text(encoding="utf-8", errors="replace")
    lines = text.splitlines()
    sockets: list[dict] = []
    fields: dict[str, str] = {}
    collecting = False
    for line in lines:
        match = _FIELD_LINE.match(line)
        if match:
            name, value = match.group(1), match.group(2).strip()
            if name == "SocketName":
                if collecting and fields:
                    sockets.append(fields)
                fields = {"name": value}
                collecting = True
            elif collecting:
                fields[name] = value
    if collecting and fields:
        sockets.append(fields)
    results = []
    for fields_dict in sockets:
        loc = _VEC3.search(fields_dict.get("RelativeLocation", ""))
        rot = _ROT3.search(fields_dict.get("RelativeRotation", ""))
        results.append({
            "name": fields_dict.get("name", ""),
            "bone": fields_dict.get("BoneName", "").strip(),
            "location": tuple(float(v) for v in loc.groups()) if loc else (0.0, 0.0, 0.0),
            "rotation": tuple(float(v) for v in rot.groups()) if rot else (0.0, 0.0, 0.0),
        })
    return results


def vehicle_socket_transforms(mesh_path: Path) -> list[dict]:
    """Socket world transforms (position, quat) in Unreal space.

    Socket transform = bone world transform * socket relative transform.
    """
    props = mesh_path.with_suffix(".props.txt")
    if not props.is_file():
        return []
    bones = parse_skeleton_file(mesh_path)
    world = _bone_world_transforms(mesh_path)
    name_to_index = {bone.name: i for i, bone in enumerate(bones)}
    results: list[dict] = []
    for socket in parse_vehicle_sockets(props):
        bone = socket["bone"]
        index = name_to_index.get(bone)
        if index is None:
            continue
        b_pos, b_quat = world[index]
        loc = socket["location"]
        pos = tuple(b_pos[i] + _rotate(b_quat, loc)[i] for i in range(3))
        rel = _rotator_to_quat(*socket["rotation"])
        quat = _quat_multiply(b_quat, rel)
        results.append({
            "name": socket["name"],
            "bone": bone,
            "position": pos,
            "quat": quat,
        })
    return results


def _family_of(folder_name: str) -> tuple[str, str, str, str]:
    """(prefix, class, family, slot) tokens of a part folder name.

    Retail part folders appear in two casings: the primary set uses
    ``V_A_2DrCoupe_Wheels_1`` while re-exported extras use lowercase
    ``v_c_perf_Wheels_13``. The prefix check is case-insensitive so both land
    in the catalog; ``build_part_catalog`` merges them into one family.
    """
    parts = folder_name.split("_")
    if len(parts) < 4 or not folder_name[:2].casefold() == "v_":
        return "", "", "", ""
    return parts[0], parts[1], parts[2], parts[3]


def build_part_catalog(base_dir: Path) -> list[dict]:
    """Part families (wheels, bumpers, hoods...) with their variant packages.

    A part family is the token right after ``V_A_`` (e.g. ``2DrCoupe``) and the
    slot is the remaining stem (e.g. ``Wheels``). Each package is one variant.

    The class/family tokens are matched case-insensitively: retail carries both
    ``V_C_Perf_Wheels_1`` and lowercase re-export extras ``v_c_perf_Wheels_13``
    for the same family, and both must land on one catalog entry. The emitted
    base keeps the canonical casing from an uppercase ``V_`` folder so it still
    matches the ``Baked_<Class>_<Family>`` vehicle IDs.
    """
    base = Path(base_dir).resolve()
    if not base.is_dir():
        return []
    return list(_build_part_catalog_cached(base))


_WHEEL_SPIN_RATE = 30.0
_WHEEL_SPIN_FRAMES = 60  # 2s clip, one full revolution per loop
# The viewer's synthetic vehicle animset mirrors these constants (clip label
# + scrubber max); see App.tsx vehicleAnimset.


def build_wheelspin_clip(skeleton) -> dict:
    """Synthesize a wheel-spin animation clip for a vehicle skeleton.

    APB vehicle rigs (EditorVehicle.psk): the tire disc sits in the XZ plane
    with the axle along each wheel bone's local Y; wheel bones carry identity
    bind orientation and the tire geometry is skinned to the WheelDamage_*
    bones (WheelMain/WheelHubCap ride as unweighted children). Retail has no
    chassis animsets (the Anim_LC_Vehicle_* sets are driver character rigs),
    so the animation pipeline synthesizes this clip: every real wheel bone
    rotates one full turn around its local axle per clip loop.

    Only the wheel bone families (WheelDamage/WheelMain/WheelHubCap) spin.
    The naive "Wheel in name" match also caught Bone:SteeringWheel, whose
    bind orientation is a non-identity 120deg frame, so rotating it about its
    local Y knocked the steering column sideways instead of turning it.

    Returns an exporter clip dict: {"name", "rate", "num_frames", "tracks"}
    where tracks aligns per-skeleton-bone (None = hold bind pose).
    """
    spin_names = {
        bone.name for bone in skeleton
        if any(family in bone.name for family in ("WheelDamage", "WheelMain", "WheelHubCap"))
    }
    tracks = []
    for bone in skeleton:
        if bone.name not in spin_names:
            tracks.append(None)
            continue
        keys = []
        for frame in range(_WHEEL_SPIN_FRAMES):
            angle = 2.0 * math.pi * frame / _WHEEL_SPIN_FRAMES
            keys.append(PsaKey(
                position=bone.position,
                # Rotation around the bone's local +Y (the axle). The exporter
                # converts quats (x,y,z,w) -> (x,z,-y,w), so a UE +Y spin
                # (0,+s,0,c) lands on glTF -Z (0,0,-s,c); that negative-Z roll
                # moves the tire tops forward. The old -sin key produced the
                # glTF +Z spin, which ran the wheels backward.
                quat=(0.0, math.sin(angle / 2.0), 0.0, math.cos(angle / 2.0)),
            ))
        tracks.append(keys)
    return {
        "name": "Wheel Spin",
        "rate": _WHEEL_SPIN_RATE,
        "num_frames": _WHEEL_SPIN_FRAMES,
        "tracks": tracks,
    }


# Special/marketing vehicles whose IDs don't match any part-family base but
# whose wheel geometry comes from a shared retail family. Evidence is the
# full-skeleton signature match against wheel part packages: the trucks share
# the TruckCurtain rig (52/55 bones), the Perf specials match V_C_Perf /
# V_E_Perf wheel packages exactly (56/56), and the 117 Vaquero matches the
# v_e_compact wheel packages (56/56).
_VEHICLE_WHEEL_BASE = {
    "Baked_A_TruckCement": "Baked_A_TruckCurtain",
    "Baked_A_TruckChristmas": "Baked_A_TruckCurtain",
    "Baked_A_TruckGarbage": "Baked_A_TruckCurtain",
    "Crim_Performance_Aletta": "Baked_C_Perf",
    "Enf_Performance_Gumball": "Baked_E_Perf",
    "Marketing_117_Crim_Vaquero": "Baked_E_Compact",
    "Marketing_117_Enf_V20": "Baked_E_Perf",
    "Marketing_DressToKill_Jericho_Phantom_Crim": "Baked_E_Perf",
    "Marketing_DressToKill_Jericho_Phantom_Enf": "Baked_E_Perf",
}


def vehicle_wheel_base(vehicle_id: str, part_catalog: list[dict]) -> str | None:
    """Part-family base whose wheels fit a vehicle, or None if direct match.

    The frontend Wheel selector keys by ``part.base``; special vehicles whose
    IDs are not a part-family base fall back to the base they share a rig
    with. ``None`` means the vehicle ID itself is a base (or has no wheels).

    The resolved base is matched against the catalog case-insensitively and
    returned in the catalog's own casing, so the frontend key always exists.
    """
    canonical_by_casefold = {
        part["base"].casefold(): part["base"] for part in part_catalog
    }
    if vehicle_id.casefold() in canonical_by_casefold:
        return None
    override = _VEHICLE_WHEEL_BASE.get(vehicle_id)
    if override is None:
        return None
    return canonical_by_casefold.get(override.casefold(), override)
