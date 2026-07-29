import json, os
from collections import Counter

base = r'D:\APBReloaded\Content\Data\district_placements'
full = json.load(open(os.path.join(base, 'Financial_Block09_bound.json'), encoding='utf-8'))
real = json.load(open(os.path.join(base, 'Financial_Block09_real.json'), encoding='utf-8'))
fp = full['placements']
rp = real['placements']


def bbox(pl):
    xs = [p['location'][0] for p in pl]
    ys = [p['location'][1] for p in pl]
    zs = [p['location'][2] for p in pl]
    return (min(xs), max(xs), min(ys), max(ys), min(zs), max(zs))


print('=== ACTOR CLASS breakdown ===')
print('  BOUND:', Counter(p.get('actor_class', '') for p in fp).most_common())
print('  REAL :', Counter(p.get('actor_class', '') for p in rp).most_common())

print('\n=== LAYER breakdown ===')
print('  BOUND:', Counter(p.get('layer', '') for p in fp).most_common())
print('  REAL :', Counter(p.get('layer', '') for p in rp).most_common())

print('\n=== PACKAGE breakdown (BOUND top 12) ===')
print('  ', Counter(p.get('package', '') for p in fp).most_common(12))

print('\n=== SPATIAL bounding box (cm) ===')
bx = bbox(fp)
rx = bbox(rp)
print('  BOUND x[%.0f..%.0f] y[%.0f..%.0f] z[%.0f..%.0f] span=(%.0f x %.0f)'
      % (bx[0], bx[1], bx[2], bx[3], bx[4], bx[5], bx[1] - bx[0], bx[3] - bx[2]))
print('  REAL  x[%.0f..%.0f] y[%.0f..%.0f] z[%.0f..%.0f] span=(%.0f x %.0f)'
      % (rx[0], rx[1], rx[2], rx[3], rx[4], rx[5], rx[1] - rx[0], rx[3] - rx[2]))

print('\n=== CROSS-CHECK actor-name overlap ===')
bn = set(p.get('actor', '') for p in fp)
rn = set(p.get('actor', '') for p in rp)
print('  bound actors:', len(bn), 'real actors:', len(rn), 'overlap:', len(bn & rn))
print('  real sample:', list(rn)[:4])
print('  bound sample:', list(bn)[:4])

print('\n=== source_packages ===')
print('  BOUND:', str(full.get('source_packages'))[:300])
print('  REAL :', str(real.get('source_packages'))[:300])
