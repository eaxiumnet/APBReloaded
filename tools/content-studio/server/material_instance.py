"""Parser for extracted MaterialInstanceConstant property dumps."""

from __future__ import annotations

import re
from dataclasses import dataclass, field
from pathlib import Path

Color = tuple[float, float, float, float]
_PARENT_RE = re.compile(r"^Parent\s*=\s*(?:\w+'|)([^'\r\n]+)'?", re.MULTILINE)
_VECTOR_RE = re.compile(
    r"VectorParameterValues\[(\d+)\]\s*=\s*\{\s*(?:ParameterInfo\s*=\s*)?"
    r"ParameterValue\s*=\s*\{([^}]*)\}\s*ParameterName\s*=\s*([^\r\n}]+)",
    re.DOTALL,
)
_SCALAR_RE = re.compile(
    r"ScalarParameterValues\[(\d+)\]\s*=\s*\{\s*(?:ParameterInfo\s*=\s*)?"
    r"ParameterValue\s*=\s*([-+.\deE]+)\s*ParameterName\s*=\s*([^\r\n}]+)",
    re.DOTALL,
)
_TEXTURE_RE = re.compile(
    r"TextureParameterValues\[(\d+)\]\s*=\s*\{\s*(?:ParameterInfo\s*=\s*)?"
    r"ParameterValue\s*=\s*(?:Texture2D')?([^'\r\n]+)'?\s*ParameterName\s*=\s*([^\r\n}]+)",
    re.DOTALL,
)
_COMPONENT_RE = re.compile(r"([RGBA])\s*=\s*([-+.\deE]+)")


@dataclass(frozen=True, slots=True)
class MaterialInstance:
    """Authored scalar, vector, texture, and parent values for one skin."""

    vectors: dict[str, Color] = field(default_factory=dict)
    scalars: dict[str, float] = field(default_factory=dict)
    textures: dict[str, str] = field(default_factory=dict)
    parent: str | None = None


def _first_records(pattern: re.Pattern[str], text: str) -> list[tuple[str, str, str]]:
    seen_indices: set[str] = set()
    records: list[tuple[str, str, str]] = []
    for match in pattern.finditer(text):
        index, value, name = match.groups()
        if index not in seen_indices:
            seen_indices.add(index)
            records.append((name.strip(), value.strip(), index))
    return records


def _vector(value: str) -> Color:
    components = {key: float(component) for key, component in _COMPONENT_RE.findall(value)}
    return (
        components.get("R", 0.0),
        components.get("G", 0.0),
        components.get("B", 0.0),
        components.get("A", 1.0),
    )


def parse_material_instance(path_or_text: Path | str) -> MaterialInstance:
    """Parse a material instance dump from a path or its text."""
    candidate = Path(path_or_text) if isinstance(path_or_text, str) else path_or_text
    text = candidate.read_text(encoding="utf-8") if candidate.is_file() else str(path_or_text)
    parent_match = _PARENT_RE.search(text)
    vectors = {name: _vector(value) for name, value, _ in _first_records(_VECTOR_RE, text)}
    scalars = {name: float(value) for name, value, _ in _first_records(_SCALAR_RE, text)}
    textures = {name: value for name, value, _ in _first_records(_TEXTURE_RE, text)}
    parent = parent_match.group(1).strip() if parent_match is not None else None
    return MaterialInstance(vectors=vectors, scalars=scalars, textures=textures, parent=parent)
