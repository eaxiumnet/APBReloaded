# B3a (EDITOR-FREE): convert weapon pskx->obj + parse .mat roles -> weapon_import_manifest.json
# Run: pwsh> python D:\APBReloaded\tools\scripts\build_weapon_import_manifest.py
# No UnrealEditor needed. Pure Python. Output consumed by import_weapons.py (gated).
from __future__ import annotations
import json, re, sys
from pathlib import Path

sys.path.insert(0, str(Path(r"D:\APBReloaded\tools\model_viewer")))
from psk_reader import load_psk, export_obj  # noqa: E402

WEAPONS = Path(r"D:\APBReloaded\Content\Extracted\Weapons")
OBJ_OUT = Path(r"D:\APBReloaded\Content\Extracted\Weapons_obj")
MANIFEST = Path(r"D:\APBReloaded\work\weapon_import_manifest.json")
LOG = Path(r"D:\APBReloaded\work\logs\weapon_manifest.log")

# .mat role -> semantic texture slot (weapon vocabulary confirmed from all 90 .mat)
ROLE_KEYS = ("Diffuse", "Normal", "Specular", "SpecPower", "Cube", "Opacity", "Emissive")


def parse_mat(mat_path: Path) -> dict:
    """Parse a umodel .mat file: 'Role=TextureName' lines -> {role: texname}."""
    roles: dict[str, str] = {}
    for line in mat_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.match(r"\s*([A-Za-z]+)\s*=\s*(\S+)", line)
        if m and m.group(1) in ROLE_KEYS:
            roles[m.group(1)] = m.group(2)
    return roles


def parse_mesh_material_slot(props_path: Path) -> list[str]:
    """SkeletalMesh props: Materials[n] = MaterialInstanceConstant'Materials.<Name>' -> [names]."""
    slots: list[str] = []
    for line in props_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.search(r"Materials\[\d+\]\s*=\s*\w+'([^']+)'", line)
        if m:
            slots.append(m.group(1).split(".")[-1])
    return slots


SUFFIX_ROLE = (
    ("_DiffSpec", "Diffuse"), ("_Diff", "Diffuse"),
    ("_Norm", "Normal"), ("_Spec", "Specular"),
    ("_Emissive", "Emissive"), ("_Opac", "Opacity"),
)
MAT_TEX_ROLES = ("Diffuse", "Normal", "Specular", "Emissive", "Opacity")


def is_usable_mat(name: str) -> bool:
    upper = name.upper()
    return "MASTER" not in upper and "SUPERSHADER" not in upper


def pick_mat(slot_names: list[str], mats: dict[str, dict]) -> dict:
    for slot in slot_names:
        if slot in mats and is_usable_mat(slot):
            return mats[slot]
    base = [n for n in mats if n.upper().endswith("_MAT") and is_usable_mat(n)]
    if base:
        return mats[base[0]]
    default = [n for n in mats if "Default" in n and is_usable_mat(n)]
    if default:
        return mats[default[0]]
    return {}


def build_global_index() -> dict[str, str]:
    index: dict[str, str] = {}
    for tga in WEAPONS.rglob("*.tga"):
        index.setdefault(tga.stem.lower(), str(tga))
    return index


def textures_from_mat(weapon_dir: Path, roles: dict, global_index: dict[str, str]) -> dict[str, str]:
    found: dict[str, str] = {}
    for role in MAT_TEX_ROLES:
        tex = roles.get(role)
        if not tex:
            continue
        hit = weapon_dir / f"{tex}.tga"
        if not hit.is_file():
            cand = list(weapon_dir.rglob(f"{tex}.tga"))
            hit = cand[0] if cand else None
        if hit and hit.is_file():
            found[role] = str(hit)
        elif tex.lower() in global_index:
            found[role] = global_index[tex.lower()]
    return found


VARIANT_SUFFIX = re.compile(r"_(Base|Bipod|Compact|Thrown|Shell|ClipEmpty|Clip|Empty)$", re.IGNORECASE)


def _stem_candidates(mesh_stem: str) -> list[str]:
    base = re.sub(r"_LOD\d+$", "", mesh_stem)
    candidates = [base]
    stripped = VARIANT_SUFFIX.sub("", base)
    while stripped != candidates[-1]:
        candidates.append(stripped)
        stripped = VARIANT_SUFFIX.sub("", stripped)
    return candidates


def resolve_textures_by_stem(weapon_dir: Path, mesh_stem: str) -> dict[str, str]:
    found: dict[str, str] = {}
    for base in _stem_candidates(mesh_stem):
        for suffix, role in SUFFIX_ROLE:
            if role in found:
                continue
            hit = weapon_dir / (base + suffix + ".tga")
            if not hit.is_file():
                cand = list(weapon_dir.rglob(base + suffix + ".tga"))
                hit = cand[0] if cand else None
            if hit and hit.is_file():
                found[role] = str(hit)
        if found:
            break
    return found


def run() -> int:
    OBJ_OUT.mkdir(parents=True, exist_ok=True)
    LOG.parent.mkdir(parents=True, exist_ok=True)
    weapons = sorted(d for d in WEAPONS.iterdir() if d.is_dir())
    global_index = build_global_index()
    out: list[dict] = []
    n_obj = n_fail = 0
    log_lines: list[str] = []
    for wd in weapons:
        mats = {p.stem: parse_mat(p) for p in wd.rglob("*.mat")}
        for psk in list(wd.rglob("*.psk")) + list(wd.rglob("*.pskx")):
            stem = psk.stem
            obj_path = OBJ_OUT / wd.name / f"{stem}.obj"
            try:
                mesh = load_psk(psk)
                obj_path.parent.mkdir(parents=True, exist_ok=True)
                export_obj(mesh, obj_path)
                n_obj += 1
            except Exception as exc:  # noqa: BLE001
                n_fail += 1
                log_lines.append(f"FAIL obj {stem}: {exc}")
                continue
            props = psk.with_suffix(".props.txt")
            slot_names = parse_mesh_material_slot(props) if props.is_file() else []
            chosen = pick_mat(slot_names, mats)
            textures = textures_from_mat(wd, chosen, global_index) or resolve_textures_by_stem(wd, stem)
            spec_power = chosen.get("SpecPower", "")
            out.append({
                "weapon": wd.name,
                "mesh": stem,
                "obj": str(obj_path),
                "material_slots": slot_names,
                "textures": textures,
                "spec_power": spec_power,
                "external_textures": not textures,
                "dest": f"/Game/Imported/Weapons/{wd.name}",
            })
    MANIFEST.write_text(json.dumps(
        {"weapons": len(weapons), "meshes": len(out), "entries": out}, indent=1), encoding="utf-8")
    LOG.write_text("\n".join(log_lines) + f"\nDONE obj_ok={n_obj} obj_fail={n_fail} meshes={len(out)}\n",
                   encoding="utf-8")
    print(f"MANIFEST weapons={len(weapons)} meshes={len(out)} obj_ok={n_obj} obj_fail={n_fail}")
    print(f"-> {MANIFEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())

