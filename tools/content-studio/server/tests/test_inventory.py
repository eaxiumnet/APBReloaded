from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from inventory import build_asset_inventory, find_prop_mesh_dir, pick_prop_mesh


def test_inventory_tracks_ledger_and_untracked_uassets(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    (tmp_path / "Content" / "Imported" / "Vehicles").mkdir(parents=True)
    ledger = {
        "entries": [
            {
                "asset_key": "vehicle/Baked_A_Saloon",
                "dest": "/Game/Imported/Vehicles/Baked_A_Saloon",
                "status": "verified",
                "source_build": "retail",
                "source_locator": "Vehicles/Baked_A_Saloon",
                "asset_class": "SkeletalMesh",
                "consumer_domain": "vehicle",
            },
            {
                "asset_key": "anim/Idle",
                "dest": "/Game/Imported/Characters/Animations/Idle",
                "status": "blocked_source",
                "source_build": "retail",
                "source_locator": "Characters/Idle",
                "asset_class": "AnimSequence",
                "consumer_domain": "character",
            },
        ]
    }
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps(ledger), encoding="utf-8")
    (tmp_path / "Content" / "Imported" / "Vehicles" / "Loose.uasset").write_bytes(b"uasset")

    result = build_asset_inventory(tmp_path)
    assert result["total"] == 3
    assert result["categories"]["vehicles"] == 2
    assert result["categories"]["animation"] == 1
    assert result["statuses"]["blocked_source"] == 1
    loose = next(item for item in result["assets"] if item["name"] == "Loose")
    assert loose["status"] == "imported_untracked"
    assert loose["provenance"] == "untracked"
    assert loose["preview_kind"] == "none"


def test_group_destination_covers_child_uasset(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    imported = tmp_path / "Content" / "Imported" / "Vehicles" / "Baked_A_Saloon"
    imported.mkdir(parents=True)
    (imported / "EditorVehicle.uasset").write_bytes(b"uasset")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "group:vehicle/Baked_A_Saloon",
            "dest": "/Game/Imported/Vehicles/Baked_A_Saloon",
            "status": "imported",
            "source_build": "retail",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    assert result["total"] == 1
    assert result["assets"][0]["provenance"] == "incomplete"


def test_inventory_categories_are_stable(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    payload = {
        "entries": [
            {
                "asset_key": "Weapon_AssaultRifle",
                "dest": "/Game/Imported/Weapons/Weapon_AssaultRifle",
                "status": "imported",
                "source_build": "retail",
            },
            {
                "asset_key": "Social_Block",
                "dest": "/Game/Imported/Districts/Social",
                "status": "bound",
                "source_build": "retail",
            },
        ]
    }
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps(payload), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    assert [asset["category"] for asset in result["assets"]] == ["maps", "weapons"]
    assert result["source_builds"] == ["retail"]


def test_weapon_rows_receive_existing_mesh_preview_hint(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    (tmp_path / "Content" / "Extracted" / "WeaponsBase" / "Weapon_AssaultRifle").mkdir(parents=True)
    (tmp_path / "Content" / "Extracted" / "WeaponsBase" / "Weapon_AssaultRifle" / "Weapon_AssaultRifle_LOD0.psk").write_bytes(b"psk")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "weapon/Weapon_AssaultRifle",
            "dest": "/Game/Imported/Weapons/Weapon_AssaultRifle",
            "status": "imported",
            "source_build": "retail",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["preview_kind"] == "weapon_mesh"
    assert asset["preview_path"].endswith(".psk")


def test_vehicle_row_normalizes_upk_name_for_preview(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    vehicle = tmp_path / "Content" / "Extracted" / "VehiclesBulk" / "Baked_A_2DrCoupe"
    (vehicle / "Baked_A_2DrCoupe" / "SkeletalMesh3").mkdir(parents=True)
    (vehicle / "Baked_A_2DrCoupe" / "SkeletalMesh3" / "EditorVehicle.psk").write_bytes(b"psk")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "Baked_A_2DrCoupe.upk#EditorVehicle",
            "dest": "/Game/Imported/Vehicles/Baked_A_2DrCoupe",
            "status": "imported",
            "source_build": "retail",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "vehicles"
    assert asset["preview_kind"] == "vehicle_mesh"
    assert asset["preview_path"].endswith("EditorVehicle.psk")


def test_material_texture_preview_resolves(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    tex = tmp_path / "Content" / "Extracted" / "MaterialDatabase" / "Aerials" / "Aerials" / "Texture2D" / "Aerials_DiffSpec.tga"
    tex.parent.mkdir(parents=True)
    tex.write_bytes(b"tga")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "Aerials.upk#Aerials_DiffSpec",
            "dest": "/Game/Imported/Materials/Aerials",
            "status": "imported",
            "source_build": "retail",
            "source_locator": "${retail_steam}/APBGame/Content/Release/Packages/MaterialDatabase/PropMaterials/Aerials.upk",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "materials"
    assert asset["preview_kind"] == "texture"
    assert asset["preview_path"].endswith("Aerials_DiffSpec.tga")


def test_character_row_preview_resolves(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    item = tmp_path / "Content" / "Extracted" / "CharactersBulk" / "F_Body_Base"
    (item / "F_Body_Base" / "SkeletalMesh3").mkdir(parents=True)
    (item / "F_Body_Base" / "SkeletalMesh3" / "F_Body_Base.psk").write_bytes(b"psk")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "F_Body_Base.upk#F_Body_Base",
            "dest": "/Game/Imported/Characters/F_Body_Base",
            "status": "imported",
            "source_build": "retail",
            "source_locator": "${retail_steam}/APBGame/Content/Release/Packages/Character/Bodies/F_Body_Base.upk",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "characters"
    assert asset["preview_kind"] == "character_mesh"
    assert asset["preview_path"] == "F_Body_Base"


def test_district_static_meshes_are_scanned_lod_deduped(tmp_path: Path) -> None:
    district = tmp_path / "Content" / "Extracted" / "Retail" / "Districts" / "AsylumMain_Package" / "StaticMesh3"
    district.mkdir(parents=True)
    (district / "AsylumMain_Generic_0001_LOD_0.pskx").write_bytes(b"pskx")
    (district / "AsylumMain_Generic_0001_LOD_1.pskx").write_bytes(b"pskx")
    (district / "AsylumMain_Generic_0002_LOD_0.pskx").write_bytes(b"pskx")
    (district / "Loose_Part.psk").write_bytes(b"psk")

    result = build_asset_inventory(tmp_path)
    meshes = [asset for asset in result["assets"] if asset["preview_kind"] == "static_mesh"]
    assert len(meshes) == 3
    assert all(asset["category"] == "maps" for asset in meshes)
    assert all("LOD_1" not in asset["name"] for asset in meshes)
    lod0 = next(asset for asset in meshes if asset["name"] == "AsylumMain_Generic_0001_LOD_0")
    assert lod0["preview_path"] == "AsylumMain_Package/StaticMesh3/AsylumMain_Generic_0001_LOD_0.pskx"
    assert lod0["asset_class"] == "StaticMesh"


def test_prop_animset_row_resolves(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    package = tmp_path / "Content" / "Extracted" / "MaterialDatabase" / "BreakableDoors" / "BreakableDoors"
    (package / "AnimSet").mkdir(parents=True)
    (package / "SkeletalMesh3").mkdir()
    (package / "AnimSet" / "ActiveDoor_Animset.psa").write_bytes(b"psa")
    (package / "SkeletalMesh3" / "BreakableDoor.psk").write_bytes(b"psk")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "BreakableDoors.upk#ActiveDoor_Animset",
            "dest": "/Game/Imported/MaterialDatabase/BreakableDoors",
            "status": "imported",
            "source_build": "retail",
            "source_locator": "${retail_steam}/APBGame/Content/Release/Packages/MaterialDatabase/MidtownDistrict/PropMaterials/BreakableDoors.upk",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "animation"
    assert asset["preview_kind"] == "prop_animation"
    assert asset["preview_path"].endswith("AnimSet/ActiveDoor_Animset.psa")


def test_prop_anim_index_skips_packages_without_mesh_sibling(tmp_path: Path) -> None:
    # A package whose AnimSet has no SkeletalMesh3 sibling must not be indexed:
    # the endpoint would 404 on every preview.
    (tmp_path / "tools").mkdir()
    solo = tmp_path / "Content" / "Extracted" / "MaterialDatabase" / "SoloAnim" / "SoloAnim" / "AnimSet"
    solo.mkdir(parents=True)
    (solo / "Loose.psa").write_bytes(b"psa")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "SoloAnim.upk#Loose",
            "dest": "/Game/Imported/MaterialDatabase/SoloAnim",
            "status": "imported",
            "source_build": "retail",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    assert result["assets"][0]["preview_kind"] == "none"


def test_find_prop_mesh_dir_walks_up_from_any_depth(tmp_path: Path) -> None:
    psa = tmp_path / "Pkg" / "Nested" / "Deeper" / "AnimSet" / "Door.psa"
    mesh_dir = tmp_path / "Pkg" / "Nested" / "SkeletalMesh3"
    psa.parent.mkdir(parents=True)
    mesh_dir.mkdir(parents=True)
    (mesh_dir / "Door.psk").write_bytes(b"psk")
    assert find_prop_mesh_dir(psa) == mesh_dir


def test_pick_prop_mesh_prefers_package_named_stem(tmp_path: Path) -> None:
    mesh_dir = tmp_path / "SkeletalMesh3"
    mesh_dir.mkdir(parents=True)
    small = mesh_dir / "BreakableDoor.psk"
    big = mesh_dir / "BreakableDoor_LOD_0.psk"
    small.write_bytes(b"x" * 10)
    big.write_bytes(b"x" * 1000)
    # The package dir name (the hint) overlaps the base mesh stem, so the
    # deterministic winner is the hint match, not the largest file.
    psa = tmp_path / "BreakableDoors" / "BreakableDoors" / "AnimSet" / "ActiveDoor_Animset.psa"
    psa.parent.mkdir(parents=True)
    psa.write_bytes(b"psa")
    assert pick_prop_mesh(mesh_dir, psa) == small


def test_pick_prop_mesh_falls_back_to_largest_without_hint(tmp_path: Path) -> None:
    mesh_dir = tmp_path / "SkeletalMesh3"
    mesh_dir.mkdir(parents=True)
    (mesh_dir / "AA_Alpha.psk").write_bytes(b"x" * 50)
    (mesh_dir / "ZZ_Beta.psk").write_bytes(b"x" * 9000)
    psa = tmp_path / "Pkg" / "Pkg" / "AnimSet" / "Unrelated.psa"
    psa.parent.mkdir(parents=True)
    psa.write_bytes(b"psa")
    assert pick_prop_mesh(mesh_dir, psa).name == "ZZ_Beta.psk"


def test_media_row_preview_resolves_to_playable_video(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    video = tmp_path / "Content" / "Extracted" / "2011" / "LoginAnimatedBackground_ai_upscale" / "05_Login_BG_AI_compat.mp4"
    video.parent.mkdir(parents=True)
    video.write_bytes(b"mp4")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "05_Login_BG_AI_compat.mp4",
            "dest": "/Game/Imported/UI/Movies/05_Login_BG_AI_compat.mp4",
            "status": "imported",
            "source_build": "2011",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "media"
    assert asset["preview_kind"] == "video"
    assert asset["preview_path"].endswith("05_Login_BG_AI_compat.mp4")
