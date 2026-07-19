#!/usr/bin/env python3
"""Gating structural checks for 2011 extract + classic login + emulator research note."""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(r"D:\APBReloaded")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-05e2326e6a85\implementer")
EXTRACT = ROOT / "Content" / "Extracted" / "2011_rtw"
THEME = ROOT / "Content" / "Audio" / "LoginTheme_APB_ThemePreMaster.wav"
RESEARCH = ROOT / "tools" / "apb_sdk_ref" / "EMULATOR_RESEARCH_AND_SERVER_ROLES.md"
CPP = ROOT / "Source" / "APBReloaded" / "Systems" / "APBFrontendWidget.cpp"
PROBE = ROOT / "Source" / "APBReloaded" / "Systems" / "APBSessionProbeSubsystem.cpp"
DOMAIN_TEST = ROOT / "tests" / "run_domain_tests.cpp"


def main() -> int:
    errs: list[str] = []
    # Inventory machine-readable
    for name in (
        "inventory_2011_summary.json",
        "inventory_2011_focus.csv",
        "inventory_2011_listing.txt",
        "inventory_2011_key_assets.json",
        "extract_2011_login_report.json",
        "login_theme_provenance.json",
    ):
        p = EXTRACT / name
        if not p.is_file() or p.stat().st_size < 50:
            errs.append(f"missing_inventory:{name}")

    # Login-era extracted artifacts
    for rel in (
        "Splash/Splash.bmp",
        "Movies/SplashScreen.bik",
        "Config/APBCompat_APBLoginLevel.ini",
        "Config/APBUI.ini",
        "packages/APBMenus_FrontEnd.upk",
    ):
        p = EXTRACT / rel
        if not p.is_file() or p.stat().st_size < 100:
            errs.append(f"missing_extract:{rel}")

    tga = list((EXTRACT / "UI" / "umodel").rglob("*.tga")) if (EXTRACT / "UI").is_dir() else []
    if len(tga) < 20:
        errs.append(f"ui_tga_too_few={len(tga)}")

    if not THEME.is_file() or THEME.stat().st_size < 100_000:
        errs.append("theme_missing_or_tiny")

    prov = (EXTRACT / "login_theme_provenance.json").read_text(encoding="utf-8", errors="ignore")
    if "APB_ThemePreMaster" not in prov and "ThemePreMaster" not in prov:
        errs.append("provenance_missing_theme")

    # Frontend wiring
    src = CPP.read_text(encoding="utf-8", errors="ignore")
    for needle in (
        "LoginTheme_APB_ThemePreMaster",
        "login_ok",
        "login_fail",
        "SetLoginCredentials",
        "Classic RTW",
    ):
        if needle not in src:
            errs.append(f"frontend_missing:{needle}")
    # no auto-register on login click
    login_fn = re.search(r"OnLoginClicked\(\)[\s\S]{0,800}?OnRegisterClicked", src)
    # simpler: ensure RegisterAccount is not called inside OnLoginClicked body
    m = re.search(r"void UAPBFrontendWidget::OnLoginClicked\(\)\s*\{([\s\S]*?)\n\}", src)
    if m and "RegisterAccount" in m.group(1):
        errs.append("login_still_auto_registers")

    probe = PROBE.read_text(encoding="utf-8", errors="ignore")
    if "login_fail" not in probe or "login_ok" not in probe:
        errs.append("probe_missing_login_ok_fail")

    dt = DOMAIN_TEST.read_text(encoding="utf-8", errors="ignore")
    if "TestLoginSuccessAndFail" not in dt or "login_fail bad password" not in dt:
        errs.append("domain_test_missing_login_fail")

    # Research note
    if not RESEARCH.is_file():
        errs.append("research_missing")
    else:
        text = RESEARCH.read_text(encoding="utf-8", errors="ignore")
        urls = re.findall(r"https?://[^\s\)]+", text)
        if len(urls) < 3:
            errs.append(f"research_urls_lt3={len(urls)}")
        for must in (
            "ivan-draga/rAPB",
            "ragezone.com",
            "ApbPrivateServer",
            "ASK_LOGIN",
            "District",
            "FURTHER",
            "server-role",
        ):
            if must.lower() not in text.lower() and must not in text:
                # case-insensitive check
                if must.lower() not in text.lower():
                    errs.append(f"research_missing:{must}")

    lines = [
        f"ok={0 if errs else 1}",
        f"tga_count={len(tga)}",
        f"theme_bytes={THEME.stat().st_size if THEME.is_file() else 0}",
        f"research_bytes={RESEARCH.stat().st_size if RESEARCH.is_file() else 0}",
        f"errors={errs}",
    ]
    SCRATCH.mkdir(parents=True, exist_ok=True)
    out = SCRATCH / "static_verify_2011_goal.txt"
    out.write_text("\n".join(lines) + "\n", encoding="utf-8")
    print("\n".join(lines))
    return 1 if errs else 0


if __name__ == "__main__":
    raise SystemExit(main())
