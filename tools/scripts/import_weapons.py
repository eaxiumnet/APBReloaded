# B3b (GATED - runs inside UnrealEditor 5.8):
#   UnrealEditor.exe APBReloaded.uproject -run=pythonscript ^
#     -script="D:\APBReloaded\tools\scripts\import_weapons.py" -nullrhi -unattended -nop4
# Reads work/weapon_import_manifest.json (built editor-free by build_weapon_import_manifest.py).
import json
import os
import sys
import unreal

sys.path.insert(0, r"D:\APBReloaded\tools\scripts")
from apb_master_material import ensure_master_material, MASTER_PATH  # noqa: E402

MANIFEST = r"D:\APBReloaded\work\weapon_import_manifest.json"
REPORT = r"D:\APBReloaded\tools\weapon_import_report.json"

ROLE_PARAM = {"Diffuse": "T_Diffuse", "Normal": "T_Normal",
              "Specular": "T_SpecMask", "Emissive": "T_Emissive"}
LINEAR_ROLES = ("Normal", "Specular")


def import_tga(src, dest_path, is_linear):
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = task.get_objects()
    if not assets:
        return None
    tex = assets[0]
    tex.set_editor_property("srgb", not is_linear)
    if is_linear and hasattr(unreal.TextureCompressionSettings, "TC_NORMALMAP"):
        tex.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_NORMALMAP)
    unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
    return tex.get_path_name()


def import_mesh(obj_path, dest_path):
    task = unreal.AssetImportTask()
    task.filename = obj_path
    task.destination_path = dest_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = task.get_objects()
    return assets[0].get_path_name() if assets else None


def make_mic(weapon, mesh_stem, dest_path, tex_paths):
    mic_name = f"MI_{mesh_stem}"
    factory = unreal.MaterialInstanceConstantFactoryNew()
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mic = tools.create_asset(mic_name, dest_path, unreal.MaterialInstanceConstant, factory)
    master = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    unreal.MaterialEditingLibrary.set_material_instance_parent(mic, master)
    for role, ue_path in tex_paths.items():
        param = ROLE_PARAM.get(role)
        if param and ue_path:
            tex = unreal.EditorAssetLibrary.load_asset(ue_path)
            if tex:
                unreal.MaterialEditingLibrary.set_material_instance_texture_parameter_value(
                    mic, param, tex)
    unreal.EditorAssetLibrary.save_asset(mic.get_path_name())
    return mic.get_path_name()


def run():
    ensure_master_material()
    with open(MANIFEST, encoding="utf-8") as fh:
        manifest = json.load(fh)
    report = {"ok": [], "fail": [], "external": []}
    for i, entry in enumerate(manifest["entries"]):
        dest = entry["dest"]
        if entry.get("external_textures"):
            report["external"].append(entry["mesh"])
            continue
        mesh_path = import_mesh(entry["obj"], dest)
        if not mesh_path:
            report["fail"].append({"mesh": entry["mesh"], "error": "OBJ import returned nothing"})
            continue
        tex_ue = {}
        for role, src in entry["textures"].items():
            tex_ue[role] = import_tga(src, dest, role in LINEAR_ROLES)
        mic_path = make_mic(entry["weapon"], entry["mesh"], dest, tex_ue)
        report["ok"].append({"mesh": entry["mesh"], "mic": mic_path})
        if (i + 1) % 50 == 0:
            unreal.SystemLibrary.collect_garbage()
    with open(REPORT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)
    unreal.log(f"B3b DONE ok={len(report['ok'])} fail={len(report['fail'])} "
               f"external={len(report['external'])} -> {REPORT}")


run()

