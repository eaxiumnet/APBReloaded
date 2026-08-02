#!/usr/bin/env python3
"""M3R task-16 atomic promotion: set status=verified only for fully proven rows.

Mirrors tools/check_strict_asset_provenance.ps1 verified-row validation exactly,
then requires allowlist eligibility (mirrors tools/promote_verified_assets.ps1):
destination under /Game/, non-empty asset_class, allowed source_build, and a
passing source-locator test. Dry-run by default; --apply performs the transition.
Idempotent: already-verified rows are never touched, and a second --apply run
produces byte-identical ledger output.
"""

from __future__ import annotations

import argparse
import copy
import datetime
import hashlib
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
LEDGER_DEFAULT = ROOT / "tools" / "import_ledger.json"
ALLOWED_SOURCE_BUILDS = ("retail", "2011", "2011+retail", "apbdb")
SHA256_RE = re.compile(r"^[0-9a-fA-F]{64}$")
EVIDENCE_OUTPUT_FIELDS = ("output_file", "output_path")
VERIFIED_BY = "tools/scripts/promote_verified_batch.py"


def read_json(path: Path) -> object:
    return json.loads(path.read_text(encoding="utf-8"))


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def project_path(path: str) -> Path:
    if not path:
        return Path()
    candidate = Path(path)
    if candidate.is_absolute():
        return candidate
    return ROOT / candidate


def get_field(entry: dict, names: tuple[str, ...]) -> object:
    for name in names:
        value = entry.get(name)
        if value is not None:
            return value
    return None


def validate_file_hash(
    failures: list[str],
    asset_key: str,
    kind: str,
    path_value: str,
    expected: object,
) -> None:
    if not path_value:
        failures.append(f"verified_missing_{kind}_path asset={asset_key}")
        return
    if not isinstance(expected, str) or not SHA256_RE.match(expected):
        failures.append(f"verified_invalid_{kind}_sha256 asset={asset_key}")
        return
    resolved = project_path(path_value)
    if not resolved.is_file():
        failures.append(f"verified_missing_{kind}_file asset={asset_key} path={path_value}")
        return
    observed = sha256(resolved)
    if observed != expected.lower():
        failures.append(f"verified_{kind}_sha256_mismatch asset={asset_key} path={path_value}")


def locator_test(source_build: str, locator: str) -> bool:
    normalized = locator.replace("\\", "/").lower()
    if source_build == "retail":
        return (
            normalized.startswith("${retail_steam}/")
            or normalized.startswith("retail ")
            or normalized.startswith("d:/apbreloaded/content/extracted/")
        )
    if source_build == "2011":
        return "2011" in normalized
    if source_build == "2011+retail":
        return "2011" in normalized or "retail" in normalized
    if source_build == "apbdb":
        return "apbdb" in normalized
    return False


def check_verified_entry(entry: dict) -> list[str]:
    """Mirror the strict gate's checks for one verified row. Returns failures."""
    failures: list[str] = []
    asset_key = str(get_field(entry, ("asset_key", "key")) or "")

    source_locator = str(get_field(entry, ("source_locator", "source_path")) or "")
    source_hash = str(get_field(entry, ("source_sha256", "source_hash")) or "")
    extractor = str(get_field(entry, ("extractor", "extraction_tool")) or "")
    extractor_args = get_field(entry, ("extractor_args", "extraction_args"))
    intermediate_hash = str(get_field(entry, ("intermediate_sha256", "intermediate_hash")) or "")
    conversion = get_field(entry, ("conversion_settings", "conversion"))
    destination = str(get_field(entry, ("destination_path", "destination", "dest")) or "")
    validation = get_field(entry, ("class_validation", "validation", "semantic_validation"))
    intermediate_path = str(get_field(entry, ("intermediate_path", "intermediate")) or "")

    if not source_locator:
        failures.append(f"verified_missing_source_locator asset={asset_key}")
    if not SHA256_RE.match(source_hash):
        failures.append(f"verified_invalid_source_sha256 asset={asset_key}")
    if not extractor:
        failures.append(f"verified_missing_extractor asset={asset_key}")
    if extractor_args is None:
        failures.append(f"verified_missing_extractor_args asset={asset_key}")
    if not SHA256_RE.match(intermediate_hash):
        failures.append(f"verified_invalid_intermediate_sha256 asset={asset_key}")
    if conversion is None:
        failures.append(f"verified_missing_conversion_settings asset={asset_key}")
    if not destination:
        failures.append(f"verified_missing_destination asset={asset_key}")
    if validation is None:
        failures.append(f"verified_missing_class_validation asset={asset_key}")

    if intermediate_path and not intermediate_hash:
        failures.append(f"verified_missing_intermediate_sha256 asset={asset_key}")
    if intermediate_hash and not intermediate_path:
        failures.append(f"verified_missing_intermediate_path asset={asset_key}")
    if intermediate_path:
        validate_file_hash(failures, asset_key, "intermediate", intermediate_path, intermediate_hash)

    evidence_rows = entry.get("d17_evidence") or []
    if not evidence_rows:
        failures.append(f"verified_missing_d17_evidence asset={asset_key}")
        return failures
    if not isinstance(evidence_rows, list):
        failures.append(f"verified_missing_d17_evidence asset={asset_key}")
        return failures

    for evidence in evidence_rows:
        if not isinstance(evidence, dict):
            failures.append(f"verified_invalid_d17_evidence asset={asset_key}")
            continue
        record_key = str(evidence.get("record_key") or "")
        schema = str(evidence.get("schema") or "")
        evidence_path = str(evidence.get("path") or "")
        evidence_hash = str(evidence.get("sha256") or "")
        if not schema:
            failures.append(f"verified_missing_d17_schema asset={asset_key}")
        if not record_key:
            failures.append(f"verified_missing_d17_record_key asset={asset_key}")
        validate_file_hash(failures, asset_key, "d17_evidence", evidence_path, evidence_hash)

        fields = evidence.get("fields")
        if fields is None:
            failures.append(f"verified_missing_d17_fields asset={asset_key} record={record_key}")
            continue
        if not isinstance(fields, dict):
            failures.append(f"verified_missing_d17_fields asset={asset_key} record={record_key}")
            continue

        evidence_source_hash = str(get_field(fields, ("source_sha256", "source_hash")) or "")
        if not SHA256_RE.match(evidence_source_hash):
            failures.append(
                f"verified_invalid_d17_source_sha256 asset={asset_key} record={record_key}"
            )
        elif (
            SHA256_RE.match(source_hash)
            and evidence_source_hash.lower() != source_hash.lower()
        ):
            failures.append(
                f"verified_d17_source_sha256_mismatch asset={asset_key} record={record_key}"
            )

        extracted_path = str(get_field(fields, ("extracted_file", "extracted_path")) or "")
        extracted_hash = str(get_field(fields, ("extracted_sha256", "extracted_hash")) or "")
        if extracted_path:
            validate_file_hash(failures, asset_key, "extracted", extracted_path, extracted_hash)

        output_path = str(get_field(fields, EVIDENCE_OUTPUT_FIELDS) or "")
        output_hash = str(
            get_field(fields, ("output_sha256", "intermediate_sha256", "output_hash")) or ""
        )
        if output_path:
            validate_file_hash(failures, asset_key, "output", output_path, output_hash)

        if intermediate_path and output_path:
            if intermediate_path.replace("\\", "/") != output_path.replace("\\", "/"):
                failures.append(
                    f"verified_intermediate_path_mismatch asset={asset_key} record={record_key}"
                )
        if (
            SHA256_RE.match(intermediate_hash)
            and SHA256_RE.match(output_hash)
            and intermediate_hash.lower() != output_hash.lower()
        ):
            failures.append(
                f"verified_intermediate_sha256_mismatch asset={asset_key} record={record_key}"
            )

    return failures


def entry_is_allowlist_eligible(entry: dict) -> list[str]:
    failures: list[str] = []
    asset_key = str(entry.get("asset_key") or "")
    destination = str(get_field(entry, ("destination_path", "destination", "dest")) or "")
    if not destination.startswith("/Game/"):
        failures.append(f"promotion_invalid_destination asset={asset_key}")
    if not str(entry.get("asset_class") or ""):
        failures.append(f"promotion_missing_asset_class asset={asset_key}")
    source_build = str(entry.get("source_build") or "")
    if source_build not in ALLOWED_SOURCE_BUILDS:
        failures.append(f"promotion_invalid_source_build asset={asset_key} value={source_build}")
    source_locator = str(get_field(entry, ("source_locator", "source_path")) or "")
    if not source_locator:
        failures.append(f"promotion_missing_source_locator asset={asset_key}")
    elif not locator_test(source_build, source_locator):
        failures.append(f"promotion_invalid_source_locator asset={asset_key}")
    return failures


def promote(ledger: dict) -> tuple[dict, dict]:
    result = copy.deepcopy(ledger)
    entries = result.get("entries", [])
    if not isinstance(entries, list):
        raise ValueError("ledger entries is not a list")
    promoted: list[str] = []
    already_verified: list[str] = []
    rejected: list[dict] = []
    now = datetime.datetime.now(datetime.timezone.utc).isoformat()

    for entry in entries:
        if not isinstance(entry, dict) or not entry.get("asset_key"):
            continue
        asset_key = str(entry["asset_key"])
        status = str(entry.get("status") or "")
        if status == "verified":
            already_verified.append(asset_key)
            continue
        failures = check_verified_entry(entry) + entry_is_allowlist_eligible(entry)
        if failures:
            rejected.append({"asset_key": asset_key, "reasons": failures[:8]})
            continue
        entry["status"] = "verified"
        entry["verified_at"] = now
        entry["verified_by"] = VERIFIED_BY
        promoted.append(asset_key)

    report = {
        "schema": "apb_verified_promotion_batch_v1",
        "ledger_entries": len(entries),
        "promoted": sorted(promoted),
        "promoted_count": len(promoted),
        "already_verified_count": len(already_verified),
        "rejected_count": len(rejected),
        "rejected_sample": rejected[:10],
        "verified_by": VERIFIED_BY,
    }
    return result, report


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--ledger", type=Path, default=LEDGER_DEFAULT)
    parser.add_argument("--report", type=Path)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()

    ledger_path = args.ledger.resolve()
    ledger = read_json(ledger_path)
    if not isinstance(ledger, dict):
        raise ValueError("ledger is not an object")
    promoted, report = promote(ledger)
    serialized = (json.dumps(promoted, indent=2) + "\n").encode("utf-8")

    if args.apply:
        if ledger_path.read_bytes() != serialized:
            ledger_path.write_bytes(serialized)
        if args.report:
            args.report.parent.mkdir(parents=True, exist_ok=True)
            report_bytes = (json.dumps(report, indent=2, sort_keys=True) + "\n").encode("utf-8")
            args.report.write_bytes(report_bytes)

    print(
        "VERIFIED_PROMOTION_PASS "
        f"promoted={report['promoted_count']} "
        f"already_verified={report['already_verified_count']} "
        f"rejected={report['rejected_count']} "
        f"apply={str(args.apply).lower()}"
    )
    if not args.apply:
        print("VERIFIED_PROMOTION_DRY_RUN no_files_written=true")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
