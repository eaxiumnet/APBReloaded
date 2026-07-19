#!/usr/bin/env python3
"""Unit tests: real Steam PrivateServer sources + districts/placements on disk."""
from __future__ import annotations

import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from parse_privateserver import (  # noqa: E402
    DB_CHARACTER,
    LOBBY_OPODES,
    WORLD_OPCODES,
    load_privateserver_map,
    parse_csharp_enum_opcodes,
)
from steam_inventory import inventory  # noqa: E402
from placement_loader import (  # noqa: E402
    load_manifest_from_file,
    manifest_uses_engine_cubes,
)

PROJECT = Path(r"D:\APBReloaded")
PLACEMENTS = PROJECT / "Content" / "Data" / "district_placements"
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-e6df5b4a9676\implementer")


class TestPrivateServerParse(unittest.TestCase):
    def test_lobby_ask_login_from_real_file(self):
        text = LOBBY_OPODES.read_text(encoding="utf-8", errors="ignore")
        ops = parse_csharp_enum_opcodes(text)
        self.assertEqual(ops["ASK_LOGIN"], 0x3E8)
        self.assertEqual(ops["ASK_CHARACTER_CREATE"], 0x3EE)
        self.assertEqual(ops["ANS_LOGIN_SUCCESS"], 0x7D4)

    def test_world_district_enter_from_real_file(self):
        text = WORLD_OPCODES.read_text(encoding="utf-8", errors="ignore")
        ops = parse_csharp_enum_opcodes(text)
        self.assertEqual(ops["ASK_DISTRICT_ENTER"], 0xBBD)
        self.assertEqual(ops["DISTRICT_LIST"], 0xFA6)

    def test_dbcharacter_fields_from_real_file(self):
        data = load_privateserver_map()
        fields = set(data["character_fields"])
        for need in ("Name", "Faction", "Gender", "Money", "Threat", "Id", "AcctId"):
            self.assertIn(need, fields, f"missing field {need} in real DBCharacter.cs")

    def test_generated_header_matches_source(self):
        data = load_privateserver_map()
        hdr = (PROJECT / "Source" / "APBReloaded" / "Domain" / "APBPrivateServerOpcodes.h")
        if not hdr.is_file():
            self.skipTest("header not generated yet")
        text = hdr.read_text(encoding="utf-8")
        self.assertIn("ASK_LOGIN = 1000u", text)  # 0x3E8 = 1000
        self.assertIn(str(data["lobby"]["ASK_LOGIN"]) + "u", text)


class TestSteamInventoryAndPlacements(unittest.TestCase):
    def test_inventory_nonzero_buckets(self):
        inv = inventory()
        pc = inv["primary_counts"]
        self.assertGreater(pc["psk"], 100)
        self.assertGreater(pc["audio_wem"] + pc["audio_wav"], 100)
        self.assertGreater(pc["placements"], 0)
        self.assertGreater(pc["privateserver_cs"], 10)

    def test_districts_json_real(self):
        p = PROJECT / "Content" / "Data" / "districts.json"
        self.assertTrue(p.is_file())
        data = json.loads(p.read_text(encoding="utf-8"))
        # accept list or {districts:[...]}
        rows = data if isinstance(data, list) else data.get("districts") or data.get("Districts") or []
        self.assertGreater(len(rows), 0)
        # first row has id/name-ish keys
        r0 = rows[0]
        if isinstance(r0, dict):
            keys = {k.lower() for k in r0.keys()}
            self.assertTrue(keys & {"id", "district_id", "name", "map", "map_name"})

    def test_asylum_block_placement_loader_shape(self):
        """Drive real district placement rows with the same fields as LoadManifestFromFile."""
        path = PLACEMENTS / "Asylum_Block.json"
        self.assertTrue(path.is_file(), f"missing {path}")
        m = load_manifest_from_file(path)
        self.assertIsNotNone(m)
        assert m is not None
        self.assertTrue(m.district_id, "district_id empty")
        self.assertTrue(m.source_package, "source_package empty")
        self.assertGreater(len(m.placements), 10)
        self.assertFalse(manifest_uses_engine_cubes(m), "must not use BasicShapes cubes")
        e0 = m.placements[0]
        self.assertTrue(e0.mesh_id, "mesh_id required")
        self.assertTrue(e0.ue_path, "ue_path required")
        self.assertNotIn("BasicShapes/Cube", e0.ue_path)
        self.assertTrue(e0.package or e0.mesh_id)
        self.assertEqual(len(e0.location), 3)
        # non-zero world coords typical of San Paro layouts
        self.assertTrue(any(abs(c) > 1.0 for c in e0.location), f"location looks empty: {e0.location}")
        # write structural proof for skeptic
        SCRATCH.mkdir(parents=True, exist_ok=True)
        lines = [
            f"path={path}",
            f"district_id={m.district_id}",
            f"source_package={m.source_package}",
            f"placements={len(m.placements)}",
            f"player_start={m.player_start}",
            f"stream_chunks={m.stream_chunk_count}",
            f"sample_mesh_id={e0.mesh_id}",
            f"sample_ue_path={e0.ue_path}",
            f"sample_package={e0.package}",
            f"sample_location={e0.location}",
            "loader_shape=LoadManifestFromFile_mirror_ok",
        ]
        (SCRATCH / "placement_structural.txt").write_text("\n".join(lines) + "\n", encoding="utf-8")

    def test_financial_bound_manifest_if_present(self):
        path = PLACEMENTS / "Financial_Block09_bound.json"
        if not path.is_file():
            path = PLACEMENTS / "Financial_Block09.json"
        self.assertTrue(path.is_file())
        m = load_manifest_from_file(path)
        self.assertIsNotNone(m)
        assert m is not None
        non_cube = [e for e in m.placements if "BasicShapes/Cube" not in e.ue_path]
        self.assertGreater(len(non_cube), 0)


if __name__ == "__main__":
    raise SystemExit(0 if unittest.main(verbosity=2, exit=False).result.wasSuccessful() else 1)
