#!/usr/bin/env python3
"""Unit tests: real Steam Dialogue_Media.txt drives stem name mapping."""
from __future__ import annotations

import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from audio_world_map import (  # noqa: E402
    NONLINEAR_STEM_IDS,
    build_catalog,
    parse_media_txt_line,
)

STEAM_DIALOGUE_MEDIA = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded"
    r"\APBGame\Content\Audio\SoundBanks\English(US)\Dialogue_Media.txt"
)
STEAM_AUDIO = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Audio"
)


class TestAudioWorldMap(unittest.TestCase):
    def test_parse_real_dialogue_media_five_stems(self):
        self.assertTrue(STEAM_DIALOGUE_MEDIA.is_file(), "Steam Dialogue_Media.txt missing")
        text = STEAM_DIALOGUE_MEDIA.read_text(errors="ignore")
        found: dict[int, str] = {}
        for line in text.splitlines():
            row = parse_media_txt_line(line)
            if row and row.short_id in NONLINEAR_STEM_IDS:
                found[row.short_id] = row.name
                self.assertIn("Interactive Music", row.hierarchy.replace("/", "\\") + row.hierarchy)
        for wid, expect in NONLINEAR_STEM_IDS.items():
            self.assertIn(wid, found, f"missing stem id {wid} in real index")
            self.assertEqual(
                found[wid].lower(),
                expect.lower(),
                f"id {wid}: index name {found[wid]!r} != {expect!r}",
            )

    def test_build_catalog_five_stems(self):
        rows = build_catalog(STEAM_AUDIO)
        by_id = {r.short_id: r for r in rows}
        for wid, expect in NONLINEAR_STEM_IDS.items():
            self.assertIn(wid, by_id)
            self.assertEqual(by_id[wid].name, expect)
            self.assertTrue(
                any("ThemeMusicNonlinear" in e for e in by_id[wid].event_names),
                f"{expect} missing Play_ThemeMusicNonlinear",
            )
            self.assertIn("interactive_music_stem", by_id[wid].play_context)


if __name__ == "__main__":
    raise SystemExit(0 if unittest.main(verbosity=2, exit=False).result.wasSuccessful() else 1)
