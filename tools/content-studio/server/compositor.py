"""Linear-RGB per-zone compositor for APB weapon material instances."""

from __future__ import annotations

import math
from io import BytesIO
from pathlib import Path
from typing import Mapping

from PIL import Image, ImageFilter

from material_instance import Color

Color3 = tuple[float, float, float]
_MASK_THRESHOLD = 96
_LUMA = (0.2126, 0.7152, 0.0722)


def _clamp(value: float) -> float:
    return max(0.0, min(1.0, value))


def _srgb_to_linear(value: int) -> float:
    normalized = value / 255.0
    return normalized / 12.92 if normalized <= 0.04045 else ((normalized + 0.055) / 1.055) ** 2.4


def _linear_to_srgb(value: float) -> int:
    linear = _clamp(value)
    encoded = linear * 12.92 if linear <= 0.0031308 else 1.055 * linear ** (1 / 2.4) - 0.055
    return round(_clamp(encoded) * 255.0)


def _luma(color: Color3) -> float:
    return sum(component * weight for component, weight in zip(color, _LUMA))


def _rgb(color: Color | None) -> Color3 | None:
    return color[:3] if color is not None else None


def _png(image: Image.Image) -> bytes:
    with BytesIO() as output:
        image.save(output, format="PNG")
        return output.getvalue()


def _load(path: Path | None, size: tuple[int, int] | None, resample: Image.Resampling) -> Image.Image | None:
    if path is None or not path.is_file():
        return None
    try:
        with Image.open(path) as source:
            image = source.convert("RGB")
    except (OSError, ValueError):
        return None
    return image.resize(size, resample) if size is not None and image.size != size else image


def _detail(base: Image.Image) -> Image.Image:
    luma = Image.new("L", base.size)
    luma.putdata([round(_luma(tuple(_srgb_to_linear(v) for v in pixel)) * 255) for pixel in base.getdata()])
    radius = max(1, round(max(base.size) / 256))
    return luma.filter(ImageFilter.GaussianBlur(radius))


def _color(colors: Mapping[str, Color], name: str) -> Color3 | None:
    return _rgb(colors.get(name))


def _pattern_color(
    family: str, colors: Mapping[str, Color], stencil_luma: float | None
) -> Color3 | None:
    first = _color(colors, f"{family} Pattern Col A")
    second = _color(colors, f"{family} Pattern Col B")
    if first is not None and second is not None and stencil_luma is not None:
        tint = _color(colors, "Pattern1 Colour Tint") or (1.0, 1.0, 1.0)
        return tuple((first[i] * (1.0 - stencil_luma) + second[i] * stencil_luma) * tint[i] for i in range(3))
    fallback = _color(colors, f"{family} Colour")
    if stencil_luma is None and fallback is not None:
        return fallback
    if first is not None:
        return first
    if second is not None:
        return second
    fallbacks = {"Body": ("Body Colour", "Base Colour"), "Stock": ("Stock Colour",), "Handle": ("Handle Colour",)}
    return next((color for key in fallbacks[family] if (color := _color(colors, key)) is not None), None)


def _scalar(scalars: Mapping[str, float], name: str) -> float | None:
    target = name.casefold()
    return next((value for key, value in scalars.items() if key.casefold() == target), None)


def _stencil_luma(
    stencil: Image.Image | None, x: int, y: int, size: tuple[int, int], family: str, scalars: Mapping[str, float]
) -> float | None:
    if stencil is None:
        return None
    key = "Body Pattern" if family == "Body" else family
    divide_x = _scalar(scalars, f"Divide {key} X") or 1.0
    divide_y = _scalar(scalars, f"Divide {key} Y") or 1.0
    nudge_x = _scalar(scalars, f"Nudge {key} X") or 0.0
    nudge_y = _scalar(scalars, f"Nudge {key} Y") or 0.0
    degrees = _scalar(scalars, f"Rotate {family}") or 0.0
    u, v = (x / size[0]) / divide_x + nudge_x, (y / size[1]) / divide_y + nudge_y
    radians = math.radians(degrees)
    u, v = (u - 0.5) * math.cos(radians) - (v - 0.5) * math.sin(radians) + 0.5, (u - 0.5) * math.sin(radians) + (v - 0.5) * math.cos(radians) + 0.5
    return _luma(tuple(_srgb_to_linear(value) for value in _bilinear_wrapped(stencil, u, v)))


def _bilinear_wrapped(image: Image.Image, u: float, v: float) -> tuple[int, int, int]:
    x, y = (u % 1.0) * image.width - 0.5, (v % 1.0) * image.height - 0.5
    x0, y0 = math.floor(x), math.floor(y)
    tx, ty = x - x0, y - y0
    samples = [image.getpixel(((x0 + dx) % image.width, (y0 + dy) % image.height)) for dy in (0, 1) for dx in (0, 1)]
    return tuple(round((samples[0][channel] * (1 - tx) + samples[1][channel] * tx) * (1 - ty) + (samples[2][channel] * (1 - tx) + samples[3][channel] * tx) * ty) for channel in range(3))


def _zone(mask1: Image.Image | None, mask2: Image.Image | None, x: int, y: int) -> str:
    first = mask1.getpixel((x, y)) if mask1 is not None else (0, 0, 0)
    second = mask2.getpixel((x, y)) if mask2 is not None else (0, 0, 0)
    r1, g1, b1 = (channel >= _MASK_THRESHOLD for channel in first)
    r2, g2, b2 = (channel >= _MASK_THRESHOLD for channel in second)
    if b2:
        return "Metal Chip"
    if r2:
        return "Stock Butt"
    if r1 and b1:
        return "Handle"
    if g1:
        return "Stock"
    if r1:
        return "Body"
    if b1:
        return "Handle"
    return "Secondary" if g2 else "Base"


def _zone_color(zone: str, colors: Mapping[str, Color], stencil_luma: float | None) -> Color3 | None:
    direct = {"Metal Chip": "Metal Chip Colour", "Stock Butt": "Stock Butt Colour", "Secondary": "Secondary Colour", "Base": "Base Colour"}
    return _color(colors, direct[zone]) if zone in direct else _pattern_color(zone, colors, stencil_luma)


def _composite_pixel(base: tuple[int, int, int, int], local_luma: int, color: Color3 | None) -> tuple[int, int, int, int]:
    source = tuple(_srgb_to_linear(channel) for channel in base[:3])
    if color is None:
        return base
    local = max(local_luma / 255.0, 0.02)
    factor = max(0.65, min(1.35, _luma(source) / local))
    return tuple(_linear_to_srgb(component * factor) for component in color) + (base[3],)


def composite_skin(
    base_diffuse: Path,
    mask1: Path | None = None,
    mask2: Path | None = None,
    colors: Mapping[str, Color] | None = None,
    stencil: Path | None = None,
    scalars: Mapping[str, float] | None = None,
) -> tuple[bytes, bytes | None]:
    """Apply authored categorical material zones to a base diffuse texture."""
    try:
        with Image.open(base_diffuse) as source:
            base = source.convert("RGBA")
    except (OSError, ValueError):
        base = Image.new("RGBA", (1, 1), (128, 128, 128, 255))
    if colors is None or (mask1 is None and mask2 is None):
        return _png(base), None
    first = _load(mask1, base.size, Image.Resampling.NEAREST)
    second = _load(mask2, base.size, Image.Resampling.NEAREST)
    if first is None and second is None:
        return _png(base), None
    pattern = _load(stencil, None, Image.Resampling.BILINEAR)
    detail = _detail(base.convert("RGB"))
    pixels = [
        _composite_pixel(
            base.getpixel((x, y)),
            detail.getpixel((x, y)),
            _zone_color(
                zone := _zone(first, second, x, y),
                colors,
                _stencil_luma(pattern, x, y, base.size, zone, scalars or {}),
            ),
        )
        for y in range(base.height)
        for x in range(base.width)
    ]
    result = Image.new("RGBA", base.size)
    result.putdata(pixels)
    return _png(result), None
