#!/usr/bin/env python3
"""Extract 2011 RTW login-era assets into the rebuild workspace.

- Copy movies/splash/config/localization snippets
- Extract login theme from 2011 StreamedSFX.pck (or Music.pck) via Wwise id + vgmstream
- Attempt umodel export of FrontEnd / GameFlowScenes UI packages
- Document provenance vs live Steam extracts
"""
from __future__ import annotations

import hashlib
import json
import shutil
import struct
import subprocess
import sys
import time
from pathlib import Path

RTW = Path(r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America")
OUT = Path(r"D:\APBReloaded\Content\Extracted\2011_rtw")
UI_OUT = OUT / "UI"
AUDIO_OUT = OUT / "Audio"
CFG_OUT = OUT / "Config"
MOV_OUT = OUT / "Movies"
LOC_OUT = OUT / "Localization"
SPLASH_OUT = OUT / "Splash"
CONTENT_AUDIO = Path(r"D:\APBReloaded\Content\Audio")
CONTENT_UI = Path(r"D:\APBReloaded\Content\Extracted\UI\2011_rtw")
VGM = Path(r"D:\APBReloaded\tools\WwiseExtract\vgmstream\vgmstream-cli.exe")
UMODEL = Path(r"D:\APBReloaded\tools\UEViewer\umodel_64.exe")
if not UMODEL.is_file():
    UMODEL = Path(r"D:\APBReloaded\tools\UEViewer\umodel.exe")

# Same short_id as live Steam extract (Play_NewThemeMusic / APB_ThemePreMaster)
THEME_ID = 540780953
THEME_NAME = "APB_ThemePreMaster"


def sha256_file(p: Path, limit: int | None = None) -> str:
    h = hashlib.sha256()
    with p.open("rb") as f:
        if limit:
            h.update(f.read(limit))
        else:
            for chunk in iter(lambda: f.read(1 << 20), b""):
                h.update(chunk)
    return h.hexdigest()


def parse_sounds_lut(data: bytes) -> list[tuple[int, int, int, int, int]]:
    if data[:4] != b"AKPK":
        return []
    lang_size, banks_size, sounds_size, _ext = struct.unpack_from("<IIII", data, 12)
    off = 28 + lang_size + banks_size
    if off + 4 > len(data):
        return []
    count = struct.unpack_from("<I", data, off)[0]
    base = off + 4
    entries = []
    for i in range(count):
        e = base + i * 20
        if e + 20 > len(data):
            break
        entries.append(struct.unpack_from("<IIIII", data, e))
    return entries


def extract_theme_from_pck(pck: Path, dest_wav: Path, dest_wem: Path) -> dict:
    info: dict = {"pck": str(pck), "ok": False}
    if not pck.is_file():
        info["error"] = "missing_pck"
        return info
    data = pck.read_bytes()
    found = {e[0]: e for e in parse_sounds_lut(data)}
    info["sound_entries"] = len(found)
    if THEME_ID not in found:
        # fallback: scan for any large wem-like slice named in bank text — keep honest
        info["error"] = f"theme_id_{THEME_ID}_not_in_lut"
        info["sample_ids"] = list(found.keys())[:20]
        return info
    _id, _f1, size, offset, _f4 = found[THEME_ID]
    blob = data[offset : offset + size]
    dest_wem.parent.mkdir(parents=True, exist_ok=True)
    dest_wem.write_bytes(blob)
    info["wem_bytes"] = len(blob)
    info["wem_path"] = str(dest_wem)
    if not VGM.is_file():
        info["error"] = "missing_vgmstream"
        return info
    dest_wav.parent.mkdir(parents=True, exist_ok=True)
    r = subprocess.run(
        [str(VGM), "-o", str(dest_wav), str(dest_wem)],
        capture_output=True,
        text=True,
    )
    if r.returncode != 0 or not dest_wav.is_file():
        info["error"] = "vgmstream_fail"
        info["stderr"] = (r.stderr or "")[-400:]
        return info
    info["ok"] = True
    info["wav_bytes"] = dest_wav.stat().st_size
    info["wav_path"] = str(dest_wav)
    info["sha256"] = sha256_file(dest_wav)
    return info


def copy_tree_files(pairs: list[tuple[Path, Path]]) -> list[dict]:
    out = []
    for src, dst in pairs:
        rec = {"src": str(src), "dst": str(dst), "ok": False}
        if not src.is_file():
            rec["error"] = "missing"
            out.append(rec)
            continue
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, dst)
        rec["ok"] = True
        rec["size"] = dst.stat().st_size
        out.append(rec)
    return out


def try_umodel(package_name: str, path: Path, out_dir: Path) -> dict:
    rec = {"package": package_name, "ok": False}
    if not UMODEL.is_file():
        rec["error"] = "umodel_missing"
        return rec
    out_dir.mkdir(parents=True, exist_ok=True)
    args = [
        str(UMODEL),
        f"-path={path}",
        "-game=apb",
        "-export",
        f"-out={out_dir}",
        package_name,
    ]
    r = subprocess.run(args, capture_output=True, text=True, timeout=180)
    rec["exit"] = r.returncode
    rec["stdout_tail"] = (r.stdout or "")[-500:]
    rec["stderr_tail"] = (r.stderr or "")[-500:]
    # count exported textures
    tgas = list(out_dir.rglob("*.tga"))
    rec["tga_count"] = len(tgas)
    rec["ok"] = r.returncode == 0 and len(tgas) > 0
    if not rec["ok"] and r.returncode == 0:
        rec["ok"] = len(list(out_dir.rglob("*"))) > 2
    return rec


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    report: dict = {"started": time.time(), "steps": {}}

    # 1) Config / movies / splash / localization
    pairs = [
        (RTW / "APBGame/Config/APBCompat_APBLoginLevel.ini", CFG_OUT / "APBCompat_APBLoginLevel.ini"),
        (RTW / "APBGame/Config/APBCompat_Dev_APBLoginLevel.ini", CFG_OUT / "APBCompat_Dev_APBLoginLevel.ini"),
        (RTW / "APBGame/Config/APBUI.ini", CFG_OUT / "APBUI.ini"),
        (RTW / "APBGame/Config/DefaultUI.ini", CFG_OUT / "DefaultUI.ini"),
        (RTW / "APBGame/Config/APBEngine.ini", CFG_OUT / "APBEngine.ini"),
        (RTW / "APBGame/Config/APBGame.ini", CFG_OUT / "APBGame.ini"),
        (RTW / "APBGame/Movies/SplashScreen.bik", MOV_OUT / "SplashScreen.bik"),
        (RTW / "APBGame/Movies/IntroTitles.bik", MOV_OUT / "IntroTitles.bik"),
        (RTW / "APBGame/Movies/LoadingMovieV1.bik", MOV_OUT / "LoadingMovieV1.bik"),
        (RTW / "APBGame/Splash/PC/Splash.bmp", SPLASH_OUT / "Splash.bmp"),
        (RTW / "APBGame/Splash/PC/EdSplash.bmp", SPLASH_OUT / "EdSplash.bmp"),
        (RTW / "Binaries/client.config", CFG_OUT / "client.config"),
        (RTW / "Launcher/launcher.config", CFG_OUT / "launcher.config"),
        (RTW / "Launcher/environment.config", CFG_OUT / "environment.config"),
    ]
    # copy a few INT files that often hold UI strings
    loc_root = RTW / "APBGame/Localization/INT"
    if loc_root.is_dir():
        for name in (
            "APBGame.int",
            "APBUI.int",
            "UIChat.int",
            "Engine.int",
            "UI.int",
        ):
            src = loc_root / name
            if not src.is_file():
                # case variants
                for alt in loc_root.glob(name.replace(".int", ".*")):
                    src = alt
                    break
            if src.is_file():
                pairs.append((src, LOC_OUT / src.name))

    report["steps"]["copy_config_movies"] = copy_tree_files(pairs)

    # 2) Theme audio from 2011 packs
    theme_wav = AUDIO_OUT / "LoginTheme_APB_ThemePreMaster_2011.wav"
    theme_wem = AUDIO_OUT / f"{THEME_ID}_{THEME_NAME}_2011.wem"
    pcks = [
        RTW / "APBGame/Content/Audio/FilePackages/StreamedSFX.pck",
        RTW / "APBGame/Content/Audio/FilePackages/Music.pck",
    ]
    theme_results = []
    for pck in pcks:
        res = extract_theme_from_pck(pck, theme_wav, theme_wem)
        theme_results.append(res)
        if res.get("ok"):
            break
    report["steps"]["theme_extract"] = theme_results

    # Promote to Content/Audio with provenance sidecar if 2011 decode succeeded
    live_theme = CONTENT_AUDIO / "LoginTheme_APB_ThemePreMaster.wav"
    provenance = {
        "asset": "LoginTheme_APB_ThemePreMaster",
        "wwise_short_id": THEME_ID,
        "role": "login_background_theme",
    }
    if theme_wav.is_file() and theme_wav.stat().st_size > 1000:
        dest = CONTENT_AUDIO / "LoginTheme_APB_ThemePreMaster.wav"
        CONTENT_AUDIO.mkdir(parents=True, exist_ok=True)
        shutil.copy2(theme_wav, dest)
        # also keep explicit 2011-named copy
        shutil.copy2(theme_wav, CONTENT_AUDIO / "LoginTheme_APB_ThemePreMaster_2011.wav")
        provenance["primary_source"] = "2011_rtw_StreamedSFX_or_Music_pck"
        provenance["dest"] = str(dest)
        provenance["bytes"] = dest.stat().st_size
        provenance["sha256"] = sha256_file(dest)
        if live_theme.is_file():
            provenance["previous_live_sha256_if_replaced"] = "see git/history"
    else:
        # Document fallback: existing live extract remains authoritative
        provenance["primary_source"] = "fallback_existing_Content_Audio"
        provenance["note"] = (
            "2011 pck did not yield ThemePreMaster via short_id; "
            "Content/Audio/LoginTheme_APB_ThemePreMaster.wav retained from Steam Wwise extract "
            "(same event Play_NewThemeMusic / APB_ThemePreMaster)."
        )
        if live_theme.is_file():
            provenance["bytes"] = live_theme.stat().st_size
            provenance["sha256"] = sha256_file(live_theme)
            # Mirror into 2011_rtw Audio as documented copy for inventory completeness
            AUDIO_OUT.mkdir(parents=True, exist_ok=True)
            documented = AUDIO_OUT / "LoginTheme_APB_ThemePreMaster_from_live_steam.wav"
            shutil.copy2(live_theme, documented)
            provenance["documented_copy"] = str(documented)
    (OUT / "login_theme_provenance.json").write_text(json.dumps(provenance, indent=2), encoding="utf-8")
    report["steps"]["theme_provenance"] = provenance

    # 3) umodel UI packages from 2011 Interface folder
    iface = RTW / "APBGame/Content/Interface"
    umodel_out = UI_OUT / "umodel"
    umodel_results = []
    for pkg in (
        "APBMenus_FrontEnd",
        "APBMenus_Art_GameFlowScenes",
        "APBMenus_GameFlowScenes",
        "APBMenus_Art_BG_Materials",
        "APBMenus_Art",
    ):
        umodel_results.append(try_umodel(pkg, iface, umodel_out / pkg))
    report["steps"]["umodel"] = umodel_results

    # Collect any TGAs into Content/Extracted/UI/2011_rtw
    CONTENT_UI.mkdir(parents=True, exist_ok=True)
    copied_tex = 0
    for tga in umodel_out.rglob("*.tga"):
        rel = tga.relative_to(umodel_out)
        dst = CONTENT_UI / rel
        dst.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(tga, dst)
        copied_tex += 1
    # If umodel failed, still copy splash bmp as login visual anchor + any existing scene_background
    existing_ui = Path(r"D:\APBReloaded\Content\Extracted\UI\ui_frontend_background")
    if copied_tex == 0 and (SPLASH_OUT / "Splash.bmp").is_file():
        shutil.copy2(SPLASH_OUT / "Splash.bmp", CONTENT_UI / "Splash_2011.bmp")
        copied_tex += 1
    if existing_ui.is_dir():
        # document path to already-extracted frontend backgrounds used by rebuild
        report["steps"]["existing_frontend_textures"] = str(existing_ui)
    report["steps"]["ui_textures_copied"] = copied_tex

    # 4) Raw package copies for offline research (not full dump of 50MB+ unless key)
    raw_pkg = OUT / "packages"
    raw_pkg.mkdir(parents=True, exist_ok=True)
    for name in ("APBMenus_FrontEnd.upk", "APBMenus_Art_BG_Materials.upk"):
        src = iface / name
        if src.is_file():
            shutil.copy2(src, raw_pkg / name)

    report["finished"] = time.time()
    report["elapsed_sec"] = round(report["finished"] - report["started"], 2)
    (OUT / "extract_2011_login_report.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=2)[:4000])
    print("OK extract report ->", OUT / "extract_2011_login_report.json")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
