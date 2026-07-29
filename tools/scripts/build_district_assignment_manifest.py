# TRACK C PAYOFF (EDITOR-FREE): join verified bindings -> district material-assignment manifest.
# For each imported district mesh, emit per-slot the target MIC that import_materials.py creates.
# Run: pwsh> python D:\APBReloaded\tools\scripts\build_district_assignment_manifest.py
# Output consumed by assign_district_materials.py (gated).
from __future__ import annotations
import json
from pathlib import Path

MATDB = Path(r"D:\APBReloaded\Content\Extracted\MaterialDatabase")
BINDINGS = Path(r"D:\APBReloaded\work\mesh_material_bindings.json")
MANIFEST = Path(r"D:\APBReloaded\work\district_assignment_manifest.json")


def mat_stems_by_package() -> dict[str, set[str]]:
    result: dict[str, set[str]] = {}
    for d in MATDB.iterdir():
        if d.is_dir():
            result[d.name.lower()] = {p.stem for p in d.rglob("*.mat")}
    return result


def real_dir_name() -> dict[str, str]:
    return {d.name.lower(): d.name for d in MATDB.iterdir() if d.is_dir()}


def resolve_slot(pkg_lower: str, mat_name: str,
                 stems: dict[str, set[str]], real_dirs: dict[str, str]) -> tuple[str, str]:
    stem = mat_name.split(".")[-1]
    pkg_stems = stems.get(pkg_lower, set())
    if stem in pkg_stems:
        real_pkg = real_dirs.get(pkg_lower, pkg_lower)
        ue_path = f"/Game/Imported/MaterialDatabase/{real_pkg}/MI_{stem}"
        return ue_path, "texture_mic"
    if not pkg_stems:
        return f"/Game/Imported/MaterialDatabase/{real_dirs.get(pkg_lower, pkg_lower)}/MI_{stem}", "color_mic"
    return "", "unresolved"


def run() -> int:
    MANIFEST.parent.mkdir(parents=True, exist_ok=True)
    b = json.loads(BINDINGS.read_text(encoding="utf-8"))
    stems = mat_stems_by_package()
    real_dirs = real_dir_name()
    entries: list[dict] = []
    n_tex = n_color = n_unres = 0
    for m in b["meshes"]:
        slot_assignments = []
        for s in m.get("slots", []):
            pkg = (s.get("material_package") or "").lower()
            mat = s.get("material") or ""
            ue_path, kind = resolve_slot(pkg, mat, stems, real_dirs)
            if kind == "texture_mic":
                n_tex += 1
            elif kind == "color_mic":
                n_color += 1
            else:
                n_unres += 1
            slot_assignments.append({"index": s.get("index", 0), "material": mat,
                                     "package": s.get("material_package", ""),
                                     "mic_path": ue_path, "kind": kind})
        entries.append({
            "mesh": m["mesh"],
            "district": m.get("district", ""),
            "ue_asset": f"/Game/Imported/Districts/{m.get('district', '')}/{m['mesh']}",
            "slots": slot_assignments,
        })
    MANIFEST.write_text(json.dumps(
        {"meshes": len(entries), "texture_mic": n_tex,
         "color_mic": n_color, "unresolved": n_unres, "entries": entries}, indent=1),
        encoding="utf-8")
    print(f"DISTRICT ASSIGNMENT meshes={len(entries)} texture_mic={n_tex} "
          f"color_mic={n_color} unresolved={n_unres}")
    print(f"-> {MANIFEST}")
    return 0


if __name__ == "__main__":
    raise SystemExit(run())
