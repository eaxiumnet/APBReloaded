# Animation semantic-parity recording: bind extracted PSA files to retail source upks
# with D17 evidence chains and parsed semantic metrics (bones, anims, keys invariant).
# Run: python tools/scripts/record_animation_import.py [--apply]
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
from pathlib import Path

from validate_m3r_semantic_parity import parse_psa

ROOT = Path(r"D:\APBReloaded")
LEDGER = ROOT / "tools" / "import_ledger.json"
EVIDENCE_DIR = ROOT / "work" / "evidence" / "animation_parity"
RETAIL = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
    r"\APBGame\Content\Release\Packages"
)
SCHEMA = "apb_animation_parity_v1"
# Family roots under Packages that own animation source upks.
FAMILY_HINTS = ("weapon", "anim/", "vehicles", "materialdatabase")


def sha256(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def build_upk_index() -> dict[str, list[str]]:
    index: dict[str, list[str]] = {}
    for upk in RETAIL.rglob("*.upk"):
        index.setdefault(upk.stem.lower(), []).append(upk)
    return index


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    upk_index = build_upk_index()
    EVIDENCE_DIR.mkdir(parents=True, exist_ok=True)
    ledger = json.loads(LEDGER.read_text(encoding="utf-8"))
    entries = {e["asset_key"]: e for e in ledger.get("entries", [])}
    evidence: dict[str, dict] = {}
    skipped: list[str] = []
    now = datetime.datetime.now(datetime.timezone.utc).isoformat()

    for psa in sorted(ROOT.joinpath("Content/Extracted").rglob("*.psa")):
        rel = psa.relative_to(ROOT.joinpath("Content/Extracted"))
        parts = rel.parts
        pkg = None
        for i, part in enumerate(parts):
            if part.lower() == "animset" and i >= 1:
                pkg = parts[i - 1]
                break
        if pkg is None:
            pkg = parts[1] if len(parts) > 1 else parts[0]
        cands = upk_index.get(pkg.lower(), [])
        fam = next((c for c in cands if c.relative_to(RETAIL).as_posix().lower()
                    .startswith(FAMILY_HINTS)), None)
        upk = fam or (cands[0] if cands else None)
        if upk is None:
            skipped.append(f"no_upk {psa.relative_to(ROOT)}")
            continue
        try:
            metrics = parse_psa(psa)
        except ValueError as exc:
            skipped.append(f"bad_psa {psa.relative_to(ROOT)}: {exc}")
            continue
        upk_rel = upk.relative_to(RETAIL).as_posix()
        locator = "${retail_steam}/APBGame/Content/Release/Packages/" + upk_rel
        src_hash = sha256(upk)
        psa_rel = psa.relative_to(ROOT).as_posix()
        psa_hash = sha256(psa)
        leaf = psa.stem
        asset_key = f"retail:{upk_rel}#{leaf}"
        if asset_key in entries:
            continue
        if pkg.lower() not in evidence:
            evidence[pkg.lower()] = {
                "schema": SCHEMA,
                "package": pkg,
                "source_locator": locator,
                "source_sha256": src_hash,
                "records": [],
            }
        evidence[pkg.lower()]["records"].append({
            "asset_key": asset_key,
            "psa": psa_rel,
            "psa_sha256": psa_hash,
            "bones": metrics["bones"],
            "anim_count": metrics["anim_count"],
            "keys": metrics["keys"],
        })
        entries[asset_key] = {
            "asset_key": asset_key,
            "source_build": "retail",
            "source_locator": locator,
            "source_package": pkg,
            "source_object": leaf,
            "source_sha256": src_hash,
            "extractor": "tools/UEViewer/umodel_64.exe",
            "extractor_args": "legacy payload extraction settings not retained",
            "intermediate_path": psa_rel,
            "intermediate_sha256": psa_hash,
            "conversion_settings": {
                "converter": "retail animation extraction",
                "format": "PSA (ActorX v2)",
                "normalize": False,
            },
            "dest": f"/Game/Imported/Animations/{leaf}",
            "asset_class": "AnimSet",
            "status": "imported",
            "validation": {
                "class": "AnimSet",
                "source_build": "retail",
                "bones": metrics["bones"],
                "anim_count": metrics["anim_count"],
                "keys": metrics["keys"],
                "anim_names": [a["name"] for a in metrics["anims"]],
                "frames": [a["frames"] for a in metrics["anims"]],
            },
            "uasset_path": None,
            "d17_evidence": [
                {
                    "record_key": asset_key,
                    "schema": SCHEMA,
                    "path": f"work/evidence/animation_parity/{pkg}.json",
                    "sha256": "PENDING",
                    "fields": {
                        "source_locator": locator,
                        "source_sha256": src_hash,
                        "extracted_file": psa_rel,
                        "extracted_sha256": psa_hash,
                        "output_file": psa_rel,
                        "output_sha256": psa_hash,
                        "bones": metrics["bones"],
                        "anim_count": metrics["anim_count"],
                    },
                }
            ],
            "updated": now,
        }

    for pkg_lower, ev in evidence.items():
        ev_path = EVIDENCE_DIR / f"{ev['package']}.json"
        ev_path.write_text(json.dumps(ev, indent=1), encoding="utf-8")
        ev_hash = sha256(ev_path)
        ev_rel = f"work/evidence/animation_parity/{ev['package']}.json"
        for entry in entries.values():
            d17 = entry.get("d17_evidence")
            if d17 and d17[0].get("path") == ev_rel:
                d17[0]["sha256"] = ev_hash

    ledger["entries"] = list(entries.values())
    ledger["updated"] = now
    if args.apply:
        LEDGER.write_text(json.dumps(ledger, indent=2) + "\n", encoding="utf-8")
    print(f"ANIMATION_LEDGER rows={len(entries)} evidence_files={len(evidence)} skipped={len(skipped)}")
    for s in skipped[:8]:
        print(f"  SKIP {s}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
