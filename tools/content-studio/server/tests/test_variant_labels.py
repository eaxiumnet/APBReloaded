from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from assets import strip_part_label  # noqa: E402


@pytest.mark.parametrize(
    "stem,folder,expected",
    [
        ("Weapon_AssaultRifle_ATac_Bodyguard_LOD0", "Weapon_AssaultRifle_ATac", "Bodyguard"),
        ("Weapon_AssaultRifle_ATac_Default_LOD0", "Weapon_AssaultRifle_ATac", "Default"),
        ("Weapon_AssaultRifle_Apocalypse_Death_LOD0", "Weapon_AssaultRifle_Apocalypse", "Death"),
        ("Weapon_TommyGun_LOD0", "Weapon_TommyGun", "Default"),
        ("Weapon_AssaultRifle_FAR_Base_LOD0", "Weapon_AssaultRifle_FAR", "Base"),
        ("Weapon_AssaultRifle_COBR-A_Scoped_LOD0", "Weapon_AssaultRifle_COBR-A", "Scoped"),
    ],
)
def test_strip_part_label(stem: str, folder: str, expected: str) -> None:
    assert strip_part_label(stem, folder) == expected


def test_strip_part_label_non_prefixed_stem_prettifies() -> None:
    # Part stem that does not share the folder prefix (e.g. Armas magnum) -> readable fallback.
    assert strip_part_label("Crm_Magnum_Mk3_LOD0", "Weapon_Armas_Magnum") == "Crm Magnum Mk3"
