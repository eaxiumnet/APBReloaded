"""Gate: M_APBMaster must compile cleanly on PCD3D_SM6 with no sampler-type mismatch.

Approach: scan real -game RHI log artifacts for the known failure signatures.
  RED fixture  – freeroam_probe_RED.log  – must contain both signatures (detector check).
  GREEN fixture – freeroam_probe_editor.log – must contain neither (regression gate).

Source guard: tools/scripts/apb_master_material.py must use BaseFlattenNormalMap and
guard missing default textures with RuntimeError.

Does NOT import the `unreal` module; runs under plain CPython outside the editor.
"""
from __future__ import annotations

import sys
from pathlib import Path

REPO = Path(__file__).parent.parent
LOG_RED = REPO / "work" / "evidence" / "freeroam_probe_RED.log"
LOG_GREEN = REPO / "work" / "evidence" / "freeroam_probe_editor.log"
MASTER_SCRIPT = REPO / "tools" / "scripts" / "apb_master_material.py"

SIG_COMPILE = "Failed to compile Material"
SIG_SAMPLER = "Sampler type is"


def scan_log(path: Path) -> dict[str, int]:
    """Return occurrence counts of each failure signature in the log file."""
    text = path.read_text(encoding="utf-8", errors="replace")
    return {
        SIG_COMPILE: text.count(SIG_COMPILE),
        SIG_SAMPLER: text.count(SIG_SAMPLER),
    }


def main() -> int:
    fails = []

    if not LOG_RED.exists():
        print(f"SKIP RED_LOG_ABSENT path={LOG_RED}")
    else:
        counts = scan_log(LOG_RED)
        if counts[SIG_COMPILE] < 1:
            fails.append(
                f"RED_SIG_MISSING sig={SIG_COMPILE!r} count={counts[SIG_COMPILE]}"
            )
        if counts[SIG_SAMPLER] < 1:
            fails.append(
                f"RED_SIG_MISSING sig={SIG_SAMPLER!r} count={counts[SIG_SAMPLER]}"
            )

    if not LOG_GREEN.exists():
        print(f"SKIP GREEN_LOG_ABSENT path={LOG_GREEN}")
    else:
        counts = scan_log(LOG_GREEN)
        if counts[SIG_COMPILE] != 0:
            fails.append(f"GREEN_HAS_COMPILE_FAILURE count={counts[SIG_COMPILE]}")
        if counts[SIG_SAMPLER] != 0:
            fails.append(f"GREEN_HAS_SAMPLER_MISMATCH count={counts[SIG_SAMPLER]}")

    src = MASTER_SCRIPT.read_text(encoding="utf-8")
    if "BaseFlattenNormalMap" not in src:
        fails.append("SRC_MISSING_BASENORMAL expected=BaseFlattenNormalMap")
    code_lines = [ln for ln in src.splitlines() if not ln.lstrip().startswith("#")]
    code = "\n".join(code_lines)
    if "EngineMaterials/DefaultNormal" in code:
        fails.append("SRC_USES_REFUTED_DEFAULTNORMAL in_non_comment_code=True")
    if "RuntimeError" not in src:
        fails.append("SRC_MISSING_RUNTIMEERROR _add_tex_param must raise on missing default")

    for line in fails:
        print(f"FAIL {line}")
    print(f"FAILS={len(fails)}")
    return 1 if fails else 0


if __name__ == "__main__":
    raise SystemExit(main())
