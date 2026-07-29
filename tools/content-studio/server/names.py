"""Resolve a disk weapon (folder/mesh name) to its real display name.

APB has three parallel naming systems:

  1. disk (umodel package/mesh):  ``Weapon_ColbyClassic`` / ``Crm_Magnum_Mk3_LOD0``
  2. SDD / apbdb taxonomy:        ``Weapon_Pistol_FBW`` / ``Weapon_Grenade_Concussion``
  3. display names:              ``Obeya FBW`` / ``Concussion Grenade`` / ``ATAC 424``

There is no exact key shared across all three, so this resolver runs a cascade
(most-authoritative first) and reports how each name was resolved so callers can
surface confidence:

    alias  -> curated mapping for marketing names no algorithm can derive
    exact  -> normalized token-set OR normalized-string is identical to a key
    catalog-> collision-free class-aware model-key match from the apbdb catalog
    derived-> honest prettified asset name (no wrong guessing)

Fuzzy/partial matching is deliberately NOT used: a confidently-wrong name (e.g.
labelling the Apocalypse rifle "ATAC 424") is worse than an honest derived name
("Apocalypse"). We only accept a canonical name on an exact normalized match;
everything else falls back to a clean prettified asset name.

Display strings are sourced from the retail localization (``weapon_display_names.json``,
extracted by tools/scripts/extract_weapon_names.py) FIRST, with the authoritative apbdb
``weapons_catalog.json`` and ``weapon_base_skin_map.json`` as fallbacks. Raw internal placeholder names
(``raptor_base``, ``DOW Base``, ``VBR_Silencer``) are rejected as junk.
"""

from __future__ import annotations

import re
from dataclasses import dataclass
from functools import lru_cache
from pathlib import Path
from typing import Literal

from _alias_data import ALIASES as _ALIASES
from _alias_data import FAMILY_DISPLAY as _FAMILY_DISPLAY
from name_sources import CatalogItem, SkinMap, load_name_sources

REPO_ROOT = Path(__file__).resolve().parents[3]

# tokens that carry no identity (extraction/versioning noise) — kept conservative
_STOP = {
    "weapon", "armas", "crm", "enf", "cr", "lod0", "lod", "design", "the",
}

CLASS_PREFIXES = frozenset({
    "smg", "assaultrifle", "rifle", "pistol", "shotgun", "sniperrifle", "sniper",
    "lmg", "lightmachinegun", "machinegun", "submachinegun", "grenade", "carbine",
    "explosive", "secondary", "primary", "armas", "revolver", "machinepistol",
    "tacticalassaultrifle",
})

Confidence = Literal["curated", "alias", "exact", "catalog", "derived"]


def _split_camel(s: str) -> str:
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1 \2", s)
    s = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1 \2", s)
    return s


def _strip_lod(s: str) -> str:
    return re.sub(r"_LOD\d+$", "", s)


def _tokens(s: str) -> frozenset[str]:
    s = _split_camel(_strip_lod(s))
    parts = re.split(r"[^A-Za-z0-9]+", s)
    return frozenset(p.lower() for p in parts if p and p.lower() not in _STOP)


def _normstr(s: str) -> str:
    """Casing/punctuation-insensitive identity (so ``ATac`` == ``ATAC``)."""
    return re.sub(r"[^a-z0-9]", "", _strip_lod(s).lower())


def _model_key(s: str) -> str:
    """Normalize a disk or catalog key after removing its optional weapon class."""
    value = _strip_lod(s)
    if value.casefold().startswith("weapon_"):
        value = value[7:]
    parts = value.split("_", 1)
    if parts and parts[0].casefold() in CLASS_PREFIXES:
        value = parts[1] if len(parts) == 2 else ""
    return re.sub(r"[^a-z0-9]", "", value.casefold())


def _prettify(folder_or_stem: str) -> str:
    """Honest readable name derived from the asset itself (no external guess)."""
    toks = [t for t in re.split(r"[^A-Za-z0-9]+", _split_camel(_strip_lod(folder_or_stem))) if t]
    toks = [t for t in toks if t.lower() not in {"weapon", "design", "crm", "enf"}]
    return " ".join(toks) if toks else folder_or_stem


def _is_junk(display: str) -> bool:
    """Reject raw internal placeholder display strings (not real product names)."""
    if not display or "_" in display:
        return True
    low = display.strip().lower()
    if low.startswith("dnt "):
        return True
    if low.endswith("base"):
        return True
    # a lone all-lowercase token like "raptor" (internal codename, not a display name)
    if display == display.lower() and " " not in display and "-" not in display:
        return True
    return False


def _clean_display(display: str) -> str:
    """Remove markup that can wrap localized display text."""
    return re.sub(r"<Color:[^>]*>(.*?)</Color>", r"\1", display, flags=re.IGNORECASE | re.DOTALL)


@dataclass(frozen=True, slots=True)
class ResolvedName:
    display: str
    sapbdb: str | None
    confidence: Confidence
    score: float


class WeaponNameResolver:
    """Builds name indexes once, resolves many disk weapons against them."""

    def __init__(
        self,
        display_names: dict[str, str],
        apbdb_weapons: list[CatalogItem],
        skinmap: SkinMap,
    ) -> None:
        self._key_display: dict[str, str] = {}

        def register(key: str | None, disp: str | None) -> None:
            if not key or not disp:
                return
            clean = _clean_display(disp)
            if not _is_junk(clean):
                if key not in self._key_display:
                    self._key_display[key] = clean

        # Registration order sets precedence (first wins): retail localization is
        # the most authoritative display source, then apbdb, then skinmap fallback.
        for sapbdb, disp in display_names.items():
            register(sapbdb, disp)
        catalog_weapons = sorted(apbdb_weapons, key=lambda w: bool(w.get("is_skin")))
        model_sources: list[str] = []
        for w in catalog_weapons:
            name = w.get("name")
            base_key = w.get("base_weapon_id")
            mesh_key = w.get("mesh_key")
            register(base_key, name)
            register(w.get("id"), name)
            register(mesh_key, name)
            if base_key:
                model_sources.append(base_key)
                if mesh_key and _model_key(mesh_key) == _model_key(base_key):
                    model_sources.append(mesh_key)
        for fam in skinmap.get("families", {}).values():
            register(fam.get("base"), fam.get("base_display_name"))

        norm_candidates: dict[str, dict[str, str]] = {}
        tok_candidates: dict[frozenset[str], dict[str, str]] = {}
        for key, disp in self._key_display.items():
            ns = _normstr(key)
            if ns:
                norm_candidates.setdefault(ns, {}).setdefault(disp, key)
            tk = _tokens(key)
            if tk:
                tok_candidates.setdefault(tk, {}).setdefault(disp, key)
        self._norm_index: dict[str, tuple[str, str]] = {
            ns: (key, display)
            for ns, displays in norm_candidates.items()
            if len(displays) == 1
            for display, key in displays.items()
        }
        self._tok_index: list[tuple[frozenset[str], str, str]] = [
            (tk, key, display)
            for tk, displays in tok_candidates.items()
            if len(displays) == 1
            for display, key in displays.items()
        ]

        model_candidates: dict[str, dict[str, str]] = {}
        for key in model_sources:
            model = _model_key(key)
            disp = self._key_display.get(key)
            if model and disp:
                displays = model_candidates.setdefault(model, {})
                if disp not in displays:
                    displays[disp] = key
        self._model_index: dict[str, tuple[str, str]] = {
            model: (key, display)
            for model, displays in model_candidates.items()
            if len(displays) == 1
            for display, key in displays.items()
        }

    def resolve(self, folder_id: str, primary_stem: str) -> ResolvedName:
        if folder_id in _FAMILY_DISPLAY:
            return ResolvedName(_FAMILY_DISPLAY[folder_id], None, "curated", 1.0)

        if folder_id in _ALIASES:
            key = _ALIASES[folder_id]
            disp = self._key_display.get(key) or _prettify(folder_id)
            return ResolvedName(disp, key, "alias", 1.0)

        candidates = [folder_id, primary_stem]

        for c in candidates:
            ns = _normstr(c)
            if ns and ns in self._norm_index:
                key, disp = self._norm_index[ns]
                return ResolvedName(disp, key, "exact", 1.0)

        for c in candidates:
            ct = _tokens(c)
            if not ct:
                continue
            for tk, key, disp in self._tok_index:
                if tk == ct:
                    return ResolvedName(disp, key, "exact", 1.0)

        for c in candidates:
            model = _model_key(c)
            if model and model in self._model_index:
                key, disp = self._model_index[model]
                return ResolvedName(disp, key, "catalog", 0.9)

        return ResolvedName(_prettify(folder_id), None, "derived", 0.0)


@lru_cache(maxsize=1)
def default_resolver() -> WeaponNameResolver:
    sources = load_name_sources(REPO_ROOT)
    return WeaponNameResolver(sources.display_names, sources.catalog_items, sources.skin_map)
