"""Create content-distinct freeroam umaps by duplicating a known-good template level.

UE5.8 EditorAssetLibrary.duplicate_asset(source_path, dest_path) — 2 args only.
"""
import unreal

TEMPLATE = "/Game/ThirdPerson/Lvl_ThirdPerson"

districts = [
    ("Financial", "/Game/Maps/Lvl_APB_Financial_Freeroam"),
    ("Waterfront", "/Game/Maps/Lvl_APB_Waterfront_Freeroam"),
    ("PGAsylum", "/Game/Maps/Lvl_APB_PGAsylum_Freeroam"),
    ("PGBeacon", "/Game/Maps/Lvl_APB_PGBeacon_Freeroam"),
    ("PGCrate", "/Game/Maps/Lvl_APB_PGCrate_Freeroam"),
    ("Social", "/Game/Maps/Lvl_APB_Social_Freeroam"),
    ("FinancialChaos", "/Game/Maps/Lvl_APB_FinancialChaos_Freeroam"),
    ("FinancialRiot", "/Game/Maps/Lvl_APB_FinancialRiot_Freeroam"),
]

offsets = {
    "Financial": (0.0, 0.0, 0.0),
    "Waterfront": (50000.0, 0.0, 0.0),
    "PGAsylum": (0.0, 50000.0, 0.0),
    "PGBeacon": (50000.0, 50000.0, 0.0),
    "PGCrate": (100000.0, 0.0, 0.0),
    "Social": (0.0, 100000.0, 0.0),
    "FinancialChaos": (100000.0, 50000.0, 0.0),
    "FinancialRiot": (50000.0, 100000.0, 0.0),
}

editor_asset_lib = unreal.EditorAssetLibrary
level_lib = unreal.EditorLevelLibrary
try:
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
except Exception:
    les = None

if not editor_asset_lib.does_asset_exist(TEMPLATE):
    unreal.log_error("Template missing: " + TEMPLATE)
else:
    for dist_id, path in districts:
        if editor_asset_lib.does_asset_exist(path):
            editor_asset_lib.delete_asset(path)
            unreal.log("deleted " + path)

        # UE5: duplicate_asset(source_asset_path, destination_asset_path)
        try:
            dup = editor_asset_lib.duplicate_asset(TEMPLATE, path)
        except Exception as e:
            unreal.log_error("duplicate_asset exception %s: %s" % (path, e))
            dup = None

        if not dup and not editor_asset_lib.does_asset_exist(path):
            try:
                asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
                src = editor_asset_lib.load_asset(TEMPLATE)
                dest_path = path.rsplit("/", 1)[0]
                dest_name = path.rsplit("/", 1)[1]
                dup = asset_tools.duplicate_asset(dest_name, dest_path, src)
            except Exception as e:
                unreal.log_error("asset_tools.duplicate failed %s: %s" % (path, e))
                continue

        if not editor_asset_lib.does_asset_exist(path):
            unreal.log_error("map still missing after duplicate: " + path)
            continue
        unreal.log("duplicated template -> " + path)

        loaded = False
        if les is not None:
            try:
                les.load_level(path)
                loaded = True
            except Exception as e:
                unreal.log_warning("les.load_level: %s" % e)
        if not loaded:
            try:
                level_lib.load_level(path)
                loaded = True
            except Exception as e:
                unreal.log_warning("level_lib.load_level: %s" % e)

        ox, oy, oz = offsets.get(dist_id, (0.0, 0.0, 0.0))

        ps = level_lib.spawn_actor_from_class(
            unreal.PlayerStart.static_class(),
            unreal.Vector(ox + 2200.0, oy - 2200.0, oz + 200.0),
            unreal.Rotator(0.0, float(len(dist_id) * 10), 0.0),
        )
        if ps:
            ps.set_actor_label("APB_PlayerStart_" + dist_id)

        note = level_lib.spawn_actor_from_class(
            unreal.Note.static_class(),
            unreal.Vector(ox + 10.0 * len(dist_id), oy + 20.0, oz + 300.0 + (hash(dist_id) % 100)),
            unreal.Rotator(0.0, float(ord(dist_id[0])), 0.0),
        )
        if note:
            note.set_actor_label("APB_DistrictMarker_" + dist_id)
            try:
                note.set_editor_property("text", "APB " + dist_id)
            except Exception:
                pass

        light = level_lib.spawn_actor_from_class(
            unreal.PointLight.static_class(),
            unreal.Vector(ox + 500.0, oy - 300.0, oz + 450.0 + len(dist_id) * 7.0),
            unreal.Rotator(0.0, 0.0, 0.0),
        )
        if light:
            light.set_actor_label("APB_Light_" + dist_id)

        if les is not None:
            try:
                les.save_current_level()
            except Exception:
                level_lib.save_current_level()
        else:
            level_lib.save_current_level()
        editor_asset_lib.save_asset(path, only_if_is_dirty=False)
        unreal.log("Saved distinct map " + path + " for " + dist_id)

unreal.log("Distinct district maps done (template-dup)")
