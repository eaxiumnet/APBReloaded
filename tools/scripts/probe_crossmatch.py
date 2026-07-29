import json, os

base = r'D:\APBReloaded\Content\Data\district_placements'
full = json.load(open(os.path.join(base, 'Financial_Block09_bound.json'), encoding='utf-8'))
real = json.load(open(os.path.join(base, 'Financial_Block09_real.json'), encoding='utf-8'))
fp = full['placements']
rp = real['placements']

# bound subset for Block09 buildings only
b09 = [p for p in fp if p.get('package') == 'FinancialDistrict_Block09']


def bbox(pl):
    xs = [p['location'][0] for p in pl]
    ys = [p['location'][1] for p in pl]
    return (min(xs), max(xs), min(ys), max(ys))


rbb = bbox(rp)
bbb = bbox(b09)
print('REAL Block09 bbox  x[%.0f..%.0f] y[%.0f..%.0f]' % rbb)
print('BOUND Block09 bbox x[%.0f..%.0f] y[%.0f..%.0f] (n=%d)' % (bbb[0], bbb[1], bbb[2], bbb[3], len(b09)))

# For each REAL building, is there a BOUND Block09 placement within 500cm (5m)?
matched = 0
near = []
for r in rp:
    rx, ry = r['location'][0], r['location'][1]
    best = 1e18
    for b in b09:
        bx, by = b['location'][0], b['location'][1]
        d = (rx - bx) ** 2 + (ry - by) ** 2
        if d < best:
            best = d
    dist = best ** 0.5
    near.append(dist)
    if dist < 500:
        matched += 1
near.sort()
print('\nREAL->BOUND nearest-neighbour (cm):')
print('  matched<5m: %d/%d' % (matched, len(rp)))
print('  median nn dist: %.0f cm  min: %.0f  max: %.0f' % (near[len(near) // 2], near[0], near[-1]))

# rotation reality: does bound Block09 share rotations with a real street grid?
from collections import Counter
brot = Counter(tuple(p.get('rotation', [0, 0, 0])) for p in b09)
print('\nBOUND Block09 rotations top:', brot.most_common(8))

# check mesh_id coherence: do bound Block09 meshes carry the B09 tag?
b09tag = sum(1 for p in b09 if '_B09' in p.get('mesh_id', '') or 'Block09' in p.get('mesh_id', ''))
print('\nBOUND Block09 placements whose mesh_id references B09/Block09: %d/%d' % (b09tag, len(b09)))
sample = [p.get('mesh_id', '') for p in b09[:8]]
print('  sample mesh_ids:', sample)
