import sys; sys.path.insert(0, 'tools/scripts')
import apb_level_dump as ald
from pathlib import Path
from collections import Counter

maps = ald.RETAIL_MAPS / 'FinancialDistrict'
data, pkg, rows = ald.load('FinancialDistrict_Props', maps)
by_idx = {r['idx']: r for r in rows}
cache = ald.prop_cache(data, pkg, rows)

# Inspect a cStreamedLightingStaticMeshActor
for row in rows:
    if row['cls'] == 'cStreamedLightingStaticMeshActor':
        props = cache.get(row['idx'], {})
        print(f'Actor: {row["name"]} idx={row["idx"]}')
        print(f'  Location: {props.get("Location")}')
        print(f'  Rotation: {props.get("Rotation")}')
        smc_ref = props.get('StaticMeshComponent')
        print(f'  StaticMeshComponent ref: {smc_ref} (type={type(smc_ref).__name__})')
        if isinstance(smc_ref, int) and smc_ref > 0:
            crow = ald.deref(smc_ref, by_idx)
            if crow:
                cp = cache.get(crow['idx'], {})
                print(f'  Component: {crow["name"]} class={crow["cls"]}')
                mref = cp.get('StaticMesh')
                print(f'  Component StaticMesh: {mref} (type={type(mref).__name__})')
                if isinstance(mref, int):
                    if mref < 0:
                        print(f'  import_path: {ald.import_path(pkg, mref)}')
                        print(f'  import_class: {ald.import_class(pkg, mref)}')
                    else:
                        local = ald.deref(mref, by_idx)
                        print(f'  local export: {local}')
            else:
                print(f'  Component ref {smc_ref} did not resolve via deref')
        print('---')
        break

# Also check StaticMeshActor
for row in rows:
    if row['cls'] == 'StaticMeshActor':
        props = cache.get(row['idx'], {})
        print(f'StaticMeshActor: {row["name"]} idx={row["idx"]}')
        print(f'  Location: {props.get("Location")}')
        print(f'  Rotation: {props.get("Rotation")}')
        smc_ref = props.get('StaticMeshComponent')
        print(f'  StaticMeshComponent ref: {smc_ref}')
        if isinstance(smc_ref, int) and smc_ref > 0:
            crow = ald.deref(smc_ref, by_idx)
            if crow:
                cp = cache.get(crow['idx'], {})
                mref = cp.get('StaticMesh')
                print(f'  Component StaticMesh: {mref}')
                if isinstance(mref, int) and mref < 0:
                    print(f'  import_path: {ald.import_path(pkg, mref)}')
        print('---')
        break

# Check what the mesh_paths look like for all prop records
recs = ald.prop_placement_records('FinancialDistrict_Props', maps)
paths = [r.get('mesh_path') for r in recs if r.get('mesh_path')]
print(f'Records with mesh_path: {len(paths)}/{len(recs)}')
for p in paths[:5]:
    print(f'  {p}')

# Check if these assets exist in the imported index
import write_block09_real_manifest as bw
asset_root = Path('D:/APBReloaded/Content/Imported/Districts/Financial')
index = bw.asset_index(asset_root)
print(f'\nAsset index size: {len(index)}')
for p in paths[:5]:
    stem = p.rsplit('.', 1)[-1]
    sanitized = bw.sanitized_stem(stem)
    found = bw.resolve_asset(p, index)
    print(f'  stem={stem} sanitized={sanitized} found={found}')
