from __future__ import annotations

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from material_instance import parse_material_instance  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[4]
FIXTURE = (
    REPO_ROOT / "Content/Extracted/Weapons/Weapon_AssaultRifle/Weapon_AssaultRifle"
    / "MaterialInstanceConstant/Weapon_AssaultRifle_Bloodrose_MAT_INST.props.txt"
)


def test_parser_normalizes_integer_and_missing_alpha_values() -> None:
    # Given
    text = """VectorParameterValues[0] = { ParameterValue = { R=1 G=0.5 B=0 } ParameterName = Tint }"""
    # When
    material = parse_material_instance(text)
    # Then
    assert material.vectors["Tint"] == (1.0, 0.5, 0.0, 1.0)


def test_parser_preserves_first_duplicate_index() -> None:
    # Given
    text = """ScalarParameterValues[0] = { ParameterValue = 2 ParameterName = Gloss }
ScalarParameterValues[0] = { ParameterValue = 9 ParameterName = Gloss }"""
    # When
    material = parse_material_instance(text)
    # Then
    assert material.scalars["Gloss"] == 2.0


def test_parser_accepts_three_component_vector_without_alpha() -> None:
    # Given
    text = """VectorParameterValues[4] = { ParameterValue = { R=.22 G=0 B=0 } ParameterName = Body Pattern Col A }"""
    # When
    material = parse_material_instance(text)
    # Then
    assert material.vectors["Body Pattern Col A"] == (0.22, 0.0, 0.0, 1.0)


def test_parser_reads_bloodrose_body_pattern_color() -> None:
    material = parse_material_instance(FIXTURE)
    assert material.vectors["Body Pattern Col A"][:3] == pytest.approx((0.22, 0.0, 0.0), abs=0.001)
