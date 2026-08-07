from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from psk import parse_skeleton_file

from vehicles import (
    build_part_catalog,
    build_vehicle_catalog,
    build_wheelspin_clip,
    find_vehicle_textures,
    resolve_vehicle_mesh,
    vehicle_materials,
    vehicle_socket_transforms,
    vehicle_wheel_base,
)


REPO_ROOT = Path(__file__).resolve().parents[4]
VEHICLES = REPO_ROOT / "Content" / "Extracted" / "VehiclesBulk"
VEHICLES_FULL = REPO_ROOT / "Content" / "Extracted" / "Retail" / "Vehicles"


def test_vehicle_catalog_finds_retail_bodies() -> None:
    vehicles = build_vehicle_catalog(VEHICLES)
    assert len(vehicles) >= 40
    taxi = next(vehicle for vehicle in vehicles if vehicle["id"] == "Baked_A_Taxi")
    assert taxi["primary"].endswith("EditorVehicle.psk")
    assert taxi["parts"]


def test_vehicle_mesh_resolution_rejects_traversal() -> None:
    try:
        resolve_vehicle_mesh(VEHICLES, "../../../AGENTS.md")
    except (ValueError, FileNotFoundError):
        return
    raise AssertionError("vehicle path traversal was accepted")


def test_vehicle_textures_resolve_for_taxi() -> None:
    taxi = resolve_vehicle_mesh(VEHICLES, "Baked_A_Taxi/Baked_A_Taxi/SkeletalMesh3/EditorVehicle.psk")
    textures = find_vehicle_textures(taxi)
    assert textures["baseColor"].name == "ExteriorDiffuse.tga"
    assert textures["normal"].name == "ExteriorNormal.tga"


def test_vehicle_materials_keep_interior_glass_and_wheel_slots_separate() -> None:
    taxi = resolve_vehicle_mesh(VEHICLES, "Baked_A_Taxi/Baked_A_Taxi/SkeletalMesh3/EditorVehicle.psk")
    material_textures, settings = vehicle_materials(taxi)
    assert material_textures["ExteriorMat"]["baseColor"].name == "ExteriorDiffuse.tga"
    assert material_textures["InteriorMat"]["baseColor"].name == "InteriorDiffuse.tga"
    assert material_textures["WheelMat"]["baseColor"].name == "WheelDiffuse.tga"
    assert settings["GlassMat"]["alpha_mode"] == "BLEND"


def test_vehicle_sockets_include_four_wheels() -> None:
    taxi = resolve_vehicle_mesh(VEHICLES, "Baked_A_Taxi/Baked_A_Taxi/SkeletalMesh3/EditorVehicle.psk")
    sockets = vehicle_socket_transforms(taxi)
    wheels = [s for s in sockets if s["name"] == "Wheel"]
    assert len(wheels) == 4, f"expected 4 wheel sockets, got {[w['bone'] for w in wheels]}"
    bones = sorted(w["bone"] for w in wheels)
    assert bones == [
        "Bone:WheelDamage_FrontLeft",
        "Bone:WheelDamage_FrontRight",
        "Bone:WheelDamage_RearLeft",
        "Bone:WheelDamage_RearRight",
    ]
    left = next(w for w in wheels if "FrontLeft" in w["bone"])
    right = next(w for w in wheels if "FrontRight" in w["bone"])
    assert left["position"][1] > 0 > right["position"][1], "front wheels straddle the vehicle axis"
    assert abs(left["position"][0] - right["position"][0]) < 0.1, "left/right wheels share the same X"
    assert abs(left["quat"][2]) > 0.99 and abs(left["quat"][3]) < 0.01, "left wheel is flipped"
    assert abs(right["quat"][3]) > 0.99 and abs(right["quat"][2]) < 0.01, "right wheel keeps base orientation"


def test_part_catalog_has_wheel_families_with_variants() -> None:
    catalog = build_part_catalog(VEHICLES_FULL)
    wheels = [c for c in catalog if c["slot"].casefold() == "wheels"]
    assert len(wheels) >= 5, f"expected wheel part families, got {len(wheels)}"
    coupe = next(c for c in wheels if c["family"] == "2DrCoupe")
    assert len(coupe["variants"]) >= 3, "2DrCoupe should have multiple wheel variants"
    variant_mesh = VEHICLES_FULL / coupe["variants"][0]["mesh"]
    assert variant_mesh.is_file(), f"variant mesh missing: {variant_mesh}"
    assert coupe["base"] == "Baked_A_2DrCoupe", "part family must map to its Baked base"


def test_part_catalog_merges_lowercase_reexport_folders_into_their_family() -> None:
    catalog = build_part_catalog(VEHICLES_FULL)
    # Retail carries both V_C_Perf_Wheels_1..12 and lowercase re-export extras
    # v_c_perf_Wheels_13..27 for the same family. Both must land in one entry.
    perf = next(
        c for c in catalog
        if c["base"].casefold() == "baked_c_perf" and c["slot"].casefold() == "wheels"
    )
    assert len(perf["variants"]) >= 13, "lowercase re-export wheels must merge in"
    assert any(v["id"].startswith("V_C_Perf") for v in perf["variants"])
    assert any(v["id"].startswith("v_c_perf") for v in perf["variants"])


def test_vehicle_wheel_base_resolves_special_vehicles() -> None:
    parts = build_part_catalog(VEHICLES_FULL)
    expected = {
        "Baked_A_TruckCement": "Baked_A_TruckCurtain",
        "Baked_A_TruckChristmas": "Baked_A_TruckCurtain",
        "Baked_A_TruckGarbage": "Baked_A_TruckCurtain",
        "Crim_Performance_Aletta": "Baked_C_Perf",
        "Enf_Performance_Gumball": "Baked_E_Perf",
        "Marketing_117_Crim_Vaquero": "Baked_E_Compact",
        "Marketing_117_Enf_V20": "Baked_E_Perf",
        "Marketing_DressToKill_Jericho_Phantom_Crim": "Baked_E_Perf",
        "Marketing_DressToKill_Jericho_Phantom_Enf": "Baked_E_Perf",
    }
    for vehicle_id, base in expected.items():
        assert vehicle_wheel_base(vehicle_id, parts) == base, vehicle_id
    # A vehicle whose ID is itself a part-family base resolves to None.
    assert vehicle_wheel_base("Baked_A_2DrCoupe", parts) is None
    # The resolved base must actually exist in the part catalog.
    bases = {part["base"].casefold() for part in parts}
    for vehicle_id, base in expected.items():
        assert base.casefold() in bases, f"missing base {base} for {vehicle_id}"


def test_part_catalog_uses_canonical_uppercase_casing_for_base() -> None:
    catalog = build_part_catalog(VEHICLES_FULL)
    # The lowercase re-export set must merge into the canonical uppercase
    # family: the emitted base casing is what the frontend keys the Wheel
    # selector by, so it must equal the vehicle ID casing exactly.
    perf = next(
        c for c in catalog
        if c["family"] == "Perf" and c["slot"] == "Wheels" and c["base"] == "Baked_C_Perf"
    )
    assert len(perf["variants"]) >= 13
    assert any(v["id"].startswith("V_C_Perf") for v in perf["variants"]), "uppercase primary must be present"
    assert any(v["id"].startswith("v_c_perf") for v in perf["variants"]), "lowercase extras must merge in"


def test_part_catalog_rejects_meshless_folders() -> None:
    catalog = build_part_catalog(VEHICLES_FULL)
    assert all("mesh" in variant for family in catalog for variant in family["variants"])


def test_part_textures_are_resolved_from_the_part_package() -> None:
    wheel = resolve_vehicle_mesh(
        VEHICLES_FULL,
        "V_A_2DrCoupe_Wheels_1/SkeletalMesh3/PartMesh.psk",
    )
    textures = find_vehicle_textures(wheel)
    assert textures["baseColor"].name == "Diffuse.tga"
    assert textures["normal"].name == "Normal.tga"
    assert textures["emissive"].name == "Emissive.tga"


def test_wheelspin_clip_targets_only_real_wheel_bones_forward_direction() -> None:
    coupe = resolve_vehicle_mesh(
        VEHICLES,
        "Baked_A_2DrCoupe/Baked_A_2DrCoupe/SkeletalMesh3/EditorVehicle.psk",
    )
    skeleton = parse_skeleton_file(coupe)
    clip = build_wheelspin_clip(skeleton)
    spinning = {skeleton[i].name for i, track in enumerate(clip["tracks"]) if track}

    # Real wheels (Damage/Main/HubCap) spin; the steering column and calipers
    # never do. The naive "Wheel in name" match also caught Bone:SteeringWheel,
    # which made the wheel swing sideways under the clip.
    assert spinning, "expected at least one spinning wheel bone"
    assert all("WheelDamage" in n or "WheelMain" in n or "WheelHubCap" in n for n in spinning)
    assert any("WheelDamage" in n for n in spinning), "tire geometry bones must spin"
    assert not any("Steering" in n or "Caliper" in n for n in spinning)
    assert len(spinning) >= 8, "both axles, both sides expected"

    # One full revolution per clip loop, spinning about the local +Y axle.
    track = next(track for track in clip["tracks"] if track)
    assert len(track) == 60
    first, mid, last = track[0], track[30], track[-1]
    assert abs(first.quat[3] - 1.0) < 1e-4, "starts at identity"
    assert mid.quat[1] > 0.9 and abs(mid.quat[3]) < 1e-4, "half turn about +Y"
    assert abs(last.quat[3] + 1.0) < 1e-2, "returns near identity (loop closure)"
