import unreal

MASTER_PATH = "/Game/Imported/Materials/M_APBMaster"
MASTER_DIR = "/Game/Imported/Materials"

TEX_PARAMS = ("T_Diffuse", "T_Normal", "T_SpecMask", "T_Emissive")
SCALAR_PARAMS = (("RoughnessScale", 0.6), ("SpecScale", 1.0), ("EmissiveIntensity", 1.0))

DEFAULT_COLOR_TEX = "/Engine/EngineResources/DefaultTexture"
# Measured, not assumed (work/evidence/normal_tex_probe.json): /Engine/EngineMaterials/DefaultNormal
# is NOT a loadable asset in UE5.8 despite DefaultNormal.uasset existing on disk. BaseFlattenNormalMap
# is TC_NORMALMAP + srgb=False, which is what SAMPLERTYPE_NORMAL requires.
DEFAULT_NORMAL_TEX = "/Engine/EngineMaterials/BaseFlattenNormalMap"

# Without these usage flags UE5 silently swaps each MIC for the grey default material
# at draw time on Nanite/ISM/skeletal meshes. Flags on the base Material inherit to all MICs.
USAGE_FLAGS = (
    "used_with_nanite",
    "used_with_static_lighting",
    "used_with_instanced_static_meshes",
    "used_with_skeletal_mesh",
)

MEL = unreal.MaterialEditingLibrary
MP = unreal.MaterialProperty
ST = unreal.MaterialSamplerType


def _add_tex_param(mat, name, y, sampler, default_texture):
    node = MEL.create_material_expression(mat, unreal.MaterialExpressionTextureSampleParameter2D, -600, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("sampler_type", sampler)
    # A sampler whose default texture is left unset binds /Engine/EngineResources/DefaultTexture
    # (sRGB colour). For SAMPLERTYPE_NORMAL that fails PCD3D_SM6 translation with
    # "Sampler type is Normal, should be Color", and the whole master falls back to Default
    # Material in game, taking all 2341 MI_* children with it. Measured in
    # work/evidence/freeroam_probe_editor.log; invisible to -run=pythonscript, which never
    # translates for PCD3D_SM6.
    tex = unreal.EditorAssetLibrary.load_asset(default_texture)
    if not tex:
        raise RuntimeError("default texture missing for %s: %s" % (name, default_texture))
    node.set_editor_property("texture", tex)
    return node


def _add_scalar(mat, name, value, y):
    s = MEL.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -600, y)
    s.set_editor_property("parameter_name", name)
    s.set_editor_property("default_value", value)
    return s


def _apply_usage_flags(mat):
    changed = False
    for flag in USAGE_FLAGS:
        if not mat.get_editor_property(flag):
            mat.set_editor_property(flag, True)
            changed = True
    if changed:
        MEL.recompile_material(mat)
        unreal.EditorAssetLibrary.save_asset(MASTER_PATH)
    return changed


def _clear_graph(mat):
    # MaterialInstanceConstant.Parent is a hard import, so deleting M_APBMaster would null the
    # parent on all 2341 MI_* children. Rebuild in place instead.
    for attr in ("delete_all_material_expressions", "delete_all_expressions"):
        fn = getattr(MEL, attr, None)
        if fn:
            fn(mat)
            return attr
    raise RuntimeError("no MEL graph-clear API found; dir=%s"
                       % [d for d in dir(MEL) if "delete" in d.lower()])


def ensure_master_material(rebuild=False):
    existing = unreal.EditorAssetLibrary.load_asset(MASTER_PATH)
    if existing and not rebuild:
        _apply_usage_flags(existing)
        return existing

    if existing:
        mat = existing
        _clear_graph(mat)
    else:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        mat = tools.create_asset("M_APBMaster", MASTER_DIR, unreal.Material, unreal.MaterialFactoryNew())

    diffuse = _add_tex_param(mat, "T_Diffuse", -300, ST.SAMPLERTYPE_COLOR, DEFAULT_COLOR_TEX)
    normal = _add_tex_param(mat, "T_Normal", 100, ST.SAMPLERTYPE_NORMAL, DEFAULT_NORMAL_TEX)
    specmask = _add_tex_param(mat, "T_SpecMask", 500, ST.SAMPLERTYPE_LINEAR_COLOR, DEFAULT_COLOR_TEX)
    emissive = _add_tex_param(mat, "T_Emissive", 900, ST.SAMPLERTYPE_COLOR, DEFAULT_COLOR_TEX)

    rough_scale = _add_scalar(mat, "RoughnessScale", 0.6, -500)
    spec_scale = _add_scalar(mat, "SpecScale", 1.0, -400)
    emis_intensity = _add_scalar(mat, "EmissiveIntensity", 1.0, 1300)

    MEL.connect_material_property(diffuse, "RGB", MP.MP_BASE_COLOR)
    MEL.connect_material_property(normal, "RGB", MP.MP_NORMAL)

    # APB (UE3 Blinn-Phong) has no metallic workflow; all district surfaces are dielectric.
    metallic = MEL.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, -200)
    metallic.set_editor_property("r", 0.0)
    MEL.connect_material_property(metallic, "", MP.MP_METALLIC)

    # No per-pixel roughness in APB source data (SpecPower scalar is dropped upstream), so
    # roughness is a MIC-tunable scalar rather than a fabricated texture.
    MEL.connect_material_property(rough_scale, "", MP.MP_ROUGHNESS)

    # Dominant real specular signal (38% of diffuse are _DiffSpec): spec mask packed in the
    # Diffuse ALPHA channel. Scaled by SpecScale for per-material tuning.
    spec_mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 200)
    MEL.connect_material_expressions(diffuse, "A", spec_mul, "A")
    MEL.connect_material_expressions(spec_scale, "", spec_mul, "B")
    MEL.connect_material_property(spec_mul, "", MP.MP_SPECULAR)

    emis_mul = MEL.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 900)
    MEL.connect_material_expressions(emissive, "RGB", emis_mul, "A")
    MEL.connect_material_expressions(emis_intensity, "", emis_mul, "B")
    MEL.connect_material_property(emis_mul, "", MP.MP_EMISSIVE_COLOR)

    for flag in USAGE_FLAGS:
        mat.set_editor_property(flag, True)

    MEL.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(MASTER_PATH)
    return mat
