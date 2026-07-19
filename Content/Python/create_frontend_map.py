"""Create Lvl_APB_Frontend empty level with Frontend GameMode."""
import unreal

path = "/Game/Maps/Lvl_APB_Frontend"
# New blank level and save
unreal.EditorLevelLibrary.new_level(path)
# Place a player start so PC spawns
ps_class = unreal.EditorAssetLibrary.load_blueprint_class("/Script/Engine.PlayerStart")
# PlayerStart is a native class
actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 0))
if actor:
    unreal.log("APB spawned PlayerStart")
# Set world game mode override via default map settings is in DefaultEngine.ini
unreal.EditorLevelLibrary.save_current_level()
unreal.log("APB saved frontend map " + path)
