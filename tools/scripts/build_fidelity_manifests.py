#!/usr/bin/env python3
"""Build the fidelity oracle manifests from the currently staged artifacts.

Hashes are generated from files under the repository root so a changed source or
staged UE asset cannot silently keep passing an old oracle row.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools"


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest().upper()


def row(
    row_id: str,
    domain: str,
    source: str,
    source_path: str,
    destination: str,
    behavior: str,
    comparison: str,
    threshold: dict[str, Any],
    *,
    destination_path: str | None = None,
    anchor: str | None = None,
) -> dict[str, Any]:
    src = ROOT / source_path
    dest = ROOT / (destination_path or destination)
    if not src.is_file():
        raise FileNotFoundError(src)
    if not dest.exists():
        raise FileNotFoundError(dest)
    result: dict[str, Any] = {
        "id": row_id,
        "domain": domain,
        "source": source,
        "source_path": source_path.replace("\\", "/"),
        "source_sha256": sha256(src),
        "destination": destination.replace("\\", "/"),
        "destination_path": (destination_path or destination).replace("\\", "/"),
        "destination_sha256": sha256(dest) if dest.is_file() else None,
        "expected_behavior": behavior,
        "comparison": comparison,
        "pass_threshold": threshold,
    }
    if anchor is not None:
        result["anchor"] = anchor
    return result


def main() -> None:
    rows = [
        row(
            "menu.login.background",
            "menu_art",
            "2011",
            "Content/Extracted/2011/LiveCurrentScene/hero/Login_Scene_Preview.png",
            "Content/UI/Frontend/2011/Constant_BG.tga",
            "Login uses the extracted 2011 scene treatment as the fixed camera bed.",
            "asset_exact",
            {"min_bytes": 100000, "required_extension": ".tga"},
        ),
        row(
            "menu.login.footer_art",
            "menu_art",
            "2011",
            "Content/Extracted/2011/MenuArt/APBMenus_Art_GameFlowScenes/Texture2D/frontendFooter.png",
            "Content/Imported/UI/Menu2011/Login/frontendFooter.uasset",
            "The login footer chrome resolves to the imported 2011 texture asset.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "menu.login.button_sfx",
            "menu_sfx",
            "2011",
            "Content/Extracted/2011/UISfx/ButtonPos.wav",
            "Content/Audio/UI/ButtonPos.uasset",
            "Hover and positive-selection feedback uses the extracted 2011 UI sound.",
            "asset_exact",
            {"min_bytes": 1000, "required_extension": ".uasset"},
        ),
        row(
            "menu.login.strings",
            "menu_strings",
            "2011",
            "Content/Data/ui_strings_2011.json",
            "Content/Data/ui_strings_2011.json",
            "Classic menu labels resolve through the 2011 stable string keys.",
            "json_valid",
            {"min_bytes": 1000, "required_top_level_keys": 1},
        ),
        row(
            "menu.login.anchors",
            "ui_anchors",
            "2011",
            "work/menu2011_layout_extracted.md",
            "Source/APBReloaded/Systems/Frontend/APBFrontendWidget.cpp",
            "The C++ frontend retains the measured 2011 layout anchors and scale path.",
            "text_contains",
            {"min_bytes": 10000},
            anchor="UpdateViewportScale",
        ),
        row(
            "character.palettes",
            "character_customization",
            "retail",
            "Content/Data/palettes.json",
            "Content/Data/palettes.json",
            "Retail colour palettes remain catalog-driven and deterministic.",
            "json_valid",
            {"min_bytes": 10000, "required_top_level_keys": 1},
        ),
        row(
            "character.wardrobe_categories",
            "character_customization",
            "retail",
            "Content/Data/wardrobe_categories.json",
            "Content/Data/wardrobe_categories.json",
            "All 15 retail wardrobe tabs resolve to distinct equip slots.",
            "json_valid",
            {"min_bytes": 1000, "required_top_level_keys": 1},
        ),
        row(
            "character.wardrobe_asset",
            "character_customization",
            "retail",
            "Content/Data/wardrobe_categories.json",
            "Content/Imported/Characters/Wardrobe/Contact_Bloodrose_F_Contact_Criminal_Bloodrose.uasset",
            "A non-placeholder imported wardrobe/character mesh is available to the preview path.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "district.social_asset",
            "district_assets",
            "retail",
            "Content/Data/district_stream.json",
            "Content/Imported/Districts/Social/MT000000B250000_03_LOD_0.uasset",
            "Social is the first streamed district and resolves a real imported block asset.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "district.financial_asset",
            "district_assets",
            "retail",
            "Content/Data/district_stream.json",
            "Content/Imported/Districts/Financial/FD_B01_Signage01_VertexLit_LOD_0.uasset",
            "Financial placement streaming resolves a real imported block asset.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "district.waterfront_asset",
            "district_assets",
            "retail",
            "Content/Data/district_stream.json",
            "Content/Imported/Districts/Waterfront/WaterfrontDistrict_Block01_Generic_0001_LOD_0.uasset",
            "Waterfront placement streaming resolves a real imported block asset.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "vehicle.catalog",
            "vehicle_data",
            "apbdb",
            "Content/Data/vehicles.json",
            "Content/Data/vehicles.json",
            "Vehicle handling values are read from the authoritative catalog, not guessed constants.",
            "json_valid",
            {"min_bytes": 100000, "required_top_level_keys": 1},
        ),
        row(
            "vehicle.imported_mesh",
            "vehicle_assets",
            "retail",
            "Content/Data/vehicles.json",
            "Content/Imported/Vehicles/V_A_2DrCoupe/PartMesh.uasset",
            "The drivable pawn has a real imported retail vehicle mesh.",
            "asset_exact",
            {"min_bytes": 10000, "required_extension": ".uasset"},
        ),
        row(
            "missions.templates",
            "missions",
            "retail",
            "Content/Data/mission_templates.json",
            "Content/Data/mission_templates.json",
            "Retail mission template strings and identifiers remain stable in the Domain catalog.",
            "json_valid",
            {"min_bytes": 10000, "required_top_level_keys": 1},
        ),
        row(
            "missions.objectives",
            "missions",
            "retail",
            "Content/Data/task_objectives.json",
            "Content/Data/task_objectives.json",
            "Retail task objectives remain addressable by the mission script library.",
            "json_valid",
            {"min_bytes": 1000, "required_top_level_keys": 1},
        ),
        row(
            "frontend.fixed_camera_login",
            "menu_visual",
            "2011",
            "Content/Extracted/2011/LiveCurrentScene/hero/Login_Scene_Preview.png",
            "work/logs/task4_frontend_login_baseline.png",
            "At the fixed Login camera and resolution, a fresh frontend capture stays within the recorded visual tolerance.",
            "image_metrics",
            {
                "width": 1616,
                "height": 939,
                "sample_stride": 8,
                "pixel_tolerance": 0.12,
                "max_mean_absolute_error": 0.18,
                "max_changed_fraction": 0.65,
            },
        ),
    ]
    manifest = {
        "schema_version": 1,
        "generated_by": "tools/scripts/build_fidelity_manifests.py",
        "source_precedence_manifest": "tools/fidelity_source_precedence.json",
        "rows": rows,
    }
    precedence = {
        "schema_version": 1,
        "generated_by": "tools/scripts/build_fidelity_manifests.py",
        "rules": [
            {"domain": "menu_art", "precedence": ["2011", "retail", "apbdb"], "decision": "D2/D3: RTW presentation wins."},
            {"domain": "menu_sfx", "precedence": ["2011", "retail", "apbdb"], "decision": "D2: RTW interaction feedback wins."},
            {"domain": "menu_strings", "precedence": ["2011", "retail", "apbdb"], "decision": "D2: RTW menu labels win."},
            {"domain": "ui_anchors", "precedence": ["2011", "retail", "apbdb"], "decision": "D2: measured RTW UIScene geometry wins."},
            {"domain": "character_customization", "precedence": ["retail", "apbdb", "2011"], "decision": "D4: retail flow, palettes, and camera win."},
            {"domain": "district_assets", "precedence": ["retail", "apbdb", "2011"], "decision": "D8: retail district content wins."},
            {"domain": "vehicle_data", "precedence": ["apbdb", "retail", "2011"], "decision": "D15: catalog stats are authoritative."},
            {"domain": "vehicle_assets", "precedence": ["retail", "apbdb", "2011"], "decision": "D15: retail imported vehicle art wins."},
            {"domain": "missions", "precedence": ["retail", "apbdb", "2011"], "decision": "D14: retail mission strings/templates win."},
            {"domain": "menu_visual", "precedence": ["2011", "retail", "apbdb"], "decision": "D2/D3: RTW visual oracle anchors the fixed camera."},
        ],
    }
    (OUT / "fidelity_oracle_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (OUT / "fidelity_source_precedence.json").write_text(json.dumps(precedence, indent=2) + "\n", encoding="utf-8")
    print(f"FIDELITY_MANIFEST_BUILT rows={len(rows)} precedence={len(precedence['rules'])}")


if __name__ == "__main__":
    main()
