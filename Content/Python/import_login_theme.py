import unreal, os
src = r"D:\APBReloaded\Content\Audio\LoginTheme_ChapterOne_APB.mp3"
if not os.path.isfile(src):
    unreal.log_error("missing " + src)
else:
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = "/Game/Audio"
    task.destination_name = "LoginTheme_ChapterOne_APB"
    task.automated = True
    task.save = True
    task.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log("imported LoginTheme_ChapterOne_APB")
# import largest background tga as UI backdrop optional
bg = r"D:\APBReloaded\Content\Extracted\UI\ui_frontend_background\Texture2D\scene_background_I1.tga"
if os.path.isfile(bg):
    t = unreal.AssetImportTask()
    t.filename = bg
    t.destination_path = "/Game/UI/Frontend"
    t.destination_name = "LoginBackground"
    t.automated = True
    t.save = True
    t.replace_existing = True
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
    unreal.log("imported LoginBackground")
