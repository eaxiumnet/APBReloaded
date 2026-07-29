import json
import unreal

OUT = r"D:\APBReloaded\work\evidence\normal_tex_probe.json"
MASTER = "/Game/Imported/Materials/M_APBMaster"

EAL = unreal.EditorAssetLibrary
MEL = unreal.MaterialEditingLibrary

CANDIDATES = (
    "/Engine/EngineMaterials/DefaultNormal",
    "/Engine/EngineMaterials/DefaultNormal.DefaultNormal",
    "/Engine/EngineMaterials/BaseFlattenNormalMap",
    "/Engine/EngineMaterials/FlatNormal",
    "/Engine/EngineResources/DefaultTexture",
)

result = {"master_intact": {}, "candidates": {}, "normalmap_textures": []}

mat = EAL.load_asset(MASTER)
result["master_intact"]["loaded"] = bool(mat)
if mat:
    names = [str(n) for n in MEL.get_texture_parameter_names(mat)]
    result["master_intact"]["texture_params"] = names
    result["master_intact"]["param_count"] = len(names)

for path in CANDIDATES:
    entry = {}
    try:
        entry["does_asset_exist"] = bool(EAL.does_asset_exist(path))
    except Exception as exc:
        entry["does_asset_exist"] = "RAISED:%s" % exc
    try:
        asset = EAL.load_asset(path)
    except Exception as exc:
        entry["load"] = "RAISED:%s" % exc
        asset = None
    else:
        entry["load"] = bool(asset)
    if asset:
        entry["class"] = type(asset).__name__
        for prop in ("compression_settings", "srgb"):
            try:
                entry[prop] = str(asset.get_editor_property(prop))
            except Exception as exc:
                entry[prop] = "RAISED:%s" % exc
    result["candidates"][path] = entry

for directory in ("/Engine/EngineMaterials", "/Engine/EngineResources"):
    try:
        listed = [str(p) for p in EAL.list_assets(directory, recursive=True)]
    except Exception as exc:
        result["normalmap_textures"].append({"dir": directory, "error": "RAISED:%s" % exc})
        continue
    for path in listed:
        asset = EAL.load_asset(path)
        if not isinstance(asset, unreal.Texture2D):
            continue
        try:
            comp = str(asset.get_editor_property("compression_settings"))
        except Exception:
            continue
        if "NORMALMAP" in comp.upper():
            result["normalmap_textures"].append({"path": path, "compression": comp})

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)

unreal.log("NORMAL_TEX_PROBE master=%s candidates=%s normalmaps=%s"
           % (json.dumps(result["master_intact"]),
              json.dumps(result["candidates"]),
              json.dumps(result["normalmap_textures"][:10])))
