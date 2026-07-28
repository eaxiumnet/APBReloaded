"""Provenance and resolution gates for tools/scripts/apb_level_dump.py.

These are the tests that the fabricated pipeline could never have passed. Each one
targets a specific fail-open blocker confirmed by reading the code on 2026-07-28; see
work/retail_map_port_evidence.md for the citations.

Run:  python tools\\scripts\\test_apb_level_dump.py
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import apb_level_dump as A  # noqa: E402

STEM = "FinancialDistrict_Block09"
MAPS = A.RETAIL_MAPS / "FinancialDistrict"

_FAILURES: list[str] = []
_PASSES: list[str] = []


def check(name: str, cond: bool, detail: str = "") -> None:
    if cond:
        _PASSES.append(name)
        print(f"  PASS  {name}" + (f"  [{detail}]" if detail else ""))
    else:
        _FAILURES.append(f"{name}: {detail}")
        print(f"  FAIL  {name}  [{detail}]")


def test_import_outer_chain(pkg) -> None:
    """Blocker 3: the import outer index was read and discarded (apb_level_dump.py:132),
    so a negative StaticMesh index could not be resolved to package.object."""
    print("\n-- import outer chain --")
    imps = pkg.imports
    check("imports parsed", len(imps) > 0, f"count={len(imps)}")
    if not imps:
        return

    has_outer = all("outer" in i for i in imps)
    check("every import retains its outer index", has_outer,
          f"missing on {sum(1 for i in imps if 'outer' not in i)}/{len(imps)}")

    # StaticMeshComponent_885.StaticMesh == -591 (proven value from the golden export).
    idx = 591
    check("import 591 in range", idx - 1 < len(imps), f"imports={len(imps)}")
    if idx - 1 >= len(imps) or not has_outer:
        return

    qualified = A.import_path(pkg, -idx)
    check("import -591 resolves to a package-qualified path",
          isinstance(qualified, str) and qualified.count(".") >= 1,
          f"got={qualified!r}")
    check("resolved path is not a bare object name",
          isinstance(qualified, str) and "." in qualified and not qualified.startswith("."),
          f"got={qualified!r}")


def test_all_components_enumerated(data, pkg, rows) -> None:
    """Blocker 4: extract_actor_transforms.py returned on the first member of m_aComponents.

    Enumeration must be complete, but note what m_aComponents actually holds: measured on
    Block09 its three slots are [FeatureGroupComponent, null, PointLightComponent] on all
    137 sets. It is NOT the geometry list -- these 411 members contain zero meshes. Mesh
    geometry hangs off the actor's direct StaticMeshComponent property instead.
    """
    print("\n-- component-set enumeration (non-geometry: lights/features) --")
    sets = [r for r in rows if r["cls"] == "cStreamedComponentSet"]
    check("component sets found", len(sets) == 137, f"count={len(sets)}")

    total_members = 0
    sets_with_multiple = 0
    for r in sets:
        blob = data[r["off"]:r["off"] + r["size"]]
        members = A.component_members(blob, pkg.names)
        total_members += len(members)
        if len(members) > 1:
            sets_with_multiple += 1

    check("total component members exceeds set count",
          total_members > len(sets),
          f"members={total_members} sets={len(sets)}")
    check("at least one set has multiple components",
          sets_with_multiple > 0,
          f"multi-member sets={sets_with_multiple}")


def test_source_id_and_scale_presence(data, pkg, rows) -> None:
    """Blocker: a fabricated 1.0 scale was indistinguishable from an absent one, and no row
    carried provenance, so no row could be compared back to its source object."""
    print("\n-- provenance + missingness parity --")
    recs = A.placement_records(STEM, MAPS)
    check("records emitted", len(recs) > 0, f"count={len(recs)}")
    if not recs:
        return

    ids = [r.get("source_id") for r in recs]
    check("every record carries a source_id", all(ids), f"missing={sum(1 for i in ids if not i)}")
    check("source_ids are unique", len(set(ids)) == len(ids),
          f"unique={len(set(ids))} total={len(ids)}")

    sid = recs[0].get("source_id") or ""
    check("source_id is (pkg_sha, export_idx, component_idx)", sid.count(":") >= 2,
          f"got={sid!r}")

    check("scale_present recorded on every row",
          all("scale_present" in r for r in recs),
          f"missing={sum(1 for r in recs if 'scale_present' not in r)}")

    # Measured on Block09: 0/137 building components carry Scale3D, so scale_present
    # MUST be False everywhere -- a True here means a defaulted 1.0 is posing as sourced.
    explicit = [r for r in recs if r.get("scale_present")]
    defaulted = [r for r in recs if not r.get("scale_present")]
    check("no building row claims a sourced scale it does not have",
          len(explicit) == 0, f"explicit={len(explicit)} of {len(recs)}")
    check("defaulted rows carry unit scale",
          all(r.get("scale") == [1.0, 1.0, 1.0] for r in defaulted),
          f"defaulted={len(defaulted)}")


def test_building_rotation_is_absent_not_invented(data, pkg, rows) -> None:
    """Blocker: the shipped pipeline wrote a yaw ramp (i*11)%360 onto building placements.

    Retail cStreamedBuildingActor carries NO Rotation property (0/137 on Block09), so the
    only faithful output is rotation_present=False with a zero rotation. Any nonzero
    building yaw is fabricated by construction.
    """
    print("\n-- building rotation: absent in source, must not be invented --")
    recs = A.placement_records(STEM, MAPS)
    check("records emitted", len(recs) > 0, f"count={len(recs)}")
    if not recs:
        return

    check("rotation_present recorded on every row",
          all("rotation_present" in r for r in recs),
          f"missing={sum(1 for r in recs if 'rotation_present' not in r)}")
    claimed = [r for r in recs if r.get("rotation_present")]
    check("no building row claims a sourced rotation", len(claimed) == 0,
          f"claimed={len(claimed)} of {len(recs)}")

    yaws = [r["rotation"][1] for r in recs if r.get("rotation")]
    check("every building yaw is exactly zero", all(abs(y) < 1e-9 for y in yaws),
          f"nonzero={sum(1 for y in yaws if abs(y) >= 1e-9)}/{len(yaws)}")
    check("yaw is not the (i*11)%360 ramp",
          not any(abs(y - (i * 11) % 360) < 1e-6 for i, y in enumerate(yaws) if y),
          "ramp signature detected")


def test_rotation_bearing_classes_are_real(data, pkg, rows) -> None:
    """Classes that DO carry Rotation must decode to varied source values, not a ramp.

    PrefabInstance (227), PointNightLight (161), PointLight (57), SpotLight (26),
    SpotNightLight (25), cGraffitiCrimeTarget (22) are where retail rotation actually lives.
    """
    print("\n-- rotation-bearing classes decode real values --")
    cache = A.prop_cache(data, pkg, rows)
    by_cls: dict[str, list[float]] = {}
    for r in rows:
        rot = cache.get(r["idx"], {}).get("Rotation")
        if isinstance(rot, list) and len(rot) == 3:
            by_cls.setdefault(r["cls"], []).append(A.uru_to_deg(rot[1]))

    check("PrefabInstance carries Rotation", len(by_cls.get("PrefabInstance", [])) > 200,
          f"count={len(by_cls.get('PrefabInstance', []))}")

    for cls, yaws in sorted(by_cls.items(), key=lambda kv: -len(kv[1]))[:4]:
        if len(yaws) < 3:
            continue
        diffs = {round(b - a, 6) for a, b in zip(yaws, yaws[1:])}
        check(f"{cls} yaw is not an arithmetic ramp", len(diffs) > 1,
              f"n={len(yaws)} distinct_steps={len(diffs)}")
        check(f"{cls} yaw spans real variety", len(set(yaws)) > 2,
              f"distinct={len(set(yaws))} of {len(yaws)}")


def main() -> int:
    print(f"apb_level_dump gates :: {STEM}")
    data, pkg, rows = A.load(STEM, MAPS)
    print(f"loaded: {len(rows)} exports, {len(pkg.names)} names, {len(pkg.imports)} imports")

    test_import_outer_chain(pkg)
    test_all_components_enumerated(data, pkg, rows)
    test_source_id_and_scale_presence(data, pkg, rows)
    test_building_rotation_is_absent_not_invented(data, pkg, rows)
    test_rotation_bearing_classes_are_real(data, pkg, rows)

    print(f"\n{len(_PASSES)} passed, {len(_FAILURES)} failed")
    for f in _FAILURES:
        print(f"  FAILED: {f}")
    return 1 if _FAILURES else 0


if __name__ == "__main__":
    raise SystemExit(main())
