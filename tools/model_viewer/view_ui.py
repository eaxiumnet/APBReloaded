#!/usr/bin/env python3
"""Browse 2011 APB UI / main-menu TGA exports (2D texture gallery).

Menu UI is Scaleform/UI textures — not skeletal meshes. This viewer pages
TGA/PNG/BMP with filter and folder grouping.

  python view_ui.py
  python view_ui.py --root D:\\APBReloaded\\Content\\Extracted\\2011\\UI\\umodel
  python view_ui.py --filter Login
  python view_ui.py --filter GameFlow

Keys:
  Left/Right or A/D  prev/next image
  Up/Down or W/S     jump ±10
  Home / End         first / last
  F                  cycle filter presets (All / GameFlow / FrontEnd / Login / Faction / Art)
  G                  toggle fit vs 1:1 pixel
  T                  toggle filename overlay
  R                  random image
  E                  open containing folder in Explorer
  Q / Esc            quit
"""
from __future__ import annotations

import argparse
import os
import random
import subprocess
import sys
from pathlib import Path

from PIL import Image

DEFAULT_ROOTS = [
    Path(r"D:\APBReloaded\Content\Extracted\2011\UI\umodel"),
    Path(r"D:\APBReloaded\Content\Extracted\2011\UI\extra"),
    Path(r"D:\APBReloaded\Content\Extracted\2011\UI\loose"),
    Path(r"D:\APBReloaded\Content\Extracted\2011_rtw\UI\umodel"),
    Path(r"D:\APBReloaded\Content\Extracted\2011\probe_export"),
]

EXTS = {".tga", ".png", ".bmp", ".jpg", ".jpeg", ".dds"}

FILTER_PRESETS = [
    ("All", None),
    ("GameFlow", "gameflow"),
    ("FrontEnd", "frontend"),
    ("Login", "login"),
    ("Faction", "faction"),
    ("Character", "character"),
    ("District", "district"),
    ("HUD", "hud"),
    ("Art", "art"),
    ("Norm", "norm"),
]


def index_images(roots: list[Path], needle: str | None) -> list[Path]:
    found: list[Path] = []
    seen: set[str] = set()
    for root in roots:
        if not root.is_dir():
            continue
        for p in root.rglob("*"):
            if not p.is_file() or p.suffix.lower() not in EXTS:
                continue
            key = str(p.resolve()).lower()
            if key in seen:
                continue
            if needle:
                blob = str(p).lower().replace("\\", "/")
                if needle.lower() not in blob and needle.lower() not in p.stem.lower():
                    continue
            seen.add(key)
            found.append(p)
    found.sort(key=lambda x: str(x).lower())
    return found


def fit_size(iw: int, ih: int, max_w: int, max_h: int) -> tuple[int, int]:
    if iw <= 0 or ih <= 0:
        return max_w, max_h
    s = min(max_w / iw, max_h / ih, 1.0) if False else min(max_w / iw, max_h / ih)
    # allow upscale small icons a bit
    if s < 1.0 or (iw < 128 and ih < 128):
        s = min(max_w / iw, max_h / ih, 4.0)
    return max(1, int(iw * s)), max(1, int(ih * s))


def open_explorer(path: Path) -> None:
    folder = path if path.is_dir() else path.parent
    subprocess.Popen(["explorer", "/select,", str(path.resolve())] if path.is_file() else ["explorer", str(folder)])


def run(roots: list[Path], filter_text: str | None) -> int:
    import pygame

    pygame.init()
    pygame.display.set_caption("APB 2011 UI / Main Menu texture browser")
    screen = pygame.display.set_mode((1280, 800), pygame.RESIZABLE)
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas", 16)
    font_sm = pygame.font.SysFont("consolas", 13)

    preset_i = 0
    if filter_text:
        for i, (name, needle) in enumerate(FILTER_PRESETS):
            if filter_text.lower() in name.lower() or (needle and filter_text.lower() == needle):
                preset_i = i
                break
        else:
            FILTER_PRESETS.insert(0, (f"Custom:{filter_text}", filter_text))
            preset_i = 0

    def current_needle() -> str | None:
        return FILTER_PRESETS[preset_i][1]

    images = index_images(roots, current_needle())
    idx = 0
    fit_mode = True
    show_hud = True
    surf_cache: pygame.Surface | None = None
    cache_path: Path | None = None
    err = ""

    def load_current() -> None:
        nonlocal surf_cache, cache_path, err
        surf_cache = None
        cache_path = None
        err = ""
        if not images:
            err = "No images found under roots (run export_2011_ui.py first)"
            return
        path = images[idx]
        try:
            img = Image.open(path).convert("RGBA")
            mode = img.mode
            data = img.tobytes()
            size = img.size
            raw = pygame.image.fromstring(data, size, mode)
            surf_cache = raw.convert_alpha()
            cache_path = path
        except Exception as e:
            err = f"load fail: {path.name}: {e}"

    def reindex(keep_name: str | None = None) -> None:
        nonlocal images, idx
        old = keep_name
        images = index_images(roots, current_needle())
        idx = 0
        if old:
            for i, p in enumerate(images):
                if p.name == old or str(p).endswith(old):
                    idx = i
                    break
        load_current()

    load_current()

    running = True
    while running:
        for ev in pygame.event.get():
            if ev.type == pygame.QUIT:
                running = False
            elif ev.type == pygame.VIDEORESIZE:
                screen = pygame.display.set_mode(ev.size, pygame.RESIZABLE)
            elif ev.type == pygame.KEYDOWN:
                if ev.key in (pygame.K_ESCAPE, pygame.K_q):
                    running = False
                elif ev.key in (pygame.K_RIGHT, pygame.K_d) and images:
                    idx = (idx + 1) % len(images)
                    load_current()
                elif ev.key in (pygame.K_LEFT, pygame.K_a) and images:
                    idx = (idx - 1) % len(images)
                    load_current()
                elif ev.key in (pygame.K_DOWN, pygame.K_s) and images:
                    idx = (idx + 10) % len(images)
                    load_current()
                elif ev.key in (pygame.K_UP, pygame.K_w) and images:
                    idx = (idx - 10) % len(images)
                    load_current()
                elif ev.key == pygame.K_HOME and images:
                    idx = 0
                    load_current()
                elif ev.key == pygame.K_END and images:
                    idx = len(images) - 1
                    load_current()
                elif ev.key == pygame.K_f:
                    preset_i = (preset_i + 1) % len(FILTER_PRESETS)
                    keep = images[idx].name if images else None
                    reindex(keep)
                elif ev.key == pygame.K_g:
                    fit_mode = not fit_mode
                elif ev.key == pygame.K_t:
                    show_hud = not show_hud
                elif ev.key == pygame.K_r and images:
                    idx = random.randrange(len(images))
                    load_current()
                elif ev.key == pygame.K_e and images:
                    open_explorer(images[idx])

        screen.fill((24, 26, 32))
        sw, sh = screen.get_size()
        margin = 48 if show_hud else 8

        if surf_cache is not None:
            iw, ih = surf_cache.get_size()
            if fit_mode:
                dw, dh = fit_size(iw, ih, sw - margin * 2, sh - margin * 2 - 40)
                drawn = pygame.transform.smoothscale(surf_cache, (dw, dh))
            else:
                drawn = surf_cache
                dw, dh = iw, ih
            x = (sw - dw) // 2
            y = (sh - dh) // 2 + (10 if show_hud else 0)
            # checkerboard for alpha
            tile = 16
            for cy in range(y, y + dh, tile):
                for cx in range(x, x + dw, tile):
                    c = (50, 52, 58) if ((cx // tile) + (cy // tile)) % 2 == 0 else (38, 40, 46)
                    pygame.draw.rect(
                        screen,
                        c,
                        pygame.Rect(cx, cy, min(tile, x + dw - cx), min(tile, y + dh - cy)),
                    )
            screen.blit(drawn, (x, y))
        elif err:
            t = font.render(err, True, (255, 120, 120))
            screen.blit(t, (20, sh // 2))

        if show_hud:
            preset_name = FILTER_PRESETS[preset_i][0]
            if images and cache_path:
                rel = cache_path
                for r in roots:
                    try:
                        rel = cache_path.relative_to(r)
                        break
                    except ValueError:
                        pass
                line1 = f"[{idx + 1}/{len(images)}] {rel}"
                line2 = f"filter={preset_name}  fit={'on' if fit_mode else '1:1'}  size={surf_cache.get_size() if surf_cache else '-'}"
            else:
                line1 = f"[0/0] filter={preset_name}"
                line2 = "No images — run: python tools/scripts/export_2011_ui.py"
            help_line = "←/→ nav  ↑/↓ ±10  F filter  G fit  R random  E explorer  Q quit"
            # backdrop
            pygame.draw.rect(screen, (0, 0, 0, 180), pygame.Rect(0, 0, sw, 44))
            pygame.draw.rect(screen, (0, 0, 0, 180), pygame.Rect(0, sh - 28, sw, 28))
            screen.blit(font.render(line1[:140], True, (230, 230, 235)), (10, 6))
            screen.blit(font_sm.render(line2, True, (170, 175, 190)), (10, 24))
            screen.blit(font_sm.render(help_line, True, (140, 150, 170)), (10, sh - 22))

        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="APB 2011 UI texture browser")
    ap.add_argument("--root", type=Path, action="append", default=None, help="Search root (repeatable)")
    ap.add_argument("--filter", type=str, default=None, help="Name filter (e.g. Login, GameFlow)")
    ap.add_argument("--list-only", action="store_true")
    args = ap.parse_args()

    roots = args.root if args.root else [r for r in DEFAULT_ROOTS if r.is_dir()]
    if not roots:
        roots = DEFAULT_ROOTS
        print("WARN: no existing roots yet; defaulting to", roots[0])

    if args.list_only:
        imgs = index_images(roots, args.filter)
        for p in imgs[:100]:
            print(p)
        print(f"count={len(imgs)}")
        return 0 if imgs else 1

    return run(roots, args.filter)


if __name__ == "__main__":
    raise SystemExit(main())
