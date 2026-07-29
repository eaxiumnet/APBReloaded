from __future__ import annotations

import json
from dataclasses import dataclass
from pathlib import Path
from typing import Protocol, TypeAlias, TypedDict

JsonValue: TypeAlias = str | int | float | bool | None | list["JsonValue"] | dict[str, "JsonValue"]


class JsonLoader(Protocol):
    def __call__(self, s: str) -> JsonValue: ...


class CatalogItem(TypedDict, total=False):
    id: str
    name: str
    base_weapon_id: str
    mesh_key: str
    is_skin: bool


class SkinFamily(TypedDict, total=False):
    base: str
    base_display_name: str


class SkinMap(TypedDict, total=False):
    families: dict[str, SkinFamily]


@dataclass(frozen=True, slots=True)
class NameSources:
    display_names: dict[str, str]
    catalog_items: list[CatalogItem]
    skin_map: SkinMap


def _read_json(path: Path, loader: JsonLoader = json.loads) -> JsonValue:
    return loader(path.read_text(encoding="utf-8"))


def _string_value(value: JsonValue | None) -> str | None:
    return value if isinstance(value, str) else None


def _catalog_items(payload: JsonValue) -> list[CatalogItem]:
    raw_items = payload.get("items") if isinstance(payload, dict) else payload
    if not isinstance(raw_items, list):
        return []
    items: list[CatalogItem] = []
    for raw_item in raw_items:
        if not isinstance(raw_item, dict):
            continue
        item: CatalogItem = {}
        for field in ("id", "name", "base_weapon_id", "mesh_key"):
            value = _string_value(raw_item.get(field))
            if value:
                item[field] = value
        is_skin = raw_item.get("is_skin")
        if isinstance(is_skin, bool):
            item["is_skin"] = is_skin
        items.append(item)
    return items


def _display_names(payload: JsonValue) -> dict[str, str]:
    raw_names = payload.get("names") if isinstance(payload, dict) else None
    if not isinstance(raw_names, dict):
        return {}
    return {
        key: value
        for key, raw_value in raw_names.items()
        if (value := _string_value(raw_value))
    }


def _skin_map(payload: JsonValue) -> SkinMap:
    raw_families = payload.get("families") if isinstance(payload, dict) else None
    if not isinstance(raw_families, dict):
        return {}
    families: dict[str, SkinFamily] = {}
    for family_id, raw_family in raw_families.items():
        if not isinstance(raw_family, dict):
            continue
        family: SkinFamily = {}
        for field in ("base", "base_display_name"):
            value = _string_value(raw_family.get(field))
            if value:
                family[field] = value
        families[family_id] = family
    return {"families": families}


def load_name_sources(repo_root: Path) -> NameSources:
    data = repo_root / "Content" / "Data"
    display_path = data / "weapon_display_names.json"
    catalog_path = data / "weapons_catalog.json"
    legacy_path = data / "weapons.json"
    skinmap_path = repo_root / "work" / "weapon_base_skin_map.json"
    display_names = _display_names(_read_json(display_path)) if display_path.is_file() else {}
    if catalog_path.is_file():
        catalog_items = _catalog_items(_read_json(catalog_path))
    elif legacy_path.is_file():
        catalog_items = _catalog_items(_read_json(legacy_path))
    else:
        catalog_items = []
    skin_map: SkinMap
    if skinmap_path.is_file():
        skin_map = _skin_map(_read_json(skinmap_path))
    else:
        skin_map = {}
    return NameSources(display_names, catalog_items, skin_map)
