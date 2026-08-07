"""Unit tests for the static bake (bake_static.py)."""

from __future__ import annotations

from bake_static import _rewrite_inventory_previews, slug


def test_slug_keeps_safe_characters():
    assert slug("Weapon_Armas_Magnum/Weapon_Armas_Magnum/SkeletalMesh3/Crm_Magnum_Mk3_LOD0.psk") == (
        "Weapon_Armas_Magnum_Weapon_Armas_Magnum_SkeletalMesh3_Crm_Magnum_Mk3_LOD0.psk"
    )


def test_slug_replaces_every_unsafe_character():
    assert slug('a b:c?d"e') == "a_b_c_d_e"
    assert slug("") == ""
    assert slug("A-Z.a_z0-9") == "A-Z.a_z0-9"


def test_slug_unicode_and_odd_whitespace():
    # A single non-ASCII char maps to a single '_' (matches the TS shim);
    # the space also becomes '_'.
    assert slug("Foo\xe9 Bar") == "Foo__Bar"


def test_inventory_previews_keep_baked_and_drop_rest():
    inventory = {
        "assets": [
            {"preview_kind": "weapon_mesh", "preview_path": "A.psk"},
            {"preview_kind": "weapon_mesh", "preview_path": "B.psk"},
            {"preview_kind": "vehicle_mesh", "preview_path": "V.psk"},
            {"preview_kind": "character_mesh", "preview_path": "Shirt"},
            {"preview_kind": "static_mesh", "preview_path": "Building.psk"},
            {"preview_kind": "prop_animation", "preview_path": "Door.psa"},
            {"preview_kind": "texture", "preview_path": "x.tga"},
            {"preview_kind": "video", "preview_path": "y.webm"},
            {"preview_kind": "none", "preview_path": None},
        ]
    }
    baked = {
        "weapon_parts": {"A.psk"},
        "vehicle_primaries": {"V.psk"},
        "clothing": {"Shirt"},
        "districts": {"Building.psk"},
        "props": {"Door.psa"},
    }
    _rewrite_inventory_previews(inventory, baked)
    kept = {
        (asset["preview_kind"], asset["preview_path"])
        for asset in inventory["assets"]
        if asset["preview_kind"] != "none"
    }
    assert kept == {
        ("weapon_mesh", "A.psk"),
        ("vehicle_mesh", "V.psk"),
        ("character_mesh", "Shirt"),
        ("static_mesh", "Building.psk"),
        ("prop_animation", "Door.psa"),
    }


def test_inventory_previews_unbaked_kinds_become_none():
    inventory = {
        "assets": [
            {"preview_kind": "weapon_mesh", "preview_path": "Missing.psk"},
            {"preview_kind": "texture", "preview_path": "tex.tga"},
            {"preview_kind": "video", "preview_path": "clip.webm"},
        ]
    }
    baked = {
        "weapon_parts": set(),
        "vehicle_primaries": set(),
        "clothing": set(),
        "districts": set(),
        "props": set(),
    }
    _rewrite_inventory_previews(inventory, baked)
    assert all(asset["preview_kind"] == "none" and asset["preview_path"] is None for asset in inventory["assets"])
