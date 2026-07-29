"""RED/GREEN contract for the curated family-override tier.

Locks apbdb-authoritative BASE FAMILY display names against the retail
disk-folder namespace. Every expected value is apbdb-verified; see the source
annotations on ``_alias_data.FAMILY_DISPLAY``.
"""

from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from names import default_resolver  # noqa: E402

FAMILY_EXPECTED: dict[str, str] = {
    "Weapon_LMG_AMG": "AMG-556",
    "Weapon_LMG_CAS": "CASE",
    "Weapon_SMG_C9S": "C9",
    "Weapon_SMG_Norsemen": "Norseman",
    "Weapon_AssaultRifle_FAR": "FAR",
    "Weapon_SMG_CBMP": "CBMP-45",
    "Weapon_CAP40": "Obeya CAP40",
    "Weapon_AssaultRifle_Raptor": "Raptor 45",
    "Weapon_SniperRifle_SBSR": "SBSR",
    "Weapon_Rifle_FFA": "FFA 5.56",
    "Weapon_Secondary_OCSP": "OCSP",
    "Weapon_Secondary_Mountie": "Mountie",
    "Weapon_Shotgun_Shredder": "Shredder",
    "Weapon_AssaultRifle_Fanatic": "S1-FA",
    "Weapon_GrenadeLauncher": "O-PGL 79",
    "Weapon_Vas": "VAS SW2",
    "Weapon_AssaultRifle_VAS": "VAS R-2",
    "Weapon_AssaultRifle_NTEC-7": "N-TEC 7",
    "Weapon_Shotgun_DOW": "DOW",
    "Weapon_AssaultRifle_Apocalypse": "AR-97 'Misery'",
    "Weapon_Colby_Commander": "Colby Commander",
    "Weapon_Rifle_SACR": "STAC 10",
    "Weapon_SniperRifle_Vesper": "Vesper",
}


@pytest.mark.parametrize("folder,expected", sorted(FAMILY_EXPECTED.items()))
def test_family_display_resolves_to_clean_family(folder: str, expected: str) -> None:
    resolved = default_resolver().resolve(folder, folder)
    assert resolved.display == expected, f"{folder}: {resolved.display!r} != {expected!r}"
    assert resolved.confidence == "curated", f"{folder}: confidence={resolved.confidence!r}"
    assert resolved.sapbdb is None, f"{folder}: sapbdb should be None, got {resolved.sapbdb!r}"


REGRESSION_EXPECTED: dict[str, tuple[str, str]] = {
    "Weapon_AssaultRifle": ("N-TEC 5", "alias"),
    "Weapon_Shotgun": ("JG-840", "alias"),
    "Weapon_SniperRifle_HVR762": ("N-HVR 762", "exact"),
    "Weapon_SMG_ACES": ("Agrotech ACES", "exact"),
    "Weapon_ConcussionGrenade": ("Concussion Grenade", "exact"),
}


@pytest.mark.parametrize("folder,expected", sorted(REGRESSION_EXPECTED.items()))
def test_existing_tiers_unchanged(folder: str, expected: tuple[str, str]) -> None:
    disp, conf = expected
    resolved = default_resolver().resolve(folder, folder)
    assert resolved.display == disp, f"{folder}: {resolved.display!r} != {disp!r}"
    assert resolved.confidence == conf, f"{folder}: {resolved.confidence!r} != {conf!r}"


def test_family_display_beats_lower_tiers() -> None:
    resolved = default_resolver().resolve(
        "Weapon_AssaultRifle_NTEC-7", "Weapon_AssaultRifle_NTEC-7_Standard_LOD0"
    )
    assert resolved.display == "N-TEC 7", resolved
    assert resolved.confidence == "curated", resolved
