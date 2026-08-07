from __future__ import annotations

from io import BytesIO
from pathlib import Path
import sys

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from compositor import composite_skin  # noqa: E402
from texture_resolver import find_default_textures  # noqa: E402

REPO_ROOT = Path(__file__).resolve().parents[4]
MESH = REPO_ROOT / "Content/Extracted/WeaponsBase/Weapon_AssaultRifle/Weapon_AssaultRifle/SkeletalMesh3/Weapon_AssaultRifle_LOD0.psk"
SKIN = "Weapon_AssaultRifle/MaterialInstanceConstant/Weapon_AssaultRifle_Bloodrose_MAT_INST.props.txt"


def _linear(channel: int) -> float:
    value = channel / 255.0
    return value / 12.92 if value <= 0.04045 else ((value + 0.055) / 1.055) ** 2.4


def test_bloodrose_pattern_cores_are_dark_red_in_linear_rgb() -> None:
    # Given
    textures = find_default_textures(MESH, skin=SKIN)
    # When
    png, _ = composite_skin(
        base_diffuse=textures["baseColor"], mask1=textures.get("mask1"), mask2=textures.get("mask2"),
        colors=textures.get("skin_colors"), stencil=textures.get("skin_stencil"), scalars=textures.get("skin_scalars"),
    )
    # Then
    with Image.open(BytesIO(png)).convert("RGBA") as output, Image.open(textures["mask1"]).convert("RGB") as mask, Image.open(textures["skin_stencil"]).convert("RGB") as stencil:
        mask = mask.resize(output.size, Image.NEAREST)
        stencil = stencil.resize(output.size, Image.BILINEAR)
        core = [tuple(_linear(value) for value in output.getpixel((x, y))[:3]) for y in range(output.height) for x in range(output.width) if output.getpixel((x, y))[3] > 0 and ((mask.getpixel((x, y))[1] >= 96) or (mask.getpixel((x, y))[0] >= 96 and mask.getpixel((x, y))[2] < 96)) and sum(_linear(value) for value in stencil.getpixel((x, y))) / 3 <= 0.10]
        valid = [output.getpixel((x, y)) for y in range(output.height) for x in range(output.width) if output.getpixel((x, y))[3] > 0]
    assert len(core) >= len(valid) * 0.0025
    mean = tuple(sum(pixel[index] for pixel in core) / len(core) for index in range(3))
    assert 0.10 <= mean[0] <= 0.40 and mean[0] >= 2.0 * max(mean[1:]) and mean[0] - max(mean[1:]) >= 0.08
    assert sum(pixel[0] >= 1.75 * max(pixel[1:]) and pixel[0] >= 0.08 for pixel in (tuple(_linear(value) for value in sample[:3]) for sample in valid)) >= len(valid) * 0.02


def test_composite_clothing() -> None:
    from compositor import composite_clothing
    import tempfile
    
    # Create dummy base image and mask
    with tempfile.TemporaryDirectory() as tmp:
        base_path = Path(tmp) / "base.tga"
        Image.new("RGBA", (32, 32), (128, 128, 128, 255)).save(base_path)
        
        mask_path = Path(tmp) / "mask.tga"
        Image.new("RGB", (32, 32), (255, 255, 255)).save(mask_path)
        
        dec_path = Path(tmp) / "decal.tga"
        Image.new("RGBA", (16, 16), (255, 0, 0, 255)).save(dec_path)

        png_bytes = composite_clothing(
            base_diffuse=base_path,
            colmasks={"0Base": mask_path},
            region_colors={"0Base": "#ff0000"},
            decals=[{"path": dec_path, "u": 0.5, "v": 0.5, "scale": 1.0, "rotation": 0.0}]
        )
        assert png_bytes is not None
        assert len(png_bytes) > 0
        
        with Image.open(BytesIO(png_bytes)) as result:
            assert result.size == (32, 32)
            assert result.mode == "RGBA"


def test_composite_body_overlay_falls_back_to_skin_under_black_overlay() -> None:
    from compositor import composite_body_overlay
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        base_path = Path(tmp) / "skin.tga"
        Image.new("RGBA", (32, 32), (200, 150, 120, 255)).save(base_path)

        overlay_path = Path(tmp) / "item.tga"
        overlay = Image.new("RGB", (32, 32), (0, 0, 0))
        overlay.paste((255, 40, 40), (16, 0, 32, 32))
        overlay.save(overlay_path)

        png_bytes = composite_body_overlay(base_path, overlay_path)
        with Image.open(BytesIO(png_bytes)) as result:
            assert result.size == (32, 32)
            assert result.mode == "RGBA"
            assert result.getpixel((4, 4))[:3] == (200, 150, 120)
            assert result.getpixel((20, 4))[:3] == (255, 40, 40)


def test_composite_body_overlay_uses_mask_source_for_coverage() -> None:
    from compositor import composite_body_overlay
    import tempfile

    with tempfile.TemporaryDirectory() as tmp:
        base_path = Path(tmp) / "skin.tga"
        Image.new("RGBA", (32, 32), (200, 150, 120, 255)).save(base_path)

        overlay_path = Path(tmp) / "item.tga"
        Image.new("RGB", (32, 32), (255, 40, 40)).save(overlay_path)

        mask_path = Path(tmp) / "diff.tga"
        mask = Image.new("RGB", (32, 32), (0, 0, 0))
        mask.paste((255, 255, 255), (0, 0, 16, 32))
        mask.save(mask_path)

        png_bytes = composite_body_overlay(base_path, overlay_path, mask_source=mask_path)
        with Image.open(BytesIO(png_bytes)) as result:
            assert result.getpixel((4, 4))[:3] == (255, 40, 40)
            assert result.getpixel((20, 4))[:3] == (200, 150, 120)
