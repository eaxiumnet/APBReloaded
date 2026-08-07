from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from inventory import build_asset_inventory
import main as main_module


def test_resolve_extracted_allows_in_root_and_blocks_escapes(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    root = tmp_path / "extracted"
    root.mkdir()
    (root / "ok.tga").write_bytes(b"tga")
    monkeypatch.setattr(main_module, "EXTRACTED_ROOT", root)
    resolved = main_module._resolve_extracted(str(root / "ok.tga"))
    assert resolved == (root / "ok.tga").resolve()
    with pytest.raises(ValueError):
        main_module._resolve_extracted("../../outside.tga")
    with pytest.raises(FileNotFoundError):
        main_module._resolve_extracted("missing.tga")


def test_texture_and_media_endpoints_serve_content(monkeypatch: pytest.MonkeyPatch, tmp_path: Path) -> None:
    pytest.importorskip("httpx")
    from fastapi.testclient import TestClient
    from PIL import Image

    root = tmp_path / "extracted"
    (root / "sub").mkdir(parents=True)
    tga = root / "sub" / "mat_Diff.tga"
    Image.new("RGB", (8, 8), (120, 90, 60)).save(tga, format="TGA")
    webm = root / "sub" / "clip.webm"
    webm.write_bytes(b"\x1aE\xdf\xa3 fake webm")
    monkeypatch.setattr(main_module, "EXTRACTED_ROOT", root)

    client = TestClient(main_module.app)
    png = client.get("/api/texture.png", params={"path": str(tga)})
    assert png.status_code == 200
    assert png.headers["content-type"] == "image/png"
    assert png.content[:8] == b"\x89PNG\r\n\x1a\n"
    media = client.get("/api/media", params={"path": str(webm)})
    assert media.status_code == 200
    assert media.headers["content-type"] == "video/webm"
    blocked = client.get("/api/media", params={"path": "../../nope.webm"})
    assert blocked.status_code == 400


def test_character_categorized_video_resolves_by_extension(tmp_path: Path) -> None:
    (tmp_path / "tools").mkdir()
    video = tmp_path / "Content" / "Extracted" / "2011" / "LoginAnimatedBackground_ai_upscale" / "01_Character_Select_BG_AI_compat.mp4"
    video.parent.mkdir(parents=True)
    video.write_bytes(b"mp4")
    (tmp_path / "tools" / "import_ledger.json").write_text(json.dumps({
        "entries": [{
            "asset_key": "01_Character_Select_BG_AI_compat.mp4",
            "dest": "/Game/Imported/Characters/LoginBG/01_Character_Select_BG_AI_compat.mp4",
            "status": "imported",
            "source_build": "2011",
        }]
    }), encoding="utf-8")
    result = build_asset_inventory(tmp_path)
    asset = result["assets"][0]
    assert asset["category"] == "characters"
    assert asset["preview_kind"] == "video"
    assert asset["preview_path"].endswith("01_Character_Select_BG_AI_compat.mp4")
