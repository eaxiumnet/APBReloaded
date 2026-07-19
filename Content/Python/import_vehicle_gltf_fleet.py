"""Import umodel-exported vehicle glTF chassis into /Game/Imported/Vehicles/*."""
import unreal
import os

ROOT = r"D:\APBReloaded\Content\Extracted\UmodelExport_Vehicles"
DEST_ROOT = "/Game/Imported/Vehicles"
tasks = []

for family in sorted(os.listdir(ROOT)):
    fam_dir = os.path.join(ROOT, family)
    if not os.path.isdir(fam_dir):
        continue
    gltf = None
    for dirpath, _, files in os.walk(fam_dir):
        for f in files:
            if f.lower().endswith(".gltf") or f.lower().endswith(".glb"):
                gltf = os.path.join(dirpath, f)
                break
        if gltf:
            break
    if not gltf:
        unreal.log_warning("No glTF for " + family)
        continue
    dest = DEST_ROOT + "/" + family
    task = unreal.AssetImportTask()
    task.filename = gltf
    task.destination_path = dest
    task.automated = True
    task.save = True
    task.replace_existing = True
    tasks.append(task)
    unreal.log("Vehicle import: %s -> %s" % (gltf, dest))

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.log("Imported %d vehicle glTF tasks" % len(tasks))
else:
    unreal.log_warning("No vehicle glTF import tasks")
