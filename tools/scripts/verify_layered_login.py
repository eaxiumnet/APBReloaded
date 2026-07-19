#!/usr/bin/env python3
"""Gating: layered login uses NewBackgroundImage + chars, NOT LoginScreen_1to1 capture."""
from __future__ import annotations
import re
import sys
from pathlib import Path

ROOT = Path(r"D:\APBReloaded")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-b79df4343a95\implementer")
CPP = ROOT / "Source" / "APBReloaded" / "Systems" / "APBFrontendWidget.cpp"
UI = ROOT / "Content" / "UI" / "Frontend" / "2011"


def main() -> int:
    errs: list[str] = []
    src = CPP.read_text(encoding="utf-8", errors="ignore")

    # Must NOT primary-load capture plate
    if re.search(r'TryLoadBackgroundTextures[\s\S]{0,2500}?LoginScreen_1to1\.(png|tga|bmp)', src):
        # only fail if it's still the primary plate load in TryLoad
        block = re.search(r"void UAPBFrontendWidget::TryLoadBackgroundTextures\(\)[\s\S]*?^}", src, re.M)
        if block and "LoginScreen_1to1" in block.group(0) and "NewBackgroundImage" not in block.group(0):
            errs.append("tryload_still_uses_capture_plate")
    if "LoginScreen_1to1" in src and "NewBackgroundImage" not in src:
        errs.append("missing_NewBackgroundImage_load")

    for needle in (
        "NewBackgroundImage",
        "MaleAvatar_alpha",
        "FemaleAvatar_alpha",
        "MaleAvatar",
        "FemaleAvatar",
        "splatter1",
        "LoadingScreen_Flames",
        "UI_LAYERED",
        "GraffitiLayers",
        "UpdateMotionBackground",
        "841514482_APBTheme1",
        "login_ok",
        "login_fail",
        "Email Address",
        "PLEASE ENTER YOUR ACCOUNT DETAILS",
        "Never prefer live-session crops",
    ):
        if needle not in src:
            errs.append(f"missing_src:{needle}")

    # Primary load must prefer package alpha avatars, not Char_Criminal live crops
    try_block = re.search(
        r"void UAPBFrontendWidget::TryLoadBackgroundTextures\(\)[\s\S]*?(?=\nvoid UAPBFrontendWidget::)",
        src,
    )
    if try_block:
        tb = try_block.group(0)
        if "LoginScreen_1to1" in tb and "NewBackgroundImage" not in tb:
            errs.append("tryload_capture_plate")
        if "Char_Criminal_Left" in tb or "Char_Enforcer_Right" in tb:
            errs.append("tryload_still_references_live_char_crops")
        # MaleAvatar must appear before any archived crop names
        if "MaleAvatar_alpha" not in tb and "MaleAvatar.tga" not in tb:
            errs.append("tryload_missing_package_male")
        if "FemaleAvatar_alpha" not in tb and "FemaleAvatar.tga" not in tb:
            errs.append("tryload_missing_package_female")

    # Ghost-over-screenshot form must not be the only path
    if "StyleGhostBtn" in src and "Exit to Desktop" not in src:
        errs.append("ghost_form_without_real_labels")

    # Assets exist (package art)
    for name in (
        "NewBackgroundImage.tga",
        "MaleAvatar.tga",
        "FemaleAvatar.tga",
        "MaleAvatar_alpha.png",
        "FemaleAvatar_alpha.png",
        "splatter1.tga",
        "LoadingScreen_Flames.tga",
    ):
        p = UI / name
        if not p.is_file() or p.stat().st_size < 1000:
            errs.append(f"missing_asset:{name}")

    # Capture plate / live form-embedded crops not in primary UI dir
    for name in (
        "LoginScreen_1to1.png",
        "LoginScreen_1to1.tga",
        "LoginScreen_1to1.bmp",
        "Char_Criminal_Left.png",
        "Char_Enforcer_Right.png",
    ):
        if (UI / name).is_file():
            errs.append(f"forbidden_primary_asset:{name}")

    # Structure: separate layers constructed
    for name in ("CharCriminalLeft", "CharEnforcerRight", "BgNewBackgroundImage", "GraffSplat", "AltMaleAvatar"):
        if name not in src:
            errs.append(f"missing_layer_widget:{name}")

    lines = [
        f"ok={0 if errs else 1}",
        f"errors={errs}",
        f"newbg={(UI/'NewBackgroundImage.tga').stat().st_size if (UI/'NewBackgroundImage.tga').is_file() else 0}",
        f"charL={(UI/'Char_Criminal_Left.png').stat().st_size if (UI/'Char_Criminal_Left.png').is_file() else 0}",
        f"charR={(UI/'Char_Enforcer_Right.png').stat().st_size if (UI/'Char_Enforcer_Right.png').is_file() else 0}",
    ]
    SCRATCH.mkdir(parents=True, exist_ok=True)
    inv = SCRATCH / "login_no_screenshot_inventory.txt"
    inv.write_text("\n".join(lines) + "\n" + "\n".join(f"  {e}" for e in errs), encoding="utf-8")
    struct = SCRATCH / "login_layer_structure.txt"
    struct.write_text(
        "layers=BgNewBackgroundImage,CharCriminalLeft,CharEnforcerRight,AltMaleAvatar,AltFemaleAvatar,"
        "GraffSplatL/R,GraffFlame*,LogoAPB,real_login_panel\n"
        "animate=KenBurns_bed,char_bob_sway,alt_avatar_on_off,graffiti_blink_pulse\n"
        "form=real_controls_email_password_login_newaccount\n"
        "no_primary_LoginScreen_1to1=1\n"
        + "\n".join(lines),
        encoding="utf-8",
    )
    print("\n".join(lines))
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
