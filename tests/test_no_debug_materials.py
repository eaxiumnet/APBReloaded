"""Gate: runtime fallback must not stamp engine DEBUG materials onto district geometry.

EnsureVisibleMeshMaterials existed to rescue umodel-imported slots that shipped empty or
WorldGrid-only, because M_APBMaster did not compile and everything rendered black. That
premise no longer holds: M_APBMaster compiles clean on PCD3D_SM6 as of the sampler-type
fix, and 2435/2559 Financial meshes already carry MICs parented to it
(work/evidence/material_census.json). The debug candidates now actively harm fidelity -
LevelColorationUnlitMaterial renders flat unlit primary colours, which is the "red ground"
defect: it lands on the placeholder ground plane and on the 124 WorldGrid-only meshes.

A 1:1 port must never present engine debug colouration as district surfacing.

Runs under plain CPython; does not import the `unreal` module.
"""
from __future__ import annotations

from pathlib import Path

REPO = Path(__file__).parent.parent
LOADER = REPO / "Source" / "APBReloaded" / "Systems" / "District" / "APBDistrictPlacementLoader.cpp"

BANNED = (
    "LevelColorationUnlitMaterial",
    "LevelColorationLitMaterial",
    "EngineDebugMaterials",
)


def code_lines(path: Path) -> list[str]:
    out = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        stripped = raw.lstrip()
        if stripped.startswith("//"):
            continue
        out.append(raw)
    return out


def main() -> int:
    fails = []

    if not LOADER.is_file():
        print(f"FAIL LOADER_ABSENT path={LOADER}")
        print("FAILS=1")
        return 1

    code = "\n".join(code_lines(LOADER))
    for token in BANNED:
        if token in code:
            fails.append(f"DEBUG_MATERIAL_IN_FALLBACK token={token}")

    for line in fails:
        print(f"FAIL {line}")
    print(f"FAILS={len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
