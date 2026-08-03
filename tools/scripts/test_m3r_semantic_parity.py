from __future__ import annotations

import copy
import json
import tempfile
import unittest
from pathlib import Path

from validate_m3r_semantic_parity import (
    DEFAULT_LEDGER,
    DEFAULT_LOADED_VALIDATION,
    DEFAULT_MESH_MANIFEST,
    DEFAULT_PLACEMENT_MANIFEST,
    SemanticParityError,
    parse_media,
    parse_png,
    parse_psa,
    parse_tga,
    parse_wav,
    read_json,
    validate_animation_parity,
    validate_audio_parity,
    validate_material_parity,
    validate_mesh_manifest,
    validate_placement_manifest,
    validate_social_conversion,
    validate_texture_parity,
    validate_ui_visual_parity,
    validate_video_parity,
)


class M3RSemanticParityTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.mesh_manifest = read_json(DEFAULT_MESH_MANIFEST)
        cls.loaded_validation = read_json(DEFAULT_LOADED_VALIDATION)
        cls.ledger = read_json(DEFAULT_LEDGER)
        cls.placement_manifest = read_json(DEFAULT_PLACEMENT_MANIFEST)

    def test_current_g1_mesh_and_social_placement_evidence_passes(self) -> None:
        mesh = validate_mesh_manifest(
            copy.deepcopy(self.mesh_manifest),
            copy.deepcopy(self.loaded_validation),
            copy.deepcopy(self.ledger),
        )

        placement = validate_placement_manifest(copy.deepcopy(self.placement_manifest))
        self.assertEqual(mesh["accepted_objects"], 26)
        self.assertEqual(mesh["accepted_sections"], 35)
        self.assertEqual(placement["accepted_rows"], 1832)
        self.assertEqual(placement["accepted_bound"], 763)

    def test_mesh_collision_mutation_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.mesh_manifest)
        mutated["entries"][0]["source_collision"]["use_simple_box_collision"] = True
        with self.assertRaisesRegex(SemanticParityError, "collision contract changed"):
            validate_mesh_manifest(
                mutated, copy.deepcopy(self.loaded_validation), copy.deepcopy(self.ledger)
            )

    def test_loaded_section_identity_mutation_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.loaded_validation)
        mutated["entries"][0]["section_identities"][0]["slot_name"] = "WrongMaterial"
        with self.assertRaisesRegex(SemanticParityError, "section/material identity mismatch"):
            validate_mesh_manifest(
                copy.deepcopy(self.mesh_manifest), mutated, copy.deepcopy(self.ledger)
            )

    def test_placement_digest_mutation_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.placement_manifest)
        mutated["placements"][0]["source_id"] = "tampered-source-id"
        with self.assertRaisesRegex(SemanticParityError, "source_id digest changed"):
            validate_placement_manifest(mutated)

    def test_engine_substitute_placement_is_rejected(self) -> None:
        mutated = copy.deepcopy(self.placement_manifest)
        bound = next(row for row in mutated["placements"] if row.get("ue_path"))
        bound["ue_path"] = "/Game/Engine/BasicShapes/Cube"
        with self.assertRaisesRegex(SemanticParityError, "engine substitute"):
            validate_placement_manifest(mutated)

    def test_incomplete_financial_batch_is_rejected(self) -> None:
        path = Path("Content/Data/district_placements/Financial_ArtProps_G1_batch.json")
        with self.assertRaisesRegex(SemanticParityError, "source_id digest changed"):
            validate_placement_manifest(read_json(path))

    def test_social_material_slot_negative_control_is_rejected(self) -> None:
        source = Path("work/evidence/social_archetype_mesh_conversion.json")
        document = read_json(source)
        document["meshes"][0]["source_material_slots"] = ["tampered_slot"]
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / source.name
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(SemanticParityError, "source/object material slots differ"):
                validate_social_conversion(path)

    def test_texture_parity_rejects_hash_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        row = next(e for e in ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "Texture2D"
                   and e.get("status") == "verified")
        row["intermediate_sha256"] = "0" * 64
        result = validate_texture_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_texture_tga_header_negative_control(self) -> None:
        import struct as struct_mod
        payload = bytearray(18)
        payload[2] = 0  # unsupported image type
        payload[12:14] = struct_mod.pack("<H", 8)
        payload[14:16] = struct_mod.pack("<H", 8)
        payload[16] = 32
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.tga"
            path.write_bytes(bytes(payload))
            with self.assertRaises(SemanticParityError):
                parse_tga(path)

    def test_png_header_negative_control(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.png"
            path.write_bytes(b"not-a-png" + b"\x00" * 32)
            with self.assertRaises(SemanticParityError):
                parse_png(path)

    def test_material_parity_rejects_chain_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        row = next(e for e in ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "MaterialInstanceConstant"
                   and e.get("status") == "verified")
        row["intermediate_sha256"] = "f" * 64
        result = validate_material_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_wav_header_negative_control(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "bad.wav"
            path.write_bytes(b"RIFF\x00\x00\x00\x00WAVEgarbage")
            with self.assertRaises(SemanticParityError):
                parse_wav(path)

    def test_audio_parity_rejects_chain_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        row = next(e for e in ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "SoundWave"
                   and e.get("status") == "verified")
        row["intermediate_sha256"] = "a" * 64
        result = validate_audio_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_video_parity_rejects_chain_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        row = next(e for e in ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "MediaFile"
                   and e.get("status") == "verified")
        row["intermediate_sha256"] = "b" * 64
        result = validate_video_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_media_dispatch_detects_containers(self) -> None:
        mp4 = next(e for e in self.ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "MediaFile"
                   and str(e.get("intermediate_path") or "").endswith(".mp4"))
        webm = next(e for e in self.ledger["entries"]
                    if isinstance(e, dict) and e.get("asset_class") == "MediaFile"
                    and str(e.get("intermediate_path") or "").endswith(".webm"))
        from validate_m3r_semantic_parity import root_path
        self.assertEqual(parse_media(root_path(mp4["intermediate_path"]))["container"], "mp4")
        self.assertEqual(parse_media(root_path(webm["intermediate_path"]))["container"], "webm")

    def test_animation_parity_rejects_chain_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        rows = [e for e in ledger["entries"]
                if isinstance(e, dict) and e.get("asset_class") == "AnimSet"]
        self.assertGreater(len(rows), 0)
        rows[0]["intermediate_sha256"] = "c" * 64
        result = validate_animation_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_psa_negative_control_rejects_key_drift(self) -> None:
        rows = [e for e in self.ledger["entries"]
                if isinstance(e, dict) and e.get("asset_class") == "AnimSet"]
        self.assertGreater(len(rows), 0)
        row = rows[0]
        from validate_m3r_semantic_parity import root_path
        metrics = parse_psa(root_path(row["intermediate_path"]))
        ledger = copy.deepcopy(self.ledger)
        for entry in ledger["entries"]:
            if isinstance(entry, dict) and entry.get("asset_key") == row["asset_key"]:
                entry["validation"] = {
                    "bones": metrics["bones"] + 1,
                    "anim_count": metrics["anim_count"],
                }
        result = validate_animation_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_ui_visual_parity_rejects_chain_mutation(self) -> None:
        ledger = copy.deepcopy(self.ledger)
        row = next(e for e in ledger["entries"]
                   if isinstance(e, dict) and e.get("asset_class") == "Texture2D"
                   and str(e.get("dest") or "").startswith("/Game/Imported/UI/")
                   and e.get("status") == "verified")
        row["intermediate_sha256"] = "d" * 64
        result = validate_ui_visual_parity(ledger)
        self.assertGreater(len(result["failures"]), 0)

    def test_new_class_parities_accept_current_ledger(self) -> None:
        for validator in (validate_texture_parity, validate_material_parity,
                          validate_audio_parity, validate_video_parity,
                          validate_animation_parity):
            result = validator(copy.deepcopy(self.ledger))
            self.assertEqual(result["status"], "pass", f"{validator.__name__} failed")
        result = validate_ui_visual_parity(copy.deepcopy(self.ledger))
        self.assertEqual(result["status"], "pass")


if __name__ == "__main__":
    unittest.main()
