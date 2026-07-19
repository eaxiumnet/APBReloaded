"""Import only district OBJs that do not yet have a matching .uasset (faster restarts)."""
import unreal
import os

base = r"D:\APBReloaded\Content\Imported\Districts"
tasks = []
skipped = 0
for district in os.listdir(base):
    dpath = os.path.join(base, district)
    if not os.path.isdir(dpath):
        continue
    for f in os.listdir(dpath):
        if not f.lower().endswith(".obj"):
            continue
        stem = os.path.splitext(f)[0]
        uasset = os.path.join(dpath, stem + ".uasset")
        if os.path.isfile(uasset):
            skipped += 1
            continue
        t = unreal.AssetImportTask()
        t.filename = os.path.join(dpath, f)
        t.destination_path = "/Game/Imported/Districts/" + district
        t.automated = True
        t.save = True
        t.replace_existing = False
        tasks.append(t)

unreal.log("import_missing: tasks=%d skipped_existing=%d" % (len(tasks), skipped))
if tasks:
    # batch to avoid one giant hang
    batch = 200
    for i in range(0, len(tasks), batch):
        chunk = tasks[i : i + batch]
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(chunk)
        unreal.log("import_missing: batch %d-%d done" % (i, i + len(chunk)))
unreal.log("import_missing: complete total_imported_attempt=%d" % len(tasks))
