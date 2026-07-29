"""RED gate: renderable placements must come from the VERTEX-LIT edge.

Supersedes the archetype premise this file previously encoded. Measured on retail
FinancialDistrict_Block09 (see work/g1_one_to_one_goal.md F11-F16):

  * All 1177 non-null Archetype refs are IMPORTS into APBGame.u, and the no-mesh
    components share ONE class-default subobject. An archetype chain therefore cannot
    supply 79 distinct building meshes -- the old assertion here was unsatisfiable.
  * retail sets HiddenGame=True on ALL 137 direct StaticMeshComponents, so none of them
    is the rendered geometry. The visible mesh is the m_VertexLitComponent target: 57/57
    have a StaticMesh, no Hidden properties, no transform of their own, and are outered
    to a DIFFERENT cStreamedBuildingActor whose Location positions them (delta 0.000).
  * 137 = 57 namers + 57 hosts + 22 absent + 1 resolved-without-vertex-lit.

F8 called cStreamedBuildingActor_109/_110 meshless; they are in fact render HOSTS. _10/_11
really do have no geometry anywhere in the Block09 package family (Props/ArtProps/Design
all 0/12 at those Locations), so they must surface a reason code, never a substitute mesh.

Run: python tools/scripts/test_archetype_resolve.py
"""
from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import apb_level_dump as ald

STEM = "FinancialDistrict_Block09"
MAPS = ald.RETAIL_MAPS / "FinancialDistrict"

TOTAL_BUILDING_ACTORS = 137
EXPECTED_RENDERABLE = 57
EXPECTED_ABSENT = 22

# F8 called these meshless; they own the visible *_VertexLit_LOD component.
RENDER_HOSTS = ("cStreamedBuildingActor_109", "cStreamedBuildingActor_110")
# No geometry in Block09's package family; must report a reason, not a mesh.
GENUINELY_ABSENT = ("cStreamedBuildingActor_10", "cStreamedBuildingActor_11")

FAILURES: list[str] = []


def check(condition: bool, message: str) -> None:
    if condition:
        print(f"  PASS  {message}")
    else:
        print(f"  FAIL  {message}")
        FAILURES.append(message)


def test_fname_number_preserved() -> None:
    """F13: decode_prop dropped the FName Number, aliasing every instanced reference.

    StaticMeshComponent_0.m_VertexLitComponent stores names[726]='StaticMeshComponent'
    with Number=653, i.e. 'StaticMeshComponent_652' -- a different export. Decoding it as
    the bare base name silently collapses distinct components onto one name.
    """
    print("\n## NameProperty keeps its FName Number")
    names = ["None", "StaticMeshComponent"]
    body = (1).to_bytes(4, "little") + (653).to_bytes(4, "little")
    got = ald.decode_prop("NameProperty", None, body, None, names)
    check(got == "StaticMeshComponent_652",
          f"Number-suffixed FName decoded (got {got!r})")

    zero = (1).to_bytes(4, "little") + (0).to_bytes(4, "little")
    got0 = ald.decode_prop("NameProperty", None, zero, None, names)
    check(got0 == "StaticMeshComponent",
          f"Number==0 stays unsuffixed (got {got0!r})")


def test_export_table_available() -> None:
    """Host resolution needs the export table's Outer index, which parse_package discarded.

    Layout proven against `umodel -list`: MATCH=3400 MISMATCH=0 at ArVer 564 / Licensee 33.
    """
    print("\n## export table is parsed (Outer needed to find the render host)")
    check(hasattr(ald, "export_table"), "ald.export_table() exists")
    if not hasattr(ald, "export_table"):
        return
    data, pkg, rows = ald.load(STEM, MAPS)
    table = ald.export_table(data)
    check(len(table) == len(rows),
          f"export_table length matches umodel rows ({len(table)} vs {len(rows)})")
    check(all("outer" in e and "archetype" in e for e in table),
          "every entry retains outer + archetype")


def test_vertexlit_renderables() -> None:
    """The rendered geometry is the m_VertexLitComponent target, positioned by its host."""
    print("\n## renderable placements come from the vertex-lit edge")
    check(hasattr(ald, "renderable_placements"), "ald.renderable_placements() exists")
    if not hasattr(ald, "renderable_placements"):
        return
    rows = ald.renderable_placements(STEM, MAPS)
    good = [r for r in rows if not r.get("reason")]
    check(len(good) == EXPECTED_RENDERABLE,
          f"{EXPECTED_RENDERABLE} renderable rows (got {len(good)})")

    meshes = {r.get("mesh_path") for r in good}
    check(len(meshes) == EXPECTED_RENDERABLE,
          f"{EXPECTED_RENDERABLE} distinct meshes (got {len(meshes)})")
    check(all(isinstance(m, str) and m.endswith("_VertexLit_LOD") for m in meshes),
          "every renderable mesh is a *_VertexLit_LOD asset")
    check(all(r.get("visible") is True for r in good),
          "renderable rows are the visible component, not the HiddenGame one")

    hosts = {r.get("host") for r in good}
    for actor in RENDER_HOSTS:
        check(actor in hosts, f"{actor} is a render host (F8 called it meshless)")

    placed = [r for r in good
              if isinstance(r.get("location"), list) and len(r["location"]) == 3]
    check(len(placed) == len(good),
          f"every renderable row carries a 3-component Location (got {len(placed)})")
    check(all(r.get("transform_source") == "host_actor" for r in good),
          "Location provenance is the host actor (component stores no transform)")


def test_absent_geometry_is_reported() -> None:
    """The 22 with no geometry anywhere must surface a reason, never a substitute mesh."""
    print("\n## genuine absences are reported, not back-filled")
    if not hasattr(ald, "renderable_placements"):
        check(False, "ald.renderable_placements() exists")
        return
    rows = ald.renderable_placements(STEM, MAPS)
    absent = [r for r in rows if r.get("reason") == "no_geometry_in_package_family"]
    check(len(absent) == EXPECTED_ABSENT,
          f"{EXPECTED_ABSENT} rows report absent geometry (got {len(absent)})")
    check(all(r.get("mesh_path") is None for r in absent),
          "absent rows carry no mesh_path")

    by_actor = {r.get("actor"): r for r in rows}
    for actor in GENUINELY_ABSENT:
        row = by_actor.get(actor)
        check(row is not None, f"{actor} is emitted rather than dropped")
        if row is None:
            continue
        check(row.get("reason") == "no_geometry_in_package_family",
              f"{actor} reason is explicit (got {row.get('reason')!r})")


def test_conservation() -> None:
    """candidates == emitted + reasoned, so nothing vanishes silently."""
    print("\n## conservation over all 137 cStreamedBuildingActor")
    if not hasattr(ald, "renderable_placements"):
        check(False, "ald.renderable_placements() exists")
        return
    rows = ald.renderable_placements(STEM, MAPS)
    actors = {r.get("actor") for r in rows if r.get("actor")}
    check(len(actors) == TOTAL_BUILDING_ACTORS,
          f"all {TOTAL_BUILDING_ACTORS} actors accounted (got {len(actors)})")

    good = [r for r in rows if not r.get("reason")]
    reasoned = [r for r in rows if r.get("reason")]
    check(len(good) + len(reasoned) == len(rows),
          f"emitted+reasoned == rows ({len(good)}+{len(reasoned)} vs {len(rows)})")
    check(len(good) == EXPECTED_RENDERABLE and len(reasoned) == len(rows) - len(good),
          f"split is {EXPECTED_RENDERABLE} renderable / {len(reasoned)} reasoned")


def main() -> int:
    print(f"# vertex-lit renderable gate: {STEM}")
    test_fname_number_preserved()
    test_export_table_available()
    test_vertexlit_renderables()
    test_absent_geometry_is_reported()
    test_conservation()
    print(f"\nFAILS={len(FAILURES)}")
    return 1 if FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
