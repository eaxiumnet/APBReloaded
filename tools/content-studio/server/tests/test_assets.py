"""Tests for asset discovery + safe path resolution against real WeaponsBase."""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from _alias_data import PRIMARY_MESH  # noqa: E402
from assets import build_weapon_catalog, resolve_mesh_path  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[4]
WEAPONS_BASE = REPO_ROOT / "Content" / "Extracted" / "WeaponsBase"


def test_catalog_finds_weapons() -> None:
    weapons = build_weapon_catalog(WEAPONS_BASE)
    assert len(weapons) >= 80, f"only {len(weapons)} weapons found"
    magnum = next((w for w in weapons if w["id"] == "Weapon_Armas_Magnum"), None)
    assert magnum is not None, "Magnum design folder missing from catalog"
    assert "Magnum_Mk3" in magnum["primary"], magnum["primary"]
    assert any("Clip" in p["name"] for p in magnum["parts"]), "clip part missing"


def test_catalog_entries_have_display_fields() -> None:
    weapons = build_weapon_catalog(WEAPONS_BASE)
    for w in weapons:
        assert w["display"] and w["display"].strip(), f"{w['id']} has empty display"
        assert "_" not in w["display"], f"raw name leaked: {w['display']}"
        assert w["name_confidence"] in {"curated", "alias", "exact", "catalog", "derived"}, w["name_confidence"]
    atac = next(w for w in weapons if w["id"] == "Weapon_AssaultRifle_ATac")
    assert atac["display"] == "ATAC 424", atac["display"]
    assert atac["name_confidence"] == "exact"


def test_every_entry_has_parts() -> None:
    for w in build_weapon_catalog(WEAPONS_BASE):
        assert w["parts"], f"{w['id']} has no parts"
        assert w["primary"] in {p["id"] for p in w["parts"]}, f"{w['id']} primary not in parts"


def test_primary_is_largest_part() -> None:
    for w in build_weapon_catalog(WEAPONS_BASE):
        override = PRIMARY_MESH.get(w["folder"])
        if override:
            pool = [p for p in w["parts"] if override.casefold() in p["name"].casefold()]
            expected = max(pool, key=lambda p: p["bytes"])["id"]
            assert w["primary"] == expected, f"{w['id']}: primary != largest '{override}' part"
        else:
            largest = max(w["parts"], key=lambda p: p["bytes"])["id"]
            assert w["primary"] == largest, f"{w['id']}: primary != largest"


def test_resolve_valid_mesh() -> None:
    weapons = build_weapon_catalog(WEAPONS_BASE)
    magnum = next(w for w in weapons if w["id"] == "Weapon_Armas_Magnum")
    resolved = resolve_mesh_path(WEAPONS_BASE, magnum["primary"])
    assert resolved.is_file()
    assert resolved.suffix.lower() in {".psk", ".pskx"}


def test_resolve_rejects_traversal() -> None:
    for bad in ["../../../AGENTS.md", "..\\..\\secret", "/etc/passwd"]:
        try:
            resolve_mesh_path(WEAPONS_BASE, bad)
            raised = False
        except (ValueError, FileNotFoundError):
            raised = True
        assert raised, f"traversal not rejected: {bad}"


def test_resolve_rejects_non_mesh() -> None:
    try:
        resolve_mesh_path(WEAPONS_BASE, "Weapon_Armas_Magnum")  # a dir, not a mesh
        raised = False
    except (ValueError, FileNotFoundError):
        raised = True
    assert raised, "non-mesh path not rejected"


# --- standalone runner (repo FAILS=N convention; no pytest required) ---
if __name__ == "__main__":
    fails = 0
    for name, fn in sorted(globals().items()):
        if name.startswith("test_") and callable(fn):
            try:
                fn()
                print(f"PASS: {name}")
            except Exception as exc:  # noqa: BLE001
                fails += 1
                print(f"FAIL: {name}: {exc}")
    print(f"FAILS={fails}")
    sys.exit(1 if fails else 0)
