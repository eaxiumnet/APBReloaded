import unreal, os
base = r"D:\APBReloaded\Content\Imported\Districts\Financial"
tasks=[]
for f in sorted(os.listdir(base)):
  if not f.startswith("ROAD_") or not f.lower().endswith(".obj"): continue
  stem=os.path.splitext(f)[0]
  if os.path.isfile(os.path.join(base, stem+".uasset")): continue
  t=unreal.AssetImportTask()
  t.filename=os.path.join(base,f)
  t.destination_path="/Game/Imported/Districts/Financial"
  t.automated=True; t.save=True; t.replace_existing=False
  tasks.append(t)
# batch 100
tasks=tasks[:120]
unreal.log("road_import tasks=%d"%len(tasks))
if tasks:
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
  unreal.log("road_import done %d"%len(tasks))
