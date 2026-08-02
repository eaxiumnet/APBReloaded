#!/usr/bin/env python3
"""Negative-fixture matrix for tools/check_static_cook_assets.ps1.

Mirrors the task-21 negative controls at the path-closure layer of the cook
audit: malformed allowlist (missing file, broken JSON, missing entries,
missing object_path, wrong class) and engine-reference policy (blocked refs
fail, allowed engine internals pass). Every gate must prove one positive and
one negative control, so the matrix also pins the happy path.

Each cell runs the real PowerShell gate against a disposable fixture tree
(Content/Data catalog + allowlist) and asserts the named marker + exit code.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
GATE = ROOT / "tools" / "check_static_cook_assets.ps1"

CATALOG = [
    "/Game/Imported/UI/Menu2011/Login/Constant_BG.Constant_BG",
    "/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha",
    "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
    "/Engine/BasicShapes/Cube.Cube",
]

# Paths the positive fixture allowlist covers (dotted + dotless forms).
POSITIVE_ENTRIES = [
    {
        "asset_key": "fixture:Constant_BG",
        "object_path": "/Game/Imported/UI/Menu2011/Login/Constant_BG.Constant_BG",
        "class": "Texture2D",
        "source_build": "2011",
        "source_locator": "Content/Extracted/2011/fixture",
    },
    {
        "asset_key": "fixture:LaRocha",
        "object_path": "/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha",
        "class": "StaticMesh",
        "source_build": "retail",
        "source_locator": "${retail_steam}/APBGame/Content/Release/Packages/fixture.upk",
    },
]


def write_json(path: Path, document: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(document, indent=2) + "\n", encoding="utf-8")


def run_audit(fixture_root: Path, allowlist: Path, scratch: Path) -> subprocess.CompletedProcess:
    output = scratch / "cook_audit.json"
    return subprocess.run(
        [
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            str(GATE),
            "-ProjectRoot",
            str(fixture_root),
            "-AllowlistPath",
            str(allowlist),
            "-Output",
            str(output),
        ],
        cwd=ROOT,
        capture_output=True,
        text=True,
        check=False,
    )


def make_catalog(fixture_root: Path, references: list[str]) -> Path:
    catalog = fixture_root / "Content" / "Data" / "fixture_catalog.json"
    write_json(catalog, {"fixture_references": references})
    return catalog


def make_allowlist(path: Path, entries: list[dict], media: list[dict] | None = None) -> Path:
    write_json(
        path,
        {
            "schema_version": 2,
            "generated_by": "fixture",
            "entries": entries,
            "media_entries": media or [],
        },
    )
    return path


class CookAuditFixtureMatrix(unittest.TestCase):
    def setUp(self) -> None:
        self._temp = tempfile.TemporaryDirectory()
        self.root = Path(self._temp.name)
        self.scratch = self.root / "scratch"
        self.scratch.mkdir()

    def tearDown(self) -> None:
        self._temp.cleanup()

    def assert_marker(self, result: subprocess.CompletedProcess, marker: str) -> None:
        combined = result.stdout + result.stderr
        self.assertIn(marker, combined, combined)

    # --- Negative controls -------------------------------------------------

    def test_missing_allowlist_fails_named(self) -> None:
        make_catalog(self.root, CATALOG)
        result = run_audit(self.root, self.root / "nope.json", self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "reason=missing_allowlist")

    def test_malformed_json_fails_named(self) -> None:
        make_catalog(self.root, CATALOG)
        broken = self.root / "broken.json"
        broken.write_text('{"entries": [ {', encoding="utf-8")
        result = run_audit(self.root, broken, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "reason=malformed_allowlist")

    def test_missing_entries_field_fails_named(self) -> None:
        make_catalog(self.root, CATALOG)
        allowlist = self.root / "no_entries.json"
        allowlist.write_text('{"schema_version": 2, "media_entries": []}', encoding="utf-8")
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "detail=entries_missing")

    def test_empty_entries_fails_unverified(self) -> None:
        # Schema-valid but empty: every /Game/ ref is unverified. This is the
        # QA orchestrator's negative fixture shape (task 20/21 allowlist bypass).
        make_catalog(self.root, CATALOG)
        allowlist = make_allowlist(self.root / "empty.json", [])
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "STATIC_COOK_ASSET_AUDIT_FAIL")
        self.assert_marker(result, "unverified=")

    def test_missing_object_path_fails_named(self) -> None:
        make_catalog(self.root, CATALOG)
        entries = [{"asset_key": "fixture:bad", "class": "StaticMesh", "source_build": "retail"}]
        allowlist = make_allowlist(self.root / "no_obj.json", entries)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "detail=missing_object_path")

    def test_unsupported_class_fails_named(self) -> None:
        make_catalog(self.root, CATALOG)
        entries = [
            {
                "asset_key": "fixture:bad",
                "object_path": "/Game/Imported/UI/Menu2011/Login/Constant_BG.Constant_BG",
                "class": "Texture2DMap",
                "source_build": "2011",
            }
        ]
        allowlist = make_allowlist(self.root / "bad_class.json", entries)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "reason=unsupported_class")
        self.assert_marker(result, "class=Texture2DMap")

    def test_supported_class_mismatch_not_enforced_by_path_closure(self) -> None:
        # Documented limitation: path-only closure cannot catch a supported-but-
        # wrong class (SoundWave on a mesh path passes the schema check), so the
        # gate fails via the uncovered ref instead. Runtime probes own that check.
        make_catalog(self.root, CATALOG)
        entries = [
            {
                "asset_key": "fixture:la",
                "object_path": "/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha",
                "class": "SoundWave",
                "source_build": "retail",
            }
        ]
        allowlist = make_allowlist(self.root / "wrong_class.json", entries)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "STATIC_COOK_ASSET_AUDIT_FAIL")
        self.assert_marker(result, "unverified=")
        self.assertNotIn("reason=unsupported_class", result.stdout + result.stderr)

    def test_engine_blocked_reference_fails(self) -> None:
        make_catalog(self.root, CATALOG)
        allowlist = make_allowlist(self.root / "engine_blocked.json", POSITIVE_ENTRIES)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 1)
        self.assert_marker(result, "STATIC_COOK_ASSET_AUDIT_FAIL")
        self.assert_marker(result, "engine_blocked=1")
        report = json.loads((self.scratch / "cook_audit.json").read_text(encoding="utf-8"))
        self.assertEqual(report["engine_blocked_references"], 1)
        self.assertTrue(any("Engine/BasicShapes/Cube.Cube" in row for row in report["engine_blocked_sample"]))

    # --- Positive controls -------------------------------------------------

    def test_full_positive_control_passes(self) -> None:
        catalog_refs = [
            "/Game/Imported/UI/Menu2011/Login/Constant_BG.Constant_BG",
            "/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha.m_contact_enforcement_larocha",
            "/Engine/EngineMaterials/DefaultMaterial.DefaultMaterial",
        ]
        make_catalog(self.root, catalog_refs)
        allowlist = make_allowlist(self.root / "ok.json", POSITIVE_ENTRIES)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_marker(result, "STATIC_COOK_ASSET_AUDIT_PASS")
        report = json.loads((self.scratch / "cook_audit.json").read_text(encoding="utf-8"))
        self.assertEqual(report["unverified_references"], 0)
        self.assertEqual(report["engine_blocked_references"], 0)
        self.assertEqual(report["allowlisted_references"], 2)

    def test_dotless_catalog_reference_closes_via_leaf_strip(self) -> None:
        # Catalogs may reference the package path without the .<leaf> suffix;
        # the leaf-stripped index must close it.
        make_catalog(self.root, ["/Game/Imported/Characters/Contact_LaRocha/m_contact_enforcement_larocha"])
        allowlist = make_allowlist(self.root / "dotless.json", POSITIVE_ENTRIES)
        result = run_audit(self.root, allowlist, self.scratch)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assert_marker(result, "STATIC_COOK_ASSET_AUDIT_PASS")


if __name__ == "__main__":
    unittest.main(verbosity=2)
