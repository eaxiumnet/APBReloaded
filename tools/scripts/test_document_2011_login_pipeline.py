#!/usr/bin/env python3
"""Tests for document_2011_login_pipeline.py against real 2011 tree + shipped function."""
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(Path(__file__).resolve().parent))
import document_2011_login_pipeline as doc  # noqa: E402

SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9234a14c05ca\implementer")


class Test2011LoginPipeline(unittest.TestCase):
    def test_required_files_exist_on_disk(self):
        for key in ("exe", "login_map", "user_ui_script", "frontend_upk", "gameflow_upk", "engine_ini"):
            p = doc.REQUIRED_PATHS[key]
            self.assertTrue(p.is_file(), f"missing {key}: {p}")

    def test_engine_map_is_apbloginlevel(self):
        maps = doc.parse_engine_maps(doc.REQUIRED_PATHS["engine_ini"])
        joined = "\n".join(maps)
        self.assertIn("APBLoginLevel", joined)
        self.assertTrue(any(m.startswith("Map=") for m in maps) or any("Map=" in m for m in maps))

    def test_uidistrict_absent_in_2011_content(self):
        hits = doc.uidistrict_present()
        self.assertEqual(hits, [], f"unexpected UIDistrict paths: {hits}")

    def test_build_pipeline_uses_real_paths(self):
        log = None
        for c in sorted((doc.APBGAME / "Logs").glob("Launch*.log"), key=lambda p: -p.stat().st_mtime):
            if c.stat().st_size > 1000:
                log = c
                break
        pipeline = doc.build_pipeline(log)
        self.assertTrue(pipeline["exists"]["exe"])
        self.assertTrue(pipeline["exists"]["login_map"])
        stages = [s["stage"] for s in pipeline["pipeline"]]
        for need in (
            "default_map",
            "load_map_shell",
            "ui_script_gameflow_manager",
            "ui_packages",
            "fullscreen_movies",
            "scene_stack",
            "not_present",
        ):
            self.assertIn(need, stages)
        # login map tiny shell
        size = Path(pipeline["paths"]["login_map"]).stat().st_size
        self.assertLess(size, 100_000, "APBLoginLevel should be small shell")

    def test_script_signals_include_gameflow_frontend(self):
        sigs = doc.mine_script_signals(doc.REQUIRED_PATHS["user_ui_script"])
        blob = " ".join(sigs).lower()
        self.assertTrue("gameflow" in blob or "frontend" in blob, sigs[:30])

    def test_main_writes_report_to_extracted(self):
        out = SCRATCH / "pipeline_test_out"
        out.mkdir(parents=True, exist_ok=True)
        code = doc.main(["--out-dir", str(out), "--scratch", str(SCRATCH)])
        self.assertEqual(code, 0)
        md = out / "LOGIN_MENU_RENDERING_PIPELINE.md"
        js = out / "login_menu_rendering_pipeline.json"
        self.assertTrue(md.is_file())
        self.assertTrue(js.is_file())
        text = md.read_text(encoding="utf-8").lower()
        self.assertIn("apbloginlevel", text)
        self.assertIn("gameflow", text)
        self.assertIn("uidistrict", text)
        data = json.loads(js.read_text(encoding="utf-8"))
        self.assertIn("pipeline", data)
        self.assertEqual(data.get("uidistrict_paths"), [])


if __name__ == "__main__":
    unittest.main()
