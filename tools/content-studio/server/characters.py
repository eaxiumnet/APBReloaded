"""Character mesh catalog for the viewer's Characters tab.

Lists every item under CharactersBulk that carries a real PSK (bodies,
clothing accessories, crowd NPCs, dev/UI characters) so the viewer can pick
one and preview it skinned + animated. Every exported character mesh shares
the full 139-bone Bip01 reference skeleton, so any animset clip applies to
any item here.
"""

from __future__ import annotations

from pathlib import Path

# Whole-body meshes (F_Body_Base etc.) — the base layers clothing skins onto.
_BODY_DIRS = {
    "F_Body", "F_Body_Base", "F_Body_Eye", "F_Body_Head", "F_Body_Skin",
    "M_Body", "M_Body_Base", "M_Body_Eye", "M_Body_Head", "M_Body_Skin",
}


def _preferred_psk(sm3_dir: Path, item_name: str) -> Path | None:
    """Pick the PSK that best represents the item for a skinned preview."""
    psks = sorted(
        path for path in sm3_dir.iterdir()
        if path.suffix.casefold() in (".psk", ".pskx")
    )
    if not psks:
        return None
    # Clothing accessories export as <Item>_Xtra.psk; prefer it over the
    # base/ID meshes that share the folder.
    xtra = [path for path in psks if "_Xtra" in path.stem]
    if xtra:
        return xtra[0]
    # Whole-character exports are named after the item (or StudioCharacter
    # for the crowd LC builds).
    named = [path for path in psks if path.stem.casefold() == item_name.casefold()]
    if named:
        return named[0]
    studio = [path for path in psks if path.stem == "StudioCharacter"]
    if studio:
        return studio[0]
    base = [path for path in psks if path.stem.endswith(("_Base", "_Body_Base"))]
    if base:
        return base[0]
    return psks[0]


def _slot(item_name: str) -> str:
    """Clothing slot label from the <Prefix>_<Slot>_<Item> naming."""
    parts = item_name.split("_")
    if len(parts) < 3:
        return "Other"
    if parts[1] in {"Test", "Config", "Minigame", "Pumpkin", "Nutcracker"}:
        return "Other"
    return parts[1]


def build_character_catalog(characters_bulk: Path) -> list[dict]:
    """List every character/clothing item with a real mesh under CharactersBulk.

    Returns dicts:
      id:       item folder name (unique)
      name:     item folder name
      category: "body" | "clothing" | "crowd" | "character"
      slot:     clothing slot label (None for non-clothing)
      psk:      selected mesh file name
      relpath:  PSK relpath under CharactersBulk (feeds /api/animation.glb)
      bytes:    mesh file size
    """
    if not characters_bulk.is_dir():
        return []

    items: list[dict] = []
    for item_dir in sorted(characters_bulk.iterdir()):
        if not item_dir.is_dir():
            continue
        sm3 = item_dir / item_dir.name / "SkeletalMesh3"
        if not sm3.is_dir():
            continue
        psk = _preferred_psk(sm3, item_dir.name)
        if psk is None:
            continue
        if item_dir.name in _BODY_DIRS:
            category: str = "body"
            slot: str | None = None
        elif "_Xtra" in psk.stem:
            category = "clothing"
            slot = _slot(item_dir.name)
        elif item_dir.name.startswith(("LC_", "CrowdLC_")):
            category = "crowd"
            slot = None
        else:
            category = "character"
            slot = None
        items.append({
            "id": item_dir.name,
            "name": item_dir.name,
            "category": category,
            "slot": slot,
            "psk": psk.name,
            "relpath": psk.relative_to(characters_bulk).as_posix(),
            "bytes": psk.stat().st_size,
        })
    return items
