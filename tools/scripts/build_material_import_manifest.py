# A2a (EDITOR-FREE): parse MaterialDatabase .mat roles -> material_import_manifest.json
# Run: pwsh> python D:\APBReloaded\tools\scripts\build_material_import_manifest.py
# Output consumed by import_materials.py (gated).
from __future__ import annotations
import json, re
from pathlib import Path

MATDB = Path(r"D:\APBReloaded\Content\Extracted\MaterialDatabase")
MANIFEST = Path(r"D:\APBReloaded\work\material_import_manifest.json")
LOG = Path(r"D:\APBReloaded\work\logs\material_manifest.log")

ROLE_KEYS = ("Diffuse", "Normal", "Emissive", "Opacity", "Specular", "Cube")


def parse_mat(mat_path: Path) -> dict:
    roles: dict[str, str] = {}
    for line in mat_path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = re.match(r"\s*([A-Za-z]+)\s*=\s*(\S+)", line)
        if m and m.group(1) in ROLE_KEYS:
            roles[m.group(1)] = m.group(2)
    return roles


def resolve_texture(pkg_dir: Path, texname: str, global_index: dict[str, str]) -> str:
    hit = pkg_dir / f"{texname}.tga"
    if hit.is_file():
        return str(hit)
    cand = list(pkg_dir.rglob(f"{texname}.tga"))
    if cand:
        return str(cand[0])
    return global_index.get(texname.lower(), "")


def build_global_index() -> dict[str, str]:
    index: dict[str, str] = {}
    for tga in MATDB.rglob("*.tga"):
        index.setdefault(tga.stem.lower(), str(tga))
    return index


def run() -> int:
    LOG.parent.mkdir(parents=True, exist_ok=True)
    global_index = build_global_index()
    pkg_dirs = sorted(d for d in MATDB.iterdir() if d.is_dir())
    entries: list[dict] = []
    n_ext = 0
    for pkg in pkg_dirs:
        district = pkg.name
        for mat in pkg.rglob("*.mat"):
            roles = parse_mat(mat)
            if not roles:
                continue
            textures = {}
            for role, texname in roles.items():
                if role in ("SpecPower",):
                    continue
                path = resolve_texture(pkg, texname, global_index)
                if path:
                    textures[role] = path
            external = bool(roles) and not textures
            if external:
                n_ext += 1
            entries.append({
                "material": mat.stem,
                "package": district,
                "textures": textures,
                "external_textures": external,
                "dest": f"/Game/Imported/MaterialDatabase/{district}",
            })
    MANIFEST.write_text(json.dumps(
        {"packages": len(pkg_dirs), "materials": len(entries),
         "external": n_ext, "entries": entries}, indent=1), encoding="utf-8")
    LOG.write_text(f"DONE packages={len(pkg_dirs)} materials={len(entries)} external={n_ext}\n",
                   encoding="utf-8")
    print(f"MATERIAL MANIFEST packages={len(pkg_dirs)} materials={len(entries)} external={n_ext}")
    print(f"-> {MANIFEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
