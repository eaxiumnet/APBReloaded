import json
import unreal

MASTER = "/Game/Imported/Materials/M_APBMaster"
OUT = r"D:\APBReloaded\tools\master_material_verify.json"

MEL = unreal.MaterialEditingLibrary
mat = unreal.EditorAssetLibrary.load_asset(MASTER)
result = {
    "loaded": bool(mat),
    "params": {},
    "sampler_mismatch": [],
    "probe": "none",
    "diag": {},
    "recompiled": False,
    "ok": False,
}


def _expressions(m):
    # UE relocated UMaterial::Expressions behind an expression collection during 5.x and the
    # Python surface differs by release. Probe every known accessor and record which one
    # resolved, so a failed lookup reports as an instrument failure rather than as a material
    # with no expressions.
    diag = {}
    for attr in ("expression_collection", "expressions"):
        try:
            value = m.get_editor_property(attr)
        except Exception as exc:
            diag[attr] = "RAISED:%s" % exc
            continue
        if value is None:
            diag[attr] = "NONE"
            continue
        holder = getattr(value, "expressions", value)
        try:
            items = list(holder)
        except TypeError as exc:
            diag[attr] = "NOT_ITERABLE:%s type=%s" % (exc, type(value).__name__)
            continue
        diag[attr] = "OK count=%d" % len(items)
        if items:
            return items, attr, diag

    try:
        diag["get_texture_parameter_names"] = [str(n) for n in MEL.get_texture_parameter_names(m)]
    except Exception as exc:
        diag["get_texture_parameter_names"] = "RAISED:%s" % exc
    diag["material_attrs"] = [d for d in dir(m) if "expr" in d.lower()]
    return [], "none", diag


if mat:
    exprs, probe, diag = _expressions(mat)
    result["probe"] = probe
    result["diag"] = diag
    for ex in exprs:
        if not isinstance(ex, unreal.MaterialExpressionTextureSampleParameter2D):
            continue
        name = str(ex.get_editor_property("parameter_name"))
        sampler = str(ex.get_editor_property("sampler_type"))
        tex = ex.get_editor_property("texture")
        path = tex.get_path_name() if tex else None
        result["params"][name] = {"sampler_type": sampler, "default_texture": path}
        # A NORMAL sampler with a colour (or absent) default resolves to
        # /Engine/EngineResources/DefaultTexture and fails compilation, which silently
        # demotes every child MI_* to the fallback material at draw time.
        if "SAMPLERTYPE_NORMAL" in sampler and (path is None or "DefaultNormal" not in path):
            result["sampler_mismatch"].append(name)

    # Translation errors are emitted only on recompile, so force one and let the caller grep
    # the editor log for the sampler-type diagnostic.
    try:
        MEL.recompile_material(mat)
        result["recompiled"] = True
    except Exception as exc:
        result["diag"]["recompile"] = "RAISED:%s" % exc

    result["ok"] = bool(result["params"]) and not result["sampler_mismatch"]

with open(OUT, "w", encoding="utf-8") as fh:
    json.dump(result, fh, indent=2)
unreal.log("MASTER_VERIFY %s" % json.dumps(result))
