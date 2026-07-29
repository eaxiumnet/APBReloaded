"""Resolver must map a space/paren archetype VertexLit stub to its own base mesh.

F19. The 22 rows previously labelled `retail_geometry_not_recovered` are NOT absent
from retail: each owns a distinct `*_LOD_0` base mesh in the SAME Block09 package
(22 KB - 403 KB, 22/22 distinct sizes) and each is ALREADY imported as a .uasset.

Two defects blocked them:
  1. `sanitized_stem` maps each of " ()" to "_" per character, so
     `Industrial Zone (LC)_0001` -> `Industrial_Zone__LC__0001` (double underscore),
     while the importer wrote `Industrial_Zone_LC_0001` (single).
  2. `resolve_geometry` trusts only upstream `base_mesh_path` and has no
     derivational fallback from `mesh_path` (strip `_VertexLit`).

The 35 rows that DID resolve are all `Generic_NNNN` - no spaces, no parens - so
defect 1 was a no-op for them and stayed invisible.

This is NOT the F14 archetype fabrication: that collapsed 79 distinct buildings onto
one shared mesh. Here each stub maps to its OWN same-named base, one-to-one.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import write_block09_real_manifest as writer

ASSET_ROOT = Path(r"D:\APBReloaded\Content\Imported\Districts\Financial")
MANIFEST = Path(r"D:\APBReloaded\Content\Data\district_placements"
                r"\Financial_Block09_unit.json")

FAILS: list[str] = []


def check(condition: bool, label: str) -> None:
    print(f"  {'ok  ' if condition else 'FAIL'}  {label}")
    if not condition:
        FAILS.append(label)


def stubs_from_manifest() -> list[dict]:
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    return [row for row in manifest["placements"]
            if row.get("edge") == "m_VertexLitComponent"
            and row.get("mesh_path")
            and any(ch in row["mesh_path"] for ch in " ()")]


def test_sanitized_stem_collapses_runs() -> None:
    """`Zone (LC)` must yield single underscores, matching what the importer wrote."""
    print("test_sanitized_stem_collapses_runs")
    got = writer.sanitized_stem("FinancialDistrict_Block09_Industrial Zone (LC)_0001_LOD")
    check("__" not in got, f"no double underscore in {got!r}")
    check(got == "FinancialDistrict_Block09_Industrial_Zone_LC_0001_LOD",
          f"matches imported asset stem (got {got!r})")


def test_archetype_stubs_resolve_to_own_base() -> None:
    """Every space/paren stub binds to its OWN distinct base asset via base_mesh_path.

    base_mesh_path is input-only (the extractor supplies it; the manifest drops it), so
    it is reconstructed here exactly as the walker emits it. Before the collapse fix this
    path returned retail_geometry_not_recovered for all 22.
    """
    print("test_archetype_stubs_resolve_to_own_base")
    index = writer.asset_index(ASSET_ROOT)
    stubs = stubs_from_manifest()
    check(len(stubs) == 22, f"22 archetype stubs in manifest (got {len(stubs)})")

    resolved: dict[str, str] = {}
    for row in stubs:
        record = dict(row)
        record["base_mesh_path"] = row["mesh_path"].replace("_VertexLit_LOD", "_LOD")
        asset, _source, resolution = writer.resolve_geometry(record, index)
        check(resolution == "linked_hidden_base_mesh",
              f"{row['mesh_path'].rsplit('.', 1)[-1][:52]} -> linked_hidden_base_mesh "
              f"(got {resolution})")
        if asset is not None:
            resolved[row["mesh_path"]] = asset.stem

    check(len(resolved) == len(stubs),
          f"all {len(stubs)} stubs bind an asset (got {len(resolved)})")
    check(len(set(resolved.values())) == len(resolved),
          f"each binds a DISTINCT base asset - no shared-mesh fabrication "
          f"({len(set(resolved.values()))} distinct of {len(resolved)})")

    for mesh_path, stem in resolved.items():
        want = mesh_path.rsplit(".", 1)[-1].replace("_VertexLit_LOD", "_LOD")
        check(stem.lower().startswith(writer.sanitized_stem(want).lower()),
              f"{stem[:58]} binds its own name, not a sibling")


def main() -> int:
    test_sanitized_stem_collapses_runs()
    test_archetype_stubs_resolve_to_own_base()
    print(f"\nFAILS={len(FAILS)}")
    for label in FAILS:
        print(f"  - {label}")
    return 1 if FAILS else 0


if __name__ == "__main__":
    raise SystemExit(main())
