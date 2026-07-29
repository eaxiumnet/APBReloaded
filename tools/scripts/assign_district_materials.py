# assign_district_materials.py (GATED - runs inside UnrealEditor 5.8):
#   UnrealEditor.exe APBReloaded.uproject -run=pythonscript ^
#     -script="D:\APBReloaded\tools\scripts\assign_district_materials.py" -nullrhi -unattended -nop4
# Reads work/district_assignment_manifest.json built by build_district_assignment_manifest.py.
# Assigns MICs to each imported district StaticMesh asset per its material slots.
import json
import unreal

MANIFEST = r"D:\APBReloaded\work\district_assignment_manifest.json"
REPORT = r"D:\APBReloaded\tools\district_assignment_report.json"


def assign_mesh(entry: dict, report: dict) -> None:
    asset_path = entry["ue_asset"]
    mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not mesh:
        report["fail"].append({"mesh": entry["mesh"], "error": "asset not found"})
        return
    changed = False
    for slot in entry["slots"]:
        mic_path = slot.get("mic_path")
        if not mic_path:
            continue
        mic = unreal.EditorAssetLibrary.load_asset(mic_path)
        if not mic and slot["kind"] == "color_mic":
            report["color_mic_pending"].append(
                {"mesh": entry["mesh"], "slot": slot["index"], "material": slot["material"]})
            continue
        if mic:
            mesh.set_material(slot["index"], mic)
            changed = True
    if changed:
        unreal.EditorAssetLibrary.save_asset(asset_path)
        report["ok"].append(entry["mesh"])
    else:
        report["no_change"].append(entry["mesh"])


def run() -> None:
    with open(MANIFEST, encoding="utf-8") as fh:
        manifest = json.load(fh)
    report: dict = {"ok": [], "fail": [], "no_change": [], "color_mic_pending": []}
    for i, entry in enumerate(manifest["entries"]):
        assign_mesh(entry, report)
        if (i + 1) % 200 == 0:
            unreal.SystemLibrary.collect_garbage()
            unreal.log(f"assign progress {i + 1}/{len(manifest['entries'])}")
    with open(REPORT, "w", encoding="utf-8") as fh:
        json.dump(report, fh, indent=1)
    unreal.log(f"ASSIGN DONE ok={len(report['ok'])} fail={len(report['fail'])} "
               f"no_change={len(report['no_change'])} color_pending={len(report['color_mic_pending'])} "
               f"-> {REPORT}")


run()
