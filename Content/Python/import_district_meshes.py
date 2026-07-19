import unreal, os
ROOT = r"D:\APBReloaded\Content\Imported\Districts\Financial"
tasks = []
for f in os.listdir(ROOT):
    if f.lower().endswith(".obj"):
        t = unreal.AssetImportTask()
        t.filename = os.path.join(ROOT, f)
        t.destination_path = "/Game/Imported/Districts/Financial"
        t.automated = True
        t.save = True
        t.replace_existing = True
        tasks.append(t)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.log("Imported district meshes: %d" % len(tasks))
else:
    unreal.log_warning("No district objs")
