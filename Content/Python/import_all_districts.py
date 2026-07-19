import unreal, os
base = r"D:\APBReloaded\Content\Imported\Districts"
tasks = []
for district in os.listdir(base):
    dpath = os.path.join(base, district)
    if not os.path.isdir(dpath):
        continue
    for f in os.listdir(dpath):
        if f.lower().endswith(".obj"):
            t = unreal.AssetImportTask()
            t.filename = os.path.join(dpath, f)
            t.destination_path = "/Game/Imported/Districts/" + district
            t.automated = True
            t.save = True
            t.replace_existing = True
            tasks.append(t)
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.log("Imported %d district objs" % len(tasks))
