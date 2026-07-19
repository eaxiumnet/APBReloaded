#!/usr/bin/env python3
"""Full-extent inventory of the 2011 APB All Points Bulletin client tree.

Walks APBGame (+ Engine content), classifies extractable asset families,
reads UPK FileVersion/Licensee, optionally probes umodel list on representatives,
and writes report artifacts under Content/Extracted/2011/.

Run:
  python tools/scripts/inventory_2011_full.py
  python tools/scripts/inventory_2011_full.py --probe
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import struct
import subprocess
import sys
import time
from collections import Counter, defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
APB_NA = ROOT / "2011 apb" / "APB All Points Bulletin" / "APB North America"
APBGAME = APB_NA / "APBGame"
CONTENT = APBGAME / "Content"
OUT_DIR = ROOT / "Content" / "Extracted" / "2011"
UMODEL = ROOT / "tools" / "UEViewer" / "umodel_64.exe"
if not UMODEL.is_file():
    UMODEL = ROOT / "tools" / "UEViewer" / "umodel.exe"

STEAM_VER = (564, 33)
EXPECTED_2011 = (547, 31)

# Name / path classifiers for UPK families
WEAPON_RE = re.compile(
    r"(Weapon_|PlaceholderWeapon|/Weapons/|AmmoCarrier|Gun_Case|Holster|Gun1Handed|Gun2Handed|Animation_Weapon)",
    re.I,
)
CHARACTER_RE = re.compile(
    r"(Contact_|Character|Clothing|Body_|Hairwear|Chestwear|Legwear|Facewear|Eyewear|"
    r"APB_CharacterTool|Pedestrian|F_Body|M_Body|/LC/|/Contact/)",
    re.I,
)
VEHICLE_RE = re.compile(
    r"(APB_Vehicles|/Vehicles/|V_[A-Z]_|Vehicle|Golemobile|Wheel|Chassis)",
    re.I,
)
TEXTURE_HINT = re.compile(
    r"(Normal|NormMap|Nrm|Specular|Spec|Diffuse|Albedo|Roughness|Mask|Bump|BRDF|_N\b|_D\b|_S\b)",
    re.I,
)


def upk_header(path: Path) -> tuple[int, int] | None:
    try:
        with path.open("rb") as f:
            head = f.read(8)
    except OSError:
        return None
    if len(head) < 8 or struct.unpack_from("<I", head, 0)[0] != 0x9E2A83C1:
        return None
    fv, lv = struct.unpack_from("<HH", head, 4)
    return int(fv), int(lv)


def classify_upk(rel: str) -> str:
    r = rel.replace("\\", "/")
    # Path-prefix families first (Anim/Weapon is animations, not weapons)
    if r.startswith("Anim/") or Path(r).name.startswith("Anim_"):
        return "animations"
    if r.startswith("VFX/"):
        return "vfx"
    if r.startswith("Interface/") or "APBMenus" in r:
        return "interface_ui"
    if r.startswith("MaterialDatabase/"):
        return "materials"
    if "District" in r and r.lower().endswith(".upk"):
        return "district_packages"
    if r.startswith("Character/") or r.startswith("Packages/APB_CharacterTool/") or r.startswith(
        "Packages/Pedestrians/"
    ):
        return "characters"
    if r.startswith("Packages/APB_Vehicles/") or r.startswith("Packages/Vehicles/") or re.search(
        r"/V_[A-Z]_", r
    ):
        return "vehicles"
    if "/DesignObjects/Weapons/" in r or r.endswith("Weapon_M16.upk") or "PlaceholderWeapons" in r:
        return "weapons"
    if WEAPON_RE.search(r) and not r.startswith("Anim/"):
        return "weapons"
    if r.startswith("DesignObjects/"):
        return "design_objects"
    if CHARACTER_RE.search(r) and r.startswith("Packages/") and "Vehicle" not in r:
        if re.search(r"(^|/)[FM]_", r) or "Cloth" in r or "Body" in r or "Hair" in r:
            return "characters_clothing"
    if r.startswith("Packages/"):
        return "packages_other"
    if r.startswith("Engine/") or "Engine" in r:
        return "engine_content"
    return "other_upk"


def walk_tree(base: Path) -> tuple[list[dict], Counter, Counter, dict]:
    rows: list[dict] = []
    ext_counts: Counter = Counter()
    ext_bytes: Counter = Counter()
    by_folder: dict[str, Counter] = defaultdict(Counter)
    version_hist: Counter = Counter()

    if not base.is_dir():
        return rows, ext_counts, ext_bytes, {"by_folder": {}, "version_hist": {}}

    for p in base.rglob("*"):
        if not p.is_file():
            continue
        try:
            rel = str(p.relative_to(base)).replace("\\", "/")
            size = p.stat().st_size
        except (OSError, ValueError):
            continue
        ext = p.suffix.lower() or "(none)"
        ext_counts[ext] += 1
        ext_bytes[ext] += size
        top = rel.split("/", 1)[0]
        by_folder[top][ext] += 1

        row: dict = {
            "rel_path": rel,
            "size": size,
            "ext": ext,
            "top": top,
            "family": None,
            "file_version": None,
            "licensee_version": None,
        }
        if ext == ".upk":
            h = upk_header(p)
            if h:
                row["file_version"], row["licensee_version"] = h
                version_hist[f"{h[0]}/{h[1]}"] += 1
            # classify relative to Content when possible
            if base == CONTENT or base.name == "Content":
                row["family"] = classify_upk(rel)
            else:
                # under APBGame
                if rel.startswith("Content/"):
                    row["family"] = classify_upk(rel[len("Content/") :])
                else:
                    row["family"] = "apbgame_non_content"
        rows.append(row)

    meta = {
        "by_folder": {k: dict(v) for k, v in sorted(by_folder.items())},
        "version_hist": dict(version_hist),
    }
    return rows, ext_counts, ext_bytes, meta


def family_counts(rows: list[dict]) -> dict[str, dict]:
    out: dict[str, dict] = {}
    for r in rows:
        fam = r.get("family") or "unclassified"
        if r.get("ext") != ".upk" and fam not in ("apbgame_non_content",):
            continue
        if fam == "unclassified" and r.get("ext") != ".upk":
            continue
        bucket = out.setdefault(fam, {"count": 0, "bytes": 0, "examples": []})
        if r.get("ext") == ".upk" or fam == "apbgame_non_content":
            if r.get("ext") != ".upk":
                continue
        bucket["count"] += 1
        bucket["bytes"] += r["size"]
        if len(bucket["examples"]) < 8:
            bucket["examples"].append(r["rel_path"])
    return out


def count_by_glob(base: Path, pattern: str) -> int:
    return sum(1 for _ in base.glob(pattern)) if base.is_dir() else 0


def rcount(base: Path, pattern: str) -> int:
    return sum(1 for _ in base.rglob(pattern)) if base.is_dir() else 0


def string_mine_texture_hints(path: Path, limit: int = 80000) -> list[str]:
    try:
        data = path.read_bytes()[:limit]
    except OSError:
        return []
    found = set()
    for m in re.finditer(rb"[A-Za-z_][A-Za-z0-9_]{3,48}", data):
        s = m.group().decode("ascii", "ignore")
        if TEXTURE_HINT.search(s):
            found.add(s)
    return sorted(found)[:40]


def probe_umodel(
    package_name: str,
    path_root: Path,
    scratch: Path,
    export: bool = False,
    out_export: Path | None = None,
) -> dict:
    """List (or export) one package with in-repo umodel. Returns result dict."""
    result = {
        "package": package_name,
        "path_root": str(path_root),
        "ok": False,
        "exit_code": None,
        "cmd": None,
        "stdout_tail": "",
        "stderr_tail": "",
        "log_file": None,
    }
    if not UMODEL.is_file():
        result["stderr_tail"] = f"umodel missing: {UMODEL}"
        return result
    if not path_root.is_dir():
        result["stderr_tail"] = f"path root missing: {path_root}"
        return result

    scratch.mkdir(parents=True, exist_ok=True)
    args = [str(UMODEL), f"-path={path_root}", "-game=apb"]
    if export and out_export:
        out_export.mkdir(parents=True, exist_ok=True)
        args += ["-export", f"-out={out_export}", package_name]
    else:
        args += ["-list", package_name]
    result["cmd"] = " ".join(args)

    try:
        proc = subprocess.run(
            args,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=120,
            cwd=str(UMODEL.parent),
        )
    except subprocess.TimeoutExpired as e:
        result["exit_code"] = -1
        result["stderr_tail"] = f"timeout: {e}"
        log = scratch / f"probe_{package_name}.log"
        log.write_text(result["stderr_tail"], encoding="utf-8")
        result["log_file"] = str(log)
        return result

    result["exit_code"] = proc.returncode
    out = (proc.stdout or "") + ("\n" + proc.stderr if proc.stderr else "")
    result["stdout_tail"] = out[-4000:]
    result["stderr_tail"] = (proc.stderr or "")[-2000:]
    # success heuristics: exit 0 or listing contains Loading package / Exporting
    lower = out.lower()
    ok = proc.returncode == 0 or (
        "loading package" in lower and "error" not in lower[:500]
    )
    # explicit failures
    if any(
        x in lower
        for x in (
            "wrong package version",
            "fatal error",
            "failed to load",
            "unknown package version",
            "exception",
        )
    ):
        ok = False
    if "0 objects" in lower and "error" in lower:
        ok = False
    # if we got class names listed, treat as success even with nonzero
    if re.search(r"\b(SkeletalMesh|StaticMesh|Texture2D|AnimSequence|Material)\b", out):
        ok = True
    result["ok"] = ok

    log = scratch / f"probe_{package_name}.log"
    log.write_text(
        f"cmd: {result['cmd']}\nexit: {proc.returncode}\nok: {ok}\n\n{out}",
        encoding="utf-8",
    )
    result["log_file"] = str(log)
    return result


def pick_probes(content: Path) -> list[tuple[str, Path, str]]:
    """(package_stem, -path root, family label)."""
    probes: list[tuple[str, Path, str]] = []

    weapon = content / "DesignObjects" / "Weapons" / "Weapon_AssaultRifle.upk"
    if weapon.is_file():
        probes.append((weapon.stem, content / "DesignObjects" / "Weapons", "weapons"))
    elif (content / "Packages" / "Weapon_M16.upk").is_file():
        probes.append(("Weapon_M16", content / "Packages", "weapons"))

    contact = content / "Character" / "Contact" / "Contact_Bloodrose.upk"
    if contact.is_file():
        probes.append((contact.stem, content / "Character" / "Contact", "characters"))
    else:
        any_c = next((content / "Character").rglob("Contact_*.upk"), None)
        if any_c:
            probes.append((any_c.stem, any_c.parent, "characters"))

    veh = content / "Packages" / "APB_Vehicles" / "V_A_2DrCoupe"
    if veh.is_dir():
        cand = next(veh.glob("*.upk"), None)
        if cand:
            probes.append((cand.stem, content / "Packages" / "APB_Vehicles", "vehicles"))
    if len(probes) < 3:
        # fallbacks
        for upk in (content / "Packages").rglob("V_*.upk"):
            probes.append((upk.stem, content / "Packages", "vehicles"))
            break
    return probes[:5]


def build_priority_matrix(
    already: dict,
    families: dict,
    probe_results: list[dict],
) -> list[dict]:
    probe_by_fam = {p.get("family") or p.get("label"): p for p in probe_results}
    rows = []

    def add(name, status, tool, notes, count=None):
        rows.append(
            {
                "class": name,
                "already_extracted": status,
                "tool_path": tool,
                "feasibility": notes,
                "approx_count": count,
            }
        )

    add(
        "Audio (Wwise banks + Music/Dialogue/SFX PCK + MusicStudio)",
        "FULL under Content/Extracted/Audio/2011 (~31k WAV)",
        "tools/WwiseExtract/dump_2011_audio.py",
        "Done — re-run dump only if sources change",
        already.get("audio_wav"),
    )
    add(
        "Login/UI textures (partial)",
        "Partial Content/Extracted/2011_rtw/UI/umodel (TGA from menu UPKs)",
        "umodel -game=apb on Interface/*.upk",
        "Feasible: 2011 UI packages open with umodel (proven 2011_rtw). Full Interface dump remaining (~191 UPK).",
        families.get("interface_ui", {}).get("count"),
    )
    add(
        "Movies (.bik)",
        "Partial copy in 2011_rtw/Movies (3 of 6)",
        "plain copy; decode via Bink/ffmpeg if needed",
        "Trivial full copy of APBGame/Movies",
        already.get("movies_bik"),
    )
    add(
        "Localization",
        "Sample INT files in 2011_rtw/Localization",
        "plain copy of APBGame/Localization",
        "Trivial — 6 locales × ~100 files",
        already.get("loc_files"),
    )
    add(
        "Config / launcher",
        "Sample in 2011_rtw/Config",
        "plain copy",
        "Trivial",
        already.get("config_ini"),
    )
    weap_probe = next((p for p in probe_results if p.get("label") == "weapons"), None)
    add(
        "Weapon meshes/textures",
        "Not bulk-extracted for 2011 (Steam UmodelExport is 564/33)",
        "umodel list/export DesignObjects/Weapons + Packages/Weapon_*",
        (
            f"Probe {'OK' if weap_probe and weap_probe.get('ok') else 'FAIL/UNKNOWN'} "
            f"(see probes). Full dump: ~{families.get('weapons', {}).get('count', '?')} weapon-class UPKs + ItemAssets bins."
        ),
        families.get("weapons", {}).get("count"),
    )
    char_probe = next((p for p in probe_results if p.get("label") == "characters"), None)
    add(
        "Characters / contacts / clothing",
        "Steam-side exports exist; 2011 Character/ not fully dumped",
        "umodel on Character/Contact + Packages/APB_CharacterTool",
        (
            f"Probe {'OK' if char_probe and char_probe.get('ok') else 'FAIL/UNKNOWN'}. "
            f"~{families.get('characters', {}).get('count', 0)} Character UPKs + "
            f"~{families.get('characters_clothing', {}).get('count', 0)} clothing-class + 806 CharacterTool."
        ),
        (families.get("characters", {}).get("count") or 0)
        + (families.get("characters_clothing", {}).get("count") or 0),
    )
    veh_probe = next((p for p in probe_results if p.get("label") == "vehicles"), None)
    add(
        "Vehicles",
        "Steam UmodelExport_Vehicles partial; 2011 APB_Vehicles not bulk-dumped",
        "umodel on Packages/APB_Vehicles (~1971 UPK)",
        f"Probe {'OK' if veh_probe and veh_probe.get('ok') else 'FAIL/UNKNOWN'}. Largest mesh family.",
        families.get("vehicles", {}).get("count"),
    )
    add(
        "District / world packages + .apb maps",
        "UE5 maps rebuilt separately; raw 2011 district UPKs not fully exported",
        "umodel on *District/Packages; .apb are cooked maps (umodel may open as packages)",
        "High volume (Financial+Waterfront+Social). Maps/*.apb + district streaming.",
        families.get("district_packages", {}).get("count"),
    )
    add(
        "Materials / textures / normal maps",
        "Partial UI TGA; MaterialDatabase + mesh packages hold Texture2D (incl. normal/spec)",
        "umodel -export (Texture2D → TGA); string-mine Normal/Specular names",
        "Normals/specs are inside UPKs as Texture2D, not loose files. Extract via umodel with meshes.",
        families.get("materials", {}).get("count"),
    )
    add(
        "Animations",
        "Not bulk-extracted for 2011",
        "umodel PSA/AnimSequence from Content/Anim",
        f"~{families.get('animations', {}).get('count', 81)} anim UPKs",
        families.get("animations", {}).get("count"),
    )
    add(
        "VFX",
        "Not bulk-extracted for 2011",
        "umodel on Content/VFX",
        f"~{families.get('vfx', {}).get('count', 84)} VFX UPKs",
        families.get("vfx", {}).get("count"),
    )
    add(
        "ItemAssets (.bin)",
        "Not decoded",
        "custom parser / apbdb correlation — not umodel",
        "1249+ small bins (vehicle/item definitions). Plain copy only; format research needed for 'full extend'.",
        already.get("itemassets_bin"),
    )
    add(
        "Script packages (.u)",
        "Not decompiled in-tree",
        "UE Explorer / bytecode tools on ScriptUserBuild/*.u",
        "Core gameplay script: APBGame.u, APBUserInterface.u, etc.",
        already.get("script_u"),
    )
    add(
        "Engine shaders (.usf) + Engine content UPKs",
        "Not required for game art dump",
        "plain copy / umodel Engine/Content",
        "Reference shaders + engine meshes",
        already.get("engine_usf"),
    )
    return rows


def write_report(
    out_dir: Path,
    summary: dict,
    matrix: list[dict],
    probes: list[dict],
    texture_samples: dict,
) -> Path:
    out_dir.mkdir(parents=True, exist_ok=True)
    md = out_dir / "EXTRACTABILITY_INVENTORY.md"
    lines = [
        "# 2011 APB full-extent extractability inventory",
        "",
        f"Generated: {summary.get('generated_iso', '')}",
        f"Source: `{summary.get('source')}`",
        "",
        "## Package version fact",
        "",
        f"- **2011 client UPKs: FileVersion {EXPECTED_2011[0]} / Licensee {EXPECTED_2011[1]}** "
        f"(histogram: `{summary.get('version_hist')}`).",
        f"- **Steam/live APB Reloaded packages (comparison only): FileVersion {STEAM_VER[0]} / Licensee {STEAM_VER[1]}**.",
        "- In-repo umodel APB fixes primarily target **564/33**. **547/31** must be validated by probes below — "
        "not assumed identical.",
        "",
        "## Content tree scale",
        "",
        f"- Content files walked: **{summary.get('content_file_count')}**",
        f"- Content `.upk`: **{summary.get('content_upk_count')}** ({summary.get('content_upk_bytes_gb')} GB)",
        f"- Content `.apb` maps/levels: **{summary.get('content_apb_count')}**",
        f"- ItemAssets `.bin`: **{summary.get('itemassets_bin')}**",
        f"- Audio leftovers in tree (bnk/pck/wav): bnk={summary.get('audio_bnk')}, "
        f"pck={summary.get('audio_pck')}, wav={summary.get('audio_wav_in_tree')}",
        f"- Full 2011 audio dump already at: `Content/Extracted/Audio/2011` "
        f"(wav_decoded≈{summary.get('audio_dump_wav_decoded')})",
        "",
        "### Extension histogram (Content/)",
        "",
        "| Ext | Count | Size (MB) |",
        "|-----|------:|----------:|",
    ]
    for ext, cnt in summary.get("ext_counts", []):
        mb = summary.get("ext_bytes", {}).get(ext, 0) / (1024 * 1024)
        lines.append(f"| `{ext}` | {cnt} | {mb:.1f} |")

    lines += [
        "",
        "### Top Content folders",
        "",
        "| Folder | Files (by ext summary) |",
        "|--------|------------------------|",
    ]
    for folder, exts in summary.get("by_folder", {}).items():
        lines.append(f"| `{folder}` | {exts} |")

    lines += [
        "",
        "## UPK families (classified)",
        "",
        "| Family | UPK count | Bytes (MB) | Examples |",
        "|--------|----------:|-----------:|----------|",
    ]
    for fam, info in sorted(summary.get("families", {}).items(), key=lambda x: -x[1].get("count", 0)):
        mb = info.get("bytes", 0) / (1024 * 1024)
        ex = ", ".join(f"`{e}`" for e in info.get("examples", [])[:3])
        lines.append(f"| **{fam}** | {info.get('count')} | {mb:.1f} | {ex} |")

    lines += [
        "",
        "## Other extractable classes (non-UPK)",
        "",
        "| Class | Path | Count | Notes |",
        "|-------|------|------:|-------|",
        f"| ItemAssets definitions | `Content/ItemAssets/` | {summary.get('itemassets_bin')} | `.bin` vehicle/item data |",
        f"| District cooked maps | `Content/*District/*.apb`, `Maps/` | {summary.get('district_apb')}+{summary.get('maps_apb')} | streaming levels |",
        f"| Movies | `APBGame/Movies/*.bik` | {summary.get('movies_bik')} | Bink video |",
        f"| Localization | `APBGame/Localization/*` | {summary.get('loc_files')} | INT/FRA/GER/ITA/RUS/SPA |",
        f"| Config | `APBGame/Config/*.ini` | {summary.get('config_ini')} | engine/game/UI |",
        f"| Script packages | `APBGame/ScriptUserBuild/*.u` | {summary.get('script_u')} | UnrealScript cooked |",
        f"| Engine shaders | `Engine/Shaders/*.usf` | {summary.get('engine_usf')} | HLSL/USF |",
        f"| Engine content UPK | `Engine/Content/**/*.upk` | {summary.get('engine_upk')} | editor/engine meshes |",
        f"| MusicStudio WAV | `Content/Audio/MusicStudio` | {summary.get('musicstudio_wav')} | already PCM |",
        f"| Wwise PCK (raw) | `Content/Audio/FilePackages` | {summary.get('audio_pck')} | dumped to WAV already |",
        f"| Audio graphs | `Content/Audio/**/*.ag` | {summary.get('audio_ag')} | vehicle/emitter graphs |",
        f"| Custom seeds | `Content/CustomSeeds/*.bin` | {summary.get('custom_seeds')} | graffiti/custom |",
        f"| Splash | `APBGame/Splash/**` | {summary.get('splash')} | BMP |",
        "",
        "## Texture / normal map feasibility",
        "",
        "Loose TGA in 2011 Content is almost none (icons only). **Diffuse, normal, specular, masks** live as "
        "`Texture2D` (and materials) **inside** character/vehicle/weapon/MaterialDatabase UPKs.",
        "Full-extent texture dump = umodel `-export` (TGA) on those packages. String-mine samples from probes:",
        "",
    ]
    for pkg, hints in texture_samples.items():
        lines.append(f"- `{pkg}`: {', '.join(f'`{h}`' for h in hints[:12]) or '(no Normal/Spec-like strings in first 80KB)'}")

    lines += [
        "",
        "## Representative umodel probes (547/31)",
        "",
        f"Tool: `{UMODEL}`",
        "",
        "| Package | Family | OK | Exit | Log |",
        "|---------|--------|:--:|-----:|-----|",
    ]
    for p in probes:
        lines.append(
            f"| `{p.get('package')}` | {p.get('label')} | {'yes' if p.get('ok') else 'no'} | "
            f"{p.get('exit_code')} | `{p.get('log_file')}` |"
        )
        # include short evidence
    lines += ["", "### Probe output excerpts", ""]
    for p in probes:
        lines.append(f"#### {p.get('package')} ({p.get('label')})")
        lines.append("```")
        lines.append((p.get("stdout_tail") or p.get("stderr_tail") or "")[:1500])
        lines.append("```")
        lines.append("")

    lines += [
        "## Priority extract matrix",
        "",
        "| Class | Already extracted? | Tool | Feasibility / remaining | Count |",
        "|-------|-------------------|------|-------------------------|------:|",
    ]
    for m in matrix:
        lines.append(
            f"| {m['class']} | {m['already_extracted']} | `{m['tool_path']}` | {m['feasibility']} | "
            f"{m.get('approx_count') if m.get('approx_count') is not None else '—'} |"
        )

    lines += [
        "",
        "## Recommended full-extent order (follow-up dumps)",
        "",
        "1. **Vehicles** (`Packages/APB_Vehicles`) — largest mesh set for freeroam fidelity.",
        "2. **Characters + clothing** (`Character/`, `Packages/APB_CharacterTool`, body/clothing UPKs).",
        "3. **Weapons** (`DesignObjects/Weapons/*`, holster clothing, `Anim/Weapon`).",
        "4. **MaterialDatabase** + re-export textures (normals/specs) with meshes.",
        "5. **District packages** + Maps `.apb` for world geometry.",
        "6. **Interface** complete TGA dump (login already partial).",
        "7. **Animations / VFX**.",
        "8. **ItemAssets .bin** + **Script .u** (data/code, not art) once parsers exist.",
        "",
        "## Non-goals / blockers",
        "",
        "- Do not assume Steam `UmodelExport` is the 2011 tree (different FileVersion).",
        "- Full bulk export of ~4.6k UPKs is multi-hour / multi-GB; this inventory defines feasibility.",
        "- ItemAssets and Script packages need specialized tools beyond umodel.",
        "",
    ]
    md.write_text("\n".join(lines), encoding="utf-8")
    return md


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", action="store_true", help="Run umodel list probes")
    ap.add_argument("--export-probe", action="store_true", help="Also try -export on probes")
    ap.add_argument(
        "--scratch",
        type=Path,
        default=Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-e4916a0f3e26\implementer"),
    )
    ap.add_argument("--out", type=Path, default=OUT_DIR)
    args = ap.parse_args(argv)

    t0 = time.time()
    if not CONTENT.is_dir():
        print(f"missing 2011 Content: {CONTENT}", file=sys.stderr)
        return 1

    content_rows, ext_counts, ext_bytes, meta = walk_tree(CONTENT)
    # also light walk of APBGame non-content for movies/loc/script
    movies_bik = rcount(APBGAME / "Movies", "*.bik")
    loc_files = sum(1 for _ in (APBGAME / "Localization").rglob("*") if _.is_file()) if (APBGAME / "Localization").is_dir() else 0
    config_ini = rcount(APBGAME / "Config", "*.ini")
    script_u = rcount(APBGAME / "ScriptUserBuild", "*.u")
    engine_usf = rcount(APB_NA / "Engine" / "Shaders", "*.usf")
    engine_upk = rcount(APB_NA / "Engine" / "Content", "*.upk")
    splash = sum(1 for _ in (APBGAME / "Splash").rglob("*") if _.is_file()) if (APBGAME / "Splash").is_dir() else 0

    upk_rows = [r for r in content_rows if r["ext"] == ".upk"]
    families = family_counts(upk_rows)
    # refine clothing under Packages root via name
    for r in upk_rows:
        if r.get("family") == "packages_other":
            name = Path(r["rel_path"]).name
            if re.match(r"^[FM]_", name) or "Cloth" in name or "Hair" in name or "Body" in name:
                r["family"] = "characters_clothing"
    families = family_counts(upk_rows)

    # weapon family: force DesignObjects/Weapons
    for r in upk_rows:
        rp = r["rel_path"].replace("\\", "/")
        if "/Weapons/" in rp or rp.endswith("Weapon_M16.upk") or "PlaceholderWeapons" in rp:
            r["family"] = "weapons"
    families = family_counts(upk_rows)

    # district apb counts
    district_apb = (
        rcount(CONTENT / "FinancialDistrict", "*.apb")
        + rcount(CONTENT / "WaterfrontDistrict", "*.apb")
        + rcount(CONTENT / "RWorldSocialDistrict", "*.apb")
    )
    maps_apb = rcount(CONTENT / "Maps", "*.apb")

    # audio dump status
    audio_dump = ROOT / "Content" / "Extracted" / "Audio" / "2011"
    audio_wav_decoded = None
    man = audio_dump / "audio_dump_manifest.json"
    if man.is_file():
        try:
            audio_wav_decoded = json.loads(man.read_text(encoding="utf-8")).get("totals", {}).get(
                "wav_decoded"
            )
        except json.JSONDecodeError:
            pass
    if audio_wav_decoded is None and audio_dump.is_dir():
        audio_wav_decoded = sum(1 for _ in audio_dump.rglob("*.wav"))

    # probes
    probes_out: list[dict] = []
    texture_samples: dict[str, list[str]] = {}
    probe_specs = pick_probes(CONTENT)
    export_root = args.out / "probe_export"

    for pkg, path_root, label in probe_specs:
        # resolve full path for string mine
        candidates = list(path_root.rglob(f"{pkg}.upk"))
        if candidates:
            texture_samples[pkg] = string_mine_texture_hints(candidates[0])
        if args.probe or args.export_probe:
            # umodel -path should be parent that contains the package file's search roots
            # Prefer CONTENT for multi-folder packages; also try specific folder
            pr = probe_umodel(pkg, path_root, args.scratch, export=False)
            pr["label"] = label
            if not pr["ok"]:
                # retry with broader Content path
                pr2 = probe_umodel(pkg, CONTENT, args.scratch, export=False)
                pr2["label"] = label
                if pr2["ok"] or (pr2.get("exit_code") == 0):
                    pr = pr2
                else:
                    # keep richer log
                    pr["retry_content"] = pr2
            if args.export_probe and pr.get("ok"):
                pr_exp = probe_umodel(
                    pkg,
                    Path(pr["path_root"]),
                    args.scratch,
                    export=True,
                    out_export=export_root / label,
                )
                pr["export"] = {
                    "ok": pr_exp.get("ok"),
                    "exit_code": pr_exp.get("exit_code"),
                    "log_file": pr_exp.get("log_file"),
                }
            probes_out.append(pr)
        else:
            probes_out.append(
                {
                    "package": pkg,
                    "path_root": str(path_root),
                    "label": label,
                    "ok": None,
                    "exit_code": None,
                    "cmd": None,
                    "stdout_tail": "(probe skipped; re-run with --probe)",
                    "log_file": None,
                }
            )

    already = {
        "audio_wav": audio_wav_decoded,
        "movies_bik": movies_bik,
        "loc_files": loc_files,
        "config_ini": config_ini,
        "itemassets_bin": ext_counts.get(".bin", 0),
        "script_u": script_u,
        "engine_usf": engine_usf,
    }
    matrix = build_priority_matrix(already, families, probes_out)

    summary = {
        "generated_iso": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "source": str(CONTENT),
        "apbgame": str(APBGAME),
        "expected_version": f"{EXPECTED_2011[0]}/{EXPECTED_2011[1]}",
        "steam_comparison_version": f"{STEAM_VER[0]}/{STEAM_VER[1]}",
        "version_hist": meta.get("version_hist"),
        "content_file_count": len(content_rows),
        "content_upk_count": len(upk_rows),
        "content_upk_bytes_gb": round(sum(r["size"] for r in upk_rows) / 1e9, 3),
        "content_apb_count": ext_counts.get(".apb", 0),
        "itemassets_bin": rcount(CONTENT / "ItemAssets", "*.bin"),
        "audio_bnk": ext_counts.get(".bnk", 0),
        "audio_pck": ext_counts.get(".pck", 0),
        "audio_wav_in_tree": ext_counts.get(".wav", 0),
        "audio_ag": ext_counts.get(".ag", 0),
        "audio_dump_wav_decoded": audio_wav_decoded,
        "musicstudio_wav": rcount(CONTENT / "Audio" / "MusicStudio", "*.wav"),
        "movies_bik": movies_bik,
        "loc_files": loc_files,
        "config_ini": config_ini,
        "script_u": script_u,
        "engine_usf": engine_usf,
        "engine_upk": engine_upk,
        "splash": splash,
        "custom_seeds": rcount(CONTENT / "CustomSeeds", "*.bin"),
        "district_apb": district_apb,
        "maps_apb": maps_apb,
        "ext_counts": ext_counts.most_common(),
        "ext_bytes": {k: v for k, v in ext_bytes.items()},
        "by_folder": meta.get("by_folder"),
        "families": families,
        "probes": probes_out,
        "priority_matrix": matrix,
        "texture_string_samples": texture_samples,
        "elapsed_sec": round(time.time() - t0, 2),
    }

    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "inventory_2011_full.json").write_text(
        json.dumps(summary, indent=2, default=str), encoding="utf-8"
    )

    # CSV of all UPKs with family
    csv_path = args.out / "inventory_2011_upk_families.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["rel_path", "family", "size", "file_version", "licensee_version"])
        for r in sorted(upk_rows, key=lambda x: x["rel_path"]):
            w.writerow(
                [
                    r["rel_path"],
                    r.get("family"),
                    r["size"],
                    r.get("file_version"),
                    r.get("licensee_version"),
                ]
            )

    md = write_report(args.out, summary, matrix, probes_out, texture_samples)
    # also dump probes JSON to scratch
    args.scratch.mkdir(parents=True, exist_ok=True)
    (args.scratch / "probe_results.json").write_text(
        json.dumps(probes_out, indent=2), encoding="utf-8"
    )
    (args.scratch / "inventory_summary_excerpt.json").write_text(
        json.dumps(
            {
                "version_hist": summary["version_hist"],
                "content_upk_count": summary["content_upk_count"],
                "families": {k: v.get("count") for k, v in families.items()},
                "probes_ok": [p.get("package") for p in probes_out if p.get("ok")],
            },
            indent=2,
        ),
        encoding="utf-8",
    )

    print(f"wrote {md}")
    print(f"wrote {csv_path}")
    print(f"upk={summary['content_upk_count']} version_hist={summary['version_hist']}")
    print(f"families={ {k:v['count'] for k,v in families.items()} }")
    print(f"probes={[ (p.get('package'), p.get('ok'), p.get('exit_code')) for p in probes_out ]}")
    print(f"elapsed={summary['elapsed_sec']}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
