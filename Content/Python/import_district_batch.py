"""Import a limited batch of missing district OBJs (default 80) then exit.

Environment / argv:
  APB_IMPORT_LIMIT=N   max tasks this run (default 80)
  APB_IMPORT_DISTRICT=Financial|Waterfront|...  optional filter
"""
import unreal
import os
import sys

base = r"D:\APBReloaded\Content\Imported\Districts"
limit = int(os.environ.get("APB_IMPORT_LIMIT", "80"))
only = os.environ.get("APB_IMPORT_DISTRICT", "").strip()

tasks = []
skipped = 0
for district in sorted(os.listdir(base)):
    if only and district.lower() != only.lower():
        continue
    dpath = os.path.join(base, district)
    if not os.path.isdir(dpath):
        continue
    for f in sorted(os.listdir(dpath)):
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
        if len(tasks) >= limit:
            break
    if len(tasks) >= limit:
        break

unreal.log("import_batch: limit=%d only=%s tasks=%d skipped_existing=%d" % (limit, only or "*", len(tasks), skipped))
if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.log("import_batch: Imported %d district objs" % len(tasks))
else:
    unreal.log("import_batch: nothing to import")
