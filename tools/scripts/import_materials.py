# A2b (GATED - runs inside UnrealEditor 5.8):
#   UnrealEditor.exe APBReloaded.uproject -run=pythonscript ^
#     -script="D:\APBReloaded\tools\scripts\import_materials.py" -nullrhi -unattended -nop4
# Reads work/material_import_manifest.json (built editor-free by build_material_import_manifest.py).
import json
import sys
import unreal

sys.path.insert(0, r"D:\APBReloaded\tools\scripts")
from apb_master_material import ensure_master_material, MASTER_PATH  # noqa: E402

MANIFEST = r"D:\APBReloaded\work\material_import_manifest.json"
REPORT = r"D:\APBReloaded\tools\material_import_report.json"

ROLE_PARAM = {"Diffuse": "T_Diffuse", "Normal": "T_Normal",
              "Specular": "T_SpecMask", "Emissive": "T_Emissive"}
LINEAR_ROLES = ("Normal", "Specular")
NORMALMAP_ROLES = ("Normal",)

_tex_cache = {}


def import_tga(src, dest_path, is_linear, is_normalmap=False):
    key = (src, dest_path)
    if key in _tex_cache:
        return _tex_cache[key]
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest_path
    task.automated = True
    task.replace_existing = True
    task.save = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    assets = task.get_objects()
    if not assets:
        _tex_cache[key] = None
        return None
    tex = assets[0]
    tex.set_editor_property("srgb", not is_linear)
    # TC_NORMALMAP is 2-channel BC5 (discards B/A) — only the true normal map may use it.
    if is_normalmap and hasattr(unreal.TextureCompressionSettings, "TC_NORMALMAP"):
        tex.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_NORMALMAP)
    elif is_linear and hasattr(unreal.TextureCompressionSettings, "TC_MASKS"):
        tex.set_editor_property("compression_settings",
                                unreal.TextureCompressionSettings.TC_MASKS)
    unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
    _tex_cache[key] = tex.get_path_name()
    return tex.get_path_name()


def make_mic(material_name, dest_path, tex_paths):
    mic_path = f"{dest_path}/MI_{material_name}"
    mic = unreal.EditorAssetLibrary.load_asset(mic_path)
    if not mic:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mic = tools.create_asset(f"MI_{material_name}", dest_path,
                                 unreal.MaterialInstanceConstant, factory)
    if not mic:
        return None
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
            report["external"].append(entry["material"])
            continue
        tex_ue = {}
        for role, src in entry["textures"].items():
            tex_ue[role] = import_tga(src, dest, role in LINEAR_ROLES,
                                      is_normalmap=role in NORMALMAP_ROLES)
        mic_path = make_mic(entry["material"], dest, tex_ue)
        if mic_path:
            report["ok"].append({"material": entry["material"], "mic": mic_path})
        else:
            report["fail"].append(entry["material"])
        if (i + 1) % 100 == 0:
            unreal.SystemLibrary.collect_garbage()
            unreal.log(f"A2b progress {i + 1}/{len(manifest['entries'])}")
    with open(REPORT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)
    unreal.log(f"A2b DONE ok={len(report['ok'])} fail={len(report['fail'])} "
               f"external={len(report['external'])} -> {REPORT}")


run()
