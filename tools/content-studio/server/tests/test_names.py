from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from names import default_resolver  # noqa: E402

# Curated disk-folder -> authoritative apbdb display name. The folder->weapon
# pairing is confirmed by viewing the rendered mesh (disk names are not apbdb
# ids); the display string on the right is authoritative in weapons_catalog.json.
# Each MUST resolve with confidence "alias".
ALIAS_EXPECTED: dict[str, str] = {
    "Weapon_Armas_AgroSniper": "NCR-762",
    "Weapon_Armas_LightMachineGun_762": "ALIG 762",
    "Weapon_Armas_Magnum": "ACT 44 GM PR1",
    "Weapon_Armas_Shotgun": "Colby CSG-20",
    "Weapon_AssaultRifle": "N-TEC 5",
    "Weapon_AutomaticShotgun": "NFAS-12",
    "Weapon_BurstFirePistol": "Joker RFP-9",
    "Weapon_BurstFireRifle": "OBIR",
    "Weapon_Carbine": "Joker SR15 Carbine",
    "Weapon_ColbyClassic": "Colby Classic",
    "Weapon_CrowdControlGun": "Stabba - CCG",
    "Weapon_DartGun": "Stabba - TG-8",
    "Weapon_EightBall": "Eight-Ball",
    "Weapon_Explosive_AAPED": "AAEPD",
    "Weapon_FragmentationGrenade": "Frag Grenade",
    "Weapon_HouseBrick": "Half-Brick",
    "Weapon_LightMachineGun_556": "SHAW 556",
    "Weapon_LightMachineGun_762": "ALIG 762",
    "Weapon_LightMachineGun_SSW": "N-SSW 74",
    "Weapon_MachinePistol": "OCA Nano",
    "Weapon_MachinePistol_PDW": "S-AS PDW",
    "Weapon_MobilePhone": "SPiN Phone",
    "Weapon_ObeyaBattleRifle": "Obeya CR762",
    "Weapon_RocketLauncher": "OSMAW",
    "Weapon_Secondary_Fr0g": "FR0G",
    "Weapon_Secondary_OUL": "UL-3 'Jersey Devil'",
    "Weapon_SemiAutoPistol": "Colby .45 AP",
    "Weapon_Shotgun": "JG-840",
    "Weapon_SMG_Vector": "CBMP-45 'Dart'",
    "Weapon_SniperRifle_50Cal": "Agrotech DMR-SD",
    "Weapon_SniperRifle_PSR": "PSR",
    "Weapon_SnowBall": "Snowball",
    "Weapon_SnubNoseRevolver": "Colby SNR 850",
    "Weapon_StarterSMG": "OCA-EW 626",
    "Weapon_StunGun": "Stabba - PIG",
}


@pytest.mark.parametrize("folder,expected", sorted(ALIAS_EXPECTED.items()))
def test_alias_resolves_to_authoritative_name(folder: str, expected: str) -> None:
    resolved = default_resolver().resolve(folder, folder)
    assert resolved.display == expected, f"{folder}: {resolved.display!r} != {expected!r}"
    assert resolved.confidence == "alias", f"{folder}: confidence {resolved.confidence}"


def test_cut_weapon_stays_derived_never_mislabelled() -> None:
    # Not present in apbdb (cut/prototype) -> must NOT be force-mapped.
    resolved = default_resolver().resolve("Weapon_TacticalAssaultRifle", "Weapon_TacticalAssaultRifle_LOD0")
    assert resolved.confidence == "derived", resolved


def test_ntec7_resolves_to_curated_family() -> None:
    resolved = default_resolver().resolve("Weapon_AssaultRifle_NTEC-7", "Weapon_AssaultRifle_NTEC-7_Standard_LOD0")
    assert resolved.display == "N-TEC 7", resolved
    assert resolved.confidence == "curated", resolved


def test_existing_exact_match_unchanged() -> None:
    # ATac resolves via the exact tier; aliases must not disturb it.
    resolved = default_resolver().resolve("Weapon_AssaultRifle_ATac", "Weapon_AssaultRifle_ATac_Default_LOD0")
    assert resolved.display == "ATAC 424"
    assert resolved.confidence in ("exact", "alias", "catalog")


def test_genuine_hvr762_keeps_name_without_alias() -> None:
    resolved = default_resolver().resolve("Weapon_SniperRifle_HVR762", "Weapon_SniperRifle_HVR762_LOD0")
    assert resolved.display == "N-HVR 762"
    assert resolved.confidence == "exact"


@pytest.mark.parametrize(
    "folder",
    ["Weapon_SniperRifle_50Cal", "Weapon_Armas_SniperRifle_50Cal"],
)
def test_50cal_folders_no_longer_mislabelled_as_hvr762(folder: str) -> None:
    # Both were wrongly aliased to "N-HVR 762" (3-way collision with genuine HVR762).
    resolved = default_resolver().resolve(folder, folder)
    assert resolved.display != "N-HVR 762", resolved
