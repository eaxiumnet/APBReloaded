import unreal, os
base = r"D:\APBReloaded\Content\Imported\Characters\Wardrobe"
tasks=[]
for f in sorted(os.listdir(base)):
  if not f.lower().endswith('.obj'): continue
  stem=os.path.splitext(f)[0]
  if os.path.isfile(os.path.join(base, stem+'.uasset')): continue
  t=unreal.AssetImportTask()
  t.filename=os.path.join(base,f)
  t.destination_path="/Game/Imported/Characters/Wardrobe"
  t.automated=True; t.save=True; t.replace_existing=False
  tasks.append(t)
unreal.log("wardrobe2 tasks=%d"%len(tasks))
if tasks:
  unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
  unreal.log("wardrobe2 imported %d"%len(tasks))
