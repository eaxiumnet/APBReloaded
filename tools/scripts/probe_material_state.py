import json
import unreal

OUT = r"D:\APBReloaded\work\evidence\material_state_probe.json"
MASTER = "/Game/Imported/Materials/M_APBMaster"
MESH_DIR = "/Game/Imported/Districts"
MIC_DIR = "/Game/Imported/MaterialDatabase"
SAMPLE = 25

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

result = {"master": {}, "meshes": [], "mics": [], "slot_histogram": {}}

mat = EAL.load_asset(MASTER)
result["master"]["loaded"] = bool(mat)
if mat:
    result["master"]["texture_params"] = [str(n) for n in MEL.get_texture_parameter_names(mat)]
    result["master"]["scalar_params"] = [str(n) for n in MEL.get_scalar_parameter_names(mat)]
    for flag in ("used_with_nanite", "used_with_static_lighting",
                 "used_with_instanced_static_meshes", "used_with_skeletal_mesh"):
        try:
            result["master"][flag] = bool(mat.get_editor_property(flag))
        except Exception as exc:
            result["master"][flag] = "RAISED:%s" % exc


def _slots(mesh):
    out = []
    try:
        statics = mesh.get_editor_property("static_materials")
    except Exception as exc:
        return [{"error": "RAISED:%s" % exc}]
    for idx, sm in enumerate(statics):
        iface = sm.get_editor_property("material_interface")
        out.append({
            "slot": idx,
            "slot_name": str(sm.get_editor_property("material_slot_name")),
            "material": iface.get_path_name() if iface else None,
        })
    return out


mesh_paths = [p for p in EAL.list_assets(MESH_DIR, recursive=True) if "Financial" in p or "/FD_" in p]
result["mesh_total_listed"] = len(mesh_paths)
for path in mesh_paths[:SAMPLE]:
    mesh = EAL.load_asset(path)
    if not isinstance(mesh, unreal.StaticMesh):
        continue
    slots = _slots(mesh)
    result["meshes"].append({"path": path, "slots": slots})
    for s in slots:
        m = s.get("material")
        if m is None:
            key = "NONE"
        elif "WorldGridMaterial" in m:
            key = "WorldGridMaterial"
        elif "LevelColorationUnlitMaterial" in m:
            key = "LevelColorationUnlitMaterial"
        elif "/MI_" in m or "MaterialDatabase" in m:
            key = "MI_MaterialDatabase"
        else:
            key = "OTHER"
        result["slot_histogram"][key] = result["slot_histogram"].get(key, 0) + 1

mic_paths = EAL.list_assets(MIC_DIR, recursive=True)
result["mic_total_listed"] = len(mic_paths)
for path in mic_paths[:5]:
    mic = EAL.load_asset(path)
    if not isinstance(mic, unreal.MaterialInstanceConstant):
        continue
    parent = mic.get_editor_property("parent")
    entry = {"path": path, "parent": parent.get_path_name() if parent else None, "textures": {}}
    for name in ("T_Diffuse", "T_Normal", "T_SpecMask", "T_Emissive"):
        try:
            tex = MEL.get_material_instance_texture_parameter_value(mic, name)
            entry["textures"][name] = tex.get_path_name() if tex else None
        except Exception as exc:
            entry["textures"][name] = "RAISED:%s" % exc
    result["mics"].append(entry)

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("MATERIAL_STATE_PROBE histogram=%s master=%s"
           % (json.dumps(result["slot_histogram"]), json.dumps(result["master"])))
