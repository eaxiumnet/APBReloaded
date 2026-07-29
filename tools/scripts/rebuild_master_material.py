import sys

sys.path.insert(0, r"D:\APBReloaded\tools\scripts")

import unreal  # noqa: E402
from apb_master_material import ensure_master_material, MASTER_PATH, TEX_PARAMS  # noqa: E402

mat = ensure_master_material(rebuild=True)
unreal.EditorAssetLibrary.save_asset(MASTER_PATH)

names = [str(n) for n in unreal.MaterialEditingLibrary.get_texture_parameter_names(mat)]
missing = [p for p in TEX_PARAMS if p not in names]
unreal.log("MASTER_REBUILD path=%s params=%s missing=%s"
           % (MASTER_PATH, names, missing))
