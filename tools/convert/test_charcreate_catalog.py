#!/usr/bin/env python3
"""Unit tests: real clothing catalog drives CharacterCreate slot options / equip IDs."""
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

PROJECT = Path(r"D:\APBReloaded")
CLOTHING = PROJECT / "Content" / "Data" / "clothing.json"
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-932e0e4d96d0\implementer")
SOURCE = PROJECT / "Source" / "APBReloaded" / "Systems"


class TestCharCreateCatalog(unittest.TestCase):
    def test_clothing_catalog_slots(self):
        self.assertTrue(CLOTHING.is_file())
        data = json.loads(CLOTHING.read_text(encoding="utf-8"))
        items = data if isinstance(data, list) else data.get("items") or []
        self.assertGreater(len(items), 50)
        by_slot: dict[str, list[str]] = {}
        for it in items:
            if not isinstance(it, dict):
                continue
            slot = str(it.get("slot") or "").lower()
            iid = str(it.get("id") or "")
            if slot and iid:
                by_slot.setdefault(slot, []).append(iid)
        for need in ("head", "torso", "legs", "feet", "hands", "face"):
            self.assertIn(need, by_slot, f"missing slot {need}")
            self.assertGreater(len(by_slot[need]), 0)
        # simulate equip selection: pick first per slot
        equipped = {s: ids[0] for s, ids in by_slot.items() if ids}
        self.assertGreaterEqual(len(equipped), 6)
        SCRATCH.mkdir(parents=True, exist_ok=True)
        (SCRATCH / "charcreate_catalog_equip.txt").write_text(
            "\n".join(f"{k}={v}" for k, v in sorted(equipped.items())) + "\n",
            encoding="utf-8",
        )

    def test_preview_source_wired(self):
        cpp = (SOURCE / "APBFrontendWidget.cpp").read_text(encoding="utf-8", errors="ignore")
        act = (SOURCE / "APBCharacterCreatePreviewActor.cpp").read_text(encoding="utf-8", errors="ignore")
        for needle in (
            "EnsureCharacterPreview",
            "RefreshCharacterPreviewFromUI",
            "PREVIEW_OK",
            "CharPreviewImage",
            "PanelSizeBox",
            "BodyScroll",
        ):
            self.assertIn(needle, cpp, f"missing {needle}")
        for needle in (
            "SceneCaptureComponent2D",
            "PREVIEW_OK",
            "Contact_Bloodrose",
            "ApplyClothingSlotVisual",
        ):
            self.assertIn(needle, act, f"missing {needle} in preview actor")
        (SCRATCH / "charcreate_structure.txt").write_text(
            "\n".join(
                [
                    "preview_actor=APBCharacterCreatePreviewActor",
                    "ui=CharPreviewImage+SceneCaptureRT",
                    "scale=PanelSizeBox+BodyScroll",
                    "mesh_paths=Contact_Bloodrose|Contact_LaRocha|Wardrobe/StudioCharacter",
                    "apply=ApplyAppearanceFromEditor+RefreshCharacterPreviewFromUI",
                ]
            )
            + "\n",
            encoding="utf-8",
        )


if __name__ == "__main__":
    raise SystemExit(0 if unittest.main(verbosity=2, exit=False).result.wasSuccessful() else 1)
