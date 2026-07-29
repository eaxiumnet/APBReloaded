"""Gate: the freeroam district must not render a placeholder ground sheet.

Measured, not assumed. Probe run work/evidence/freeroam_probe_emissive.log produced
work/evidence/financial_render.png, where a colour census found one quantised bucket,
rgb(248,248,248), holding 113881 of 921600 px = 12.36% of frame. A lighting-driven blowout
spreads across many colours; a single flat bucket that size is one flat unlit surface. That
surface was /Engine/BasicShapes/Plane scaled to 4km at Z=+5 wearing the BasicShapeMaterial
fallback via EnsureVisibleMeshMaterials. After hiding it and dropping it to Z=-2000 the same
bucket measures 252 px = 0.03% (work/evidence/financial_render_realground.png), while midtone
- actual shaded geometry - rises 20.76% -> 25.89%.

Two independent reasons it must not be a *visible* mesh any more:

1. Occlusion. The real retail ground now exists - Financial_Block09_realv2.json carries 348
   layer=road and 630 layer=terrain rows, transform_source=retail_actor, and road Z sits at
   about -4.99. A plane at Z=+5 is ABOVE the roads and would hide the geometry it is standing
   in for.

2. It never worked as a floor anyway. The probe pawn spawned at Z=500 and settled at Z=-887.2
   with ground=box_collision, i.e. it fell straight through this plane. /Engine/BasicShapes/Plane
   ships no simple collision primitive, so SetCollisionEnabled(QueryAndPhysics) on it blocks
   nothing.

An INVISIBLE collision backstop is still allowed - that is world bounds, not a visual
placeholder - but it must not be visible and must not be painted by the material helper.

Runs under plain CPython; does not import the `unreal` module.
"""
from __future__ import annotations

import re
from pathlib import Path

REPO = Path(__file__).parent.parent
GM = REPO / "Source" / "APBReloaded" / "Systems" / "District" / "APBFreeroamGameMode.cpp"
MANIFEST = REPO / "Content" / "Data" / "district_placements" / "Financial_Block09_realv2.json"

fails: list[str] = []
src = GM.read_text(encoding="utf-8", errors="replace")

# Isolate the ground-plane block so we only judge that construct. Walk OUTWARD from the
# Plane reference to the enclosing brace, then forward to its match. Do not rfind twice -
# that lands in the preceding sibling block (measured: it captured the PPV block instead).
block = ""
m = re.search(r"BasicShapes/Plane", src)
if m:
    start = src.rfind("{", 0, m.start())
    if start != -1:
        depth, i = 0, start
        while i < len(src):
            if src[i] == "{":
                depth += 1
            elif src[i] == "}":
                depth -= 1
                if depth == 0:
                    block = src[start:i + 1]
                    break
            i += 1

if not m:
    print("PLANE_ABSENT: no BasicShapes/Plane reference - placeholder fully removed")
else:
    print("PLANE_PRESENT: BasicShapes/Plane still referenced; requiring invisible backstop")
    if "EnsureVisibleMeshMaterials" in block:
        fails.append(
            "ground plane still calls EnsureVisibleMeshMaterials -> BasicShapeMaterial "
            "is the measured rgb(240,240,240) clip source (n=27531)"
        )
    if not re.search(r"SetVisibility\s*\(\s*false|SetHiddenInGame\s*\(\s*true|bHiddenInGame\s*=\s*true", block):
        fails.append("ground plane is not made invisible (no SetVisibility(false)/SetHiddenInGame(true))")
    zs = [float(z.rstrip(".") or "0") for z in re.findall(r"FVector\s+\w+\s*\(\s*At\.X\s*,\s*At\.Y\s*,\s*(-?[\d.]+)f", block)]
    if zs and max(zs) > -100.0:
        fails.append(
            f"backstop Z={max(zs)} is not safely below retail road level (~-5); "
            "it must not sit at or above the real ground"
        )

# The replacement ground must actually exist, or removing the plane just yields a void.
if MANIFEST.is_file():
    import json

    rows = json.loads(MANIFEST.read_text(encoding="utf-8")).get("placements", [])
    road = sum(1 for r in rows if r.get("layer") == "road")
    terr = sum(1 for r in rows if r.get("layer") == "terrain")
    print(f"GROUND_ROWS road={road} terrain={terr}")
    if road == 0 or terr == 0:
        fails.append(f"real ground missing from manifest (road={road} terrain={terr}); plane is still load-bearing")
else:
    fails.append(f"manifest not found: {MANIFEST}")

for f in fails:
    print(f"FAIL {f}")
print(f"FAILS={len(fails)}")
raise SystemExit(1 if fails else 0)
