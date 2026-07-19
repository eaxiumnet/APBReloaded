#!/usr/bin/env python3
"""Textured 3D model inspector for APB umodel PSK/PSKX + TGA exports.

Solid + diffuse texture + simple lighting (not wireframe-only).

  python view_models.py
  python view_models.py --headless
  python view_models.py --file path.psk
"""
from __future__ import annotations

import argparse
import ctypes
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from psk_reader import export_obj, index_models, load_psk, normalize_mesh  # noqa: E402

DEFAULT_ROOT = Path(r"D:\APBReloaded\Content\Extracted\UmodelExport")
SCRATCH_DEFAULT = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-4dca613c47ad\implementer")
OBJ_CACHE = Path(r"D:\APBReloaded\Content\Extracted\ModelViewerCache")


def load_texture_rgba(path: str | Path) -> tuple[np.ndarray, int, int] | None:
    if not path:
        return None
    p = Path(path)
    if not p.is_file():
        return None
    try:
        img = Image.open(p).convert("RGBA")
        # Power-of-two optional; GL accepts NPOT on modern
        arr = np.array(img, dtype=np.uint8)
        # Flip vertically for OpenGL
        arr = np.flipud(arr)
        h, w = arr.shape[0], arr.shape[1]
        return arr, w, h
    except Exception as e:
        print("texture load fail", p, e)
        return None


def run_headless(root: Path, scratch: Path, file: Path | None, max_load: int = 5) -> int:
    scratch.mkdir(parents=True, exist_ok=True)
    log_path = scratch / "viewer_run.log"
    lines: list[str] = []
    lines.append(f"viewer_start mode=textured root={root}")
    models = [file] if file else index_models(root, limit=200)
    lines.append(f"index_count={len(models) if not file else 1}")
    ok = 0
    for p in models[:max_load] if not file else models:
        try:
            mesh = load_psk(Path(p))
            normalize_mesh(mesh)
            tex_ok = bool(mesh.diffuse_path and Path(mesh.diffuse_path).is_file())
            obj = export_obj(mesh, OBJ_CACHE / f"{Path(p).stem}.obj")
            lines.append(
                f"LOAD_OK path={p} verts={len(mesh.vertices)} wedges={len(mesh.wedges)} "
                f"faces={len(mesh.faces)} tris={len(mesh.pos)//3} "
                f"diffuse={mesh.diffuse_path or 'NONE'} textured={int(tex_ok)} obj={obj}"
            )
            ok += 1
        except Exception as e:
            lines.append(f"LOAD_FAIL path={p} err={e}")
    lines.append(f"load_success={ok}")
    lines.append("viewer_mode=solid_textured")
    lines.append("viewer_headless_done")
    log_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 0 if ok > 0 else 1


def run_interactive(root: Path, file: Path | None) -> int:
    import pygame
    from pygame.locals import (
        DOUBLEBUF,
        OPENGL,
        QUIT,
        KEYDOWN,
        K_ESCAPE,
        K_LEFT,
        K_RIGHT,
        K_UP,
        K_DOWN,
        K_EQUALS,
        K_MINUS,
        K_n,
        K_p,
        K_w,
        K_t,
        K_r,
    )
    from OpenGL.GL import (
        GL_COLOR_BUFFER_BIT,
        GL_DEPTH_BUFFER_BIT,
        GL_DEPTH_TEST,
        GL_TRIANGLES,
        GL_FLOAT,
        GL_UNSIGNED_BYTE,
        GL_TEXTURE_2D,
        GL_TEXTURE_MIN_FILTER,
        GL_TEXTURE_MAG_FILTER,
        GL_LINEAR,
        GL_RGBA,
        GL_RGB,
        GL_LIGHTING,
        GL_LIGHT0,
        GL_COLOR_MATERIAL,
        GL_FRONT_AND_BACK,
        GL_AMBIENT_AND_DIFFUSE,
        GL_POSITION,
        GL_DIFFUSE,
        GL_AMBIENT,
        GL_NORMALIZE,
        GL_CULL_FACE,
        GL_BACK,
        GL_MODELVIEW,
        GL_PROJECTION,
        GL_VERTEX_ARRAY,
        GL_NORMAL_ARRAY,
        GL_TEXTURE_COORD_ARRAY,
        glEnable,
        glDisable,
        glClear,
        glClearColor,
        glViewport,
        glMatrixMode,
        glLoadIdentity,
        glRotatef,
        glTranslatef,
        glLightfv,
        glColorMaterial,
        glColor3f,
        glBindTexture,
        glGenTextures,
        glTexImage2D,
        glTexParameteri,
        glEnableClientState,
        glDisableClientState,
        glVertexPointer,
        glNormalPointer,
        glTexCoordPointer,
        glDrawArrays,
        glCullFace,
        glShadeModel,
        GL_SMOOTH,
    )
    from OpenGL.GLU import gluPerspective

    models = [file] if file else index_models(root, limit=3000)
    if not models:
        print("No PSK/PSKX under", root)
        return 1

    w, h = 1280, 720
    pygame.init()
    pygame.display.set_mode((w, h), DOUBLEBUF | OPENGL)
    pygame.display.set_caption(
        "APB Model Viewer — textured solid | N/P model | arrows orbit | +/- zoom | W wire | T texture | Esc"
    )

    glEnable(GL_DEPTH_TEST)
    glEnable(GL_NORMALIZE)
    glEnable(GL_COLOR_MATERIAL)
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE)
    glShadeModel(GL_SMOOTH)
    glEnable(GL_LIGHTING)
    glEnable(GL_LIGHT0)
    glLightfv(GL_LIGHT0, GL_POSITION, (2.5, 4.0, 3.0, 1.0))
    glLightfv(GL_LIGHT0, GL_DIFFUSE, (1.0, 1.0, 1.0, 1.0))
    glLightfv(GL_LIGHT0, GL_AMBIENT, (0.35, 0.38, 0.42, 1.0))
    glEnable(GL_CULL_FACE)
    glCullFace(GL_BACK)
    glClearColor(0.06, 0.08, 0.12, 1.0)

    glMatrixMode(GL_PROJECTION)
    glLoadIdentity()
    gluPerspective(45.0, w / float(h), 0.05, 50.0)
    glMatrixMode(GL_MODELVIEW)

    idx = 0
    ang_y, ang_x, zoom = 35.0, 20.0, -3.2
    wire = False
    use_tex = True
    tex_id = 0
    vbo_pos = None
    vbo_nrm = None
    vbo_uv = None
    tri_count = 0
    status = ""

    def upload_texture(path: str) -> int:
        data = load_texture_rgba(path)
        tid = int(glGenTextures(1))
        glBindTexture(GL_TEXTURE_2D, tid)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR)
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR)
        if data is None:
            checker = np.zeros((64, 64, 4), dtype=np.uint8)
            for y in range(64):
                for x in range(64):
                    c = 220 if ((x // 8) ^ (y // 8)) & 1 else 40
                    checker[y, x] = (c, c, c + 20, 255)
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, checker)
            return tid
        arr, tw, th = data
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, arr)
        return tid

    # Per-material draw ranges: list of (tex_id, start_vertex, count)
    draw_batches: list[tuple[int, int, int]] = []
    loaded_tex_ids: list[int] = []

    def clear_textures():
        nonlocal loaded_tex_ids
        if loaded_tex_ids:
            try:
                from OpenGL.GL import glDeleteTextures

                glDeleteTextures(loaded_tex_ids)
            except Exception:
                pass
        loaded_tex_ids = []

    def load_idx(i: int):
        nonlocal tex_id, vbo_pos, vbo_nrm, vbo_uv, tri_count, status, draw_batches
        path = Path(models[i])
        mesh = load_psk(path)
        normalize_mesh(mesh)
        export_obj(mesh, OBJ_CACHE / f"{path.stem}.obj")

        clear_textures()
        draw_batches = []
        # Primary diffuse
        primary = upload_texture(mesh.diffuse_path)
        loaded_tex_ids.append(primary)
        tex_id = primary

        if mesh.pos:
            vbo_pos = np.array(mesh.pos, dtype=np.float32).reshape(-1)
            vbo_nrm = np.array(mesh.norms, dtype=np.float32).reshape(-1)
            vbo_uv = np.array(mesh.uvs, dtype=np.float32).reshape(-1)
            tri_count = len(mesh.pos) // 3
            # One batch for full mesh with diffuse (multi-mat shares diffuse unless hair tex exists)
            if mesh.mat_tri_ranges:
                for mat_idx, start, count in mesh.mat_tri_ranges:
                    mat_name = (
                        mesh.materials[mat_idx].name.lower()
                        if mat_idx < len(mesh.materials)
                        else ""
                    )
                    tid = primary
                    if "hair" in mat_name:
                        # Prefer hair-named texture in same package
                        from psk_reader import find_package_textures

                        tmap = find_package_textures(path)
                        for k, tp in tmap.items():
                            if "hair" in k:
                                tid = upload_texture(str(tp))
                                loaded_tex_ids.append(tid)
                                break
                    draw_batches.append((tid, start, count))
            else:
                draw_batches.append((primary, 0, len(mesh.pos)))
        else:
            vbo_pos = np.zeros(0, dtype=np.float32)
            vbo_nrm = np.zeros(0, dtype=np.float32)
            vbo_uv = np.zeros(0, dtype=np.float32)
            tri_count = 0

        tex_name = Path(mesh.diffuse_path).name if mesh.diffuse_path else "NONE"
        status = (
            f"[{i+1}/{len(models)}] {path.name}  tris={tri_count}  "
            f"diffuse={tex_name}  wedges={len(mesh.wedges)}"
        )
        print(status, "path=", path)
        print("  texture=", mesh.diffuse_path or "(checker fallback)")
        return mesh

    load_idx(idx)
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas", 16)
    running = True
    auto_spin = True

    # Overlay via separate 2D surface blitted after? With OPENGL need pygame.font as texture or use simple caption.
    # Update window caption each frame instead of 2D overlay for simplicity.

    while running:
        for ev in pygame.event.get():
            if ev.type == QUIT:
                running = False
            elif ev.type == KEYDOWN:
                if ev.key == K_ESCAPE:
                    running = False
                elif ev.key == K_LEFT:
                    ang_y -= 8
                    auto_spin = False
                elif ev.key == K_RIGHT:
                    ang_y += 8
                    auto_spin = False
                elif ev.key == K_UP:
                    ang_x -= 6
                    auto_spin = False
                elif ev.key == K_DOWN:
                    ang_x += 6
                    auto_spin = False
                elif ev.key == K_EQUALS:
                    zoom += 0.25
                elif ev.key == K_MINUS:
                    zoom -= 0.25
                elif ev.key == K_n:
                    idx = (idx + 1) % len(models)
                    load_idx(idx)
                elif ev.key == K_p:
                    idx = (idx - 1) % len(models)
                    load_idx(idx)
                elif ev.key == K_w:
                    wire = not wire
                elif ev.key == K_t:
                    use_tex = not use_tex
                elif ev.key == K_r:
                    auto_spin = True

        if auto_spin:
            ang_y += 0.6

        glViewport(0, 0, w, h)
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glMatrixMode(GL_MODELVIEW)
        glLoadIdentity()
        glTranslatef(0.0, 0.0, zoom)
        glRotatef(ang_x, 1, 0, 0)
        glRotatef(ang_y, 0, 1, 0)

        if use_tex:
            glEnable(GL_TEXTURE_2D)
            glBindTexture(GL_TEXTURE_2D, tex_id)
        else:
            glDisable(GL_TEXTURE_2D)
            glColor3f(0.75, 0.82, 0.9)

        if wire:
            from OpenGL.GL import glPolygonMode, GL_LINE, GL_FILL, GL_FRONT_AND_BACK

            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)
            glDisable(GL_LIGHTING)
        else:
            from OpenGL.GL import glPolygonMode, GL_FILL, GL_FRONT_AND_BACK

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)
            glEnable(GL_LIGHTING)

        if tri_count > 0 and vbo_pos is not None and len(vbo_pos) > 0:
            glEnableClientState(GL_VERTEX_ARRAY)
            glEnableClientState(GL_NORMAL_ARRAY)
            glEnableClientState(GL_TEXTURE_COORD_ARRAY)
            glVertexPointer(3, GL_FLOAT, 0, vbo_pos)
            glNormalPointer(GL_FLOAT, 0, vbo_nrm)
            glTexCoordPointer(2, GL_FLOAT, 0, vbo_uv)
            if draw_batches:
                for tid, start, count in draw_batches:
                    if use_tex:
                        glBindTexture(GL_TEXTURE_2D, tid)
                    glDrawArrays(GL_TRIANGLES, start, count)
            else:
                glDrawArrays(GL_TRIANGLES, 0, tri_count * 3)
            glDisableClientState(GL_TEXTURE_COORD_ARRAY)
            glDisableClientState(GL_NORMAL_ARRAY)
            glDisableClientState(GL_VERTEX_ARRAY)

        pygame.display.set_caption(
            f"APB Textured Viewer | {status} | W=wire T=tex R=spin | tris={tri_count}"
        )
        pygame.display.flip()
        clock.tick(60)

    pygame.quit()
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="APB textured model viewer")
    ap.add_argument("--root", type=Path, default=DEFAULT_ROOT)
    ap.add_argument("--file", type=Path, default=None)
    ap.add_argument("--headless", action="store_true")
    ap.add_argument("--list-only", action="store_true")
    ap.add_argument("--scratch", type=Path, default=SCRATCH_DEFAULT)
    args = ap.parse_args()

    if args.list_only:
        models = index_models(args.root, limit=80)
        for m in models:
            print(m)
        print(f"count={len(models)}")
        return 0 if models else 1

    if args.headless:
        return run_headless(args.root, args.scratch, args.file)

    try:
        return run_interactive(args.root, args.file)
    except Exception as e:
        print("interactive failed, falling back headless:", e)
        import traceback

        traceback.print_exc()
        return run_headless(args.root, args.scratch, args.file)


if __name__ == "__main__":
    raise SystemExit(main())
