#!/usr/bin/env python3
"""Enumerate the retail APB Packages tree into per-category extraction list files.

Turns the user's "rip ALL" into finite, categorized .txt lists of full .upk
paths that extract_batch.ps1 consumes. Read-only; writes only under work/.
"""
from __future__ import annotations

import re
from collections import Counter
from pathlib import Path

ROOT = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages")
OUT = Path(r"D:\APBReloaded\work\rip_lists")
OUT.mkdir(parents=True, exist_ok=True)

# ordered rules: first match wins. (category, predicate)
def rel(p: Path) -> str:
    return str(p.relative_to(ROOT)).replace("\\", "/")

ANIM = re.compile(r"(^|/)Anim[_/]", re.I)

def categorize(p: Path) -> str:
    r = rel(p)
    name = p.name
    if ANIM.search(r):
        return "_anim_skip"          # animation packages: not meshes, skip for now
    if re.search(r"WeaponSkin", name, re.I):
        return "weapon_skins"
    if r.startswith("DesignObjects/Weapons"):
        return "weapons_base"
    if r.startswith(("APB_Vehicles", "Vehicles")):
        return "vehicles"
    if re.search(r"Contact_", name):
        return "contacts_npc"
    if r.startswith(("Character", "APB_CharacterTool", "CharacterCustomisation")):
        return "characters"
    if re.search(r"Wardrobe|Clothing", name, re.I):
        return "clothing"
    if r.startswith("SymbolEditor"):
        return "symbols"
    return "_other"

buckets: dict[str, list[str]] = {}
for upk in ROOT.rglob("*.upk"):
    cat = categorize(upk)
    buckets.setdefault(cat, []).append(str(upk))

summary = Counter()
for cat, paths in sorted(buckets.items()):
    paths.sort()
    summary[cat] = len(paths)
    if cat.startswith("_"):
        continue  # don't emit skip/other as rip lists
    (OUT / f"{cat}.txt").write_text("\n".join(paths) + "\n", encoding="utf-8")

print("=== category counts (retail Packages) ===")
for cat, n in sorted(summary.items(), key=lambda kv: -kv[1]):
    emitted = "" if cat.startswith("_") else f"  -> {cat}.txt"
    print(f"  {cat:16} {n:6}{emitted}")
print(f"\nlists written under {OUT}")
print("total upk:", sum(summary.values()))
