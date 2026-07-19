"""Set Lvl_APB_Frontend world GameMode override to APBFrontendGameMode."""
import unreal

path = "/Game/Maps/Lvl_APB_Frontend"
if not unreal.EditorAssetLibrary.does_asset_exist(path):
    unreal.EditorLevelLibrary.new_level(path)
else:
    unreal.EditorLevelLibrary.load_level(path)

# Spawn player start if missing
starts = unreal.GameplayStatics.get_all_actors_of_class(unreal.EditorLevelLibrary.get_editor_world(), unreal.PlayerStart)
if len(starts) == 0:
    unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator())

world_settings = unreal.EditorLevelLibrary.get_game_mode()
# Set via World Settings actor
ws_actors = unreal.GameplayStatics.get_all_actors_of_class(unreal.EditorLevelLibrary.get_editor_world(), unreal.WorldSettings)
gm_cls = unreal.load_class(None, "/Script/APBReloaded.APBFrontendGameMode")
if gm_cls and len(ws_actors) > 0:
    ws = ws_actors[0]
    # DefaultGameMode soft class path
    try:
        ws.set_editor_property("default_game_mode", gm_cls)
        unreal.log("APB set WorldSettings default_game_mode to Frontend")
    except Exception as e:
        unreal.log_warning("set default_game_mode failed: " + str(e))

unreal.EditorLevelLibrary.save_current_level()
unreal.log("APB frontend map saved")
