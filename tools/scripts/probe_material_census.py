import json
import unreal

OUT = r"D:\APBReloaded\work\evidence\material_census.json"
MESH_DIR = "/Game/Imported/Districts"
MIC_DIR = "/Game/Imported/MaterialDatabase"
MASTER = "/Game/Imported/Materials/M_APBMaster"
MIC_SAMPLE = 250

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

result = {
    "mesh_total": 0,
    "mesh_scanned": 0,
    "slot_histogram": {},
    "meshes_with_any_worldgrid": 0,
    "meshes_all_worldgrid": 0,
    "worldgrid_examples": [],
    "mic_total": 0,
    "mic_scanned": 0,
    "mic_parent_histogram": {},
    "mic_texture_histogram": {},
    "mic_default_diffuse": [],
}


def classify(path):
    if path is None:
        return "NONE"
    if "WorldGridMaterial" in path:
        return "WorldGridMaterial"
    if "LevelColorationUnlitMaterial" in path:
        return "LevelColorationUnlitMaterial"
    if "/MI_" in path or "MaterialDatabase" in path:
        return "MI_MaterialDatabase"
    return "OTHER"


def bump(bucket, key):
    result[bucket][key] = result[bucket].get(key, 0) + 1


mesh_paths = [p for p in EAL.list_assets(MESH_DIR, recursive=True)
              if "Financial" in p or "/FD_" in p]
result["mesh_total"] = len(mesh_paths)

for path in mesh_paths:
    mesh = EAL.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    try:
        statics = mesh.get_editor_property("static_materials")
    except Exception:
        continue
    result["mesh_scanned"] += 1
    kinds = []
    for sm in statics:
        iface = sm.get_editor_property("material_interface")
        kind = classify(iface.get_path_name() if iface else None)
        kinds.append(kind)
        bump("slot_histogram", kind)
    if not kinds:
        continue
    grid = [k for k in kinds if k == "WorldGridMaterial"]
    if grid:
        result["meshes_with_any_worldgrid"] += 1
        if len(result["worldgrid_examples"]) < 25:
            result["worldgrid_examples"].append({"path": path, "slots": kinds})
    if len(grid) == len(kinds):
        result["meshes_all_worldgrid"] += 1

def _flush():
    with open(OUT, "w", encoding="utf-8") as fh:
        json.dump(result, fh, indent=2)


# The mesh census is the expensive half; persist it before touching MICs so a failure in the
# MIC pass cannot discard it.
_flush()

# unreal.Array rejects strided slicing, so materialise a native list before sampling.
mic_paths = [str(p) for p in EAL.list_assets(MIC_DIR, recursive=True)]
result["mic_total"] = len(mic_paths)
step = max(1, len(mic_paths) // MIC_SAMPLE)
for path in mic_paths[::step]:
    mic = EAL.load_asset(path)
    if not isinstance(mic, unreal.MaterialInstanceConstant):
        continue
    result["mic_scanned"] += 1
    parent = mic.get_editor_property("parent")
    bump("mic_parent_histogram", parent.get_path_name() if parent else "NONE")
    for name in ("T_Diffuse", "T_Normal"):
        try:
            tex = MEL.get_material_instance_texture_parameter_value(mic, name)
        except Exception:
            bump("mic_texture_histogram", "%s:RAISED" % name)
            continue
        tex_path = tex.get_path_name() if tex else None
        if tex_path is None:
            bump("mic_texture_histogram", "%s:NONE" % name)
        elif "/Engine/" in tex_path:
            bump("mic_texture_histogram", "%s:ENGINE_DEFAULT" % name)
            if name == "T_Diffuse" and len(result["mic_default_diffuse"]) < 25:
                result["mic_default_diffuse"].append(path)
        else:
            bump("mic_texture_histogram", "%s:REAL" % name)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)

unreal.log("MATERIAL_CENSUS meshes=%d/%d slots=%s any_grid=%d all_grid=%d mics=%d/%d parents=%s tex=%s"
           % (result["mesh_scanned"], result["mesh_total"], json.dumps(result["slot_histogram"]),
              result["meshes_with_any_worldgrid"], result["meshes_all_worldgrid"],
              result["mic_scanned"], result["mic_total"],
              json.dumps(result["mic_parent_histogram"]),
              json.dumps(result["mic_texture_histogram"])))
