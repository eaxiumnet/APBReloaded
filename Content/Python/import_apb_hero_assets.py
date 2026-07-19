"""Import hero OBJ/TGA into /Game/Imported as uassets (run via UnrealEditor-Cmd -ExecutePythonScript)."""
import unreal
import os

ROOT = r"D:\APBReloaded\Content\Imported"
tasks = []

def make_task(filename, dest_path):
    task = unreal.AssetImportTask()
    task.filename = filename
    task.destination_path = dest_path
    task.automated = True
    task.save = True
    task.replace_existing = True
    return task

pairs = [
    (os.path.join(ROOT, r"Characters\Contact_LaRocha\m_contact_enforcement_larocha.obj"),
     "/Game/Imported/Characters/Contact_LaRocha"),
    (os.path.join(ROOT, r"Characters\Contact_Sofia\F_Contact_Enforcement_Sofia.obj"),
     "/Game/Imported/Characters/Contact_Sofia"),
    (os.path.join(ROOT, r"Characters\Contact_Bloodrose\F_Contact_Criminal_Bloodrose.obj"),
     "/Game/Imported/Characters/Contact_Bloodrose"),
    (os.path.join(ROOT, r"Vehicles\V_A_2DrCoupe\PartMesh.obj"),
     "/Game/Imported/Vehicles/V_A_2DrCoupe"),
]

for src, dest in pairs:
    if os.path.isfile(src):
        tasks.append(make_task(src, dest))
        unreal.log("APB import queue: " + src)
    else:
        unreal.log_warning("Missing " + src)

# TGA textures
for dirpath, _, files in os.walk(ROOT):
    for f in files:
        if f.lower().endswith(".tga"):
            full = os.path.join(dirpath, f)
            rel = os.path.relpath(dirpath, ROOT).replace("\\", "/")
            dest = "/Game/Imported/" + rel
            tasks.append(make_task(full, dest))

if tasks:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    unreal.log("APB imported %d tasks" % len(tasks))
else:
    unreal.log_warning("No import tasks")
