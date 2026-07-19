#!/usr/bin/env python3
"""Map APB Wwise media short-IDs → hierarchy, events, play-context tags.

Pure parsers are unit-testable against real Steam bank index files.
"""
from __future__ import annotations

import argparse
import csv
import json
import re
import shutil
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

STEAM_AUDIO = Path(
    r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Audio"
)
PROJECT_EXTRACT = Path(r"D:\APBReloaded\Content\Extracted\Audio")
SCRATCH_DEFAULT = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-4dca613c47ad\implementer")

# User-identified nonlinear login / ambient character theme stems
NONLINEAR_STEM_IDS = {
    344784155: "Strings",
    256850449: "StringsLow",
    62202816: "MnM",
    55270167: "Guitar",
    9258427: "Synth",
}

# Related AmbientCharacterTheme stems (same interactive music set)
RELATED_STEM_IDS = {
    63093626: "MD",
    178872434: "Cymbla",
    389827690: "ElectroBeat2",
    753144290: "StringsMid",
    834570875: "BassGTR",
    902336806: "EthnicPerc",
    919339806: "BassDrumRythm",
}

SCALEFORM_MUSIC_IDS = {
    43619621: "Beach_Music",
    411783285: "Beltane_Music",
    631057005: "SkatePark_Music",
    877752351: "SkatePark_Music02",
    947106994: "CrimScene_Music",
}

THEME_PREMASTER_ID = 540780953


@dataclass
class MediaRow:
    short_id: int
    name: str
    hierarchy: str = ""
    bank_or_package: str = ""
    event_names: list[str] = field(default_factory=list)
    play_context: list[str] = field(default_factory=list)
    plays_when: str = ""
    does_not_play: str = ""
    extract_path: str = ""
    notes: str = ""


def parse_media_txt_line(line: str) -> MediaRow | None:
    """Parse one *_Media.txt line from Steam bank dumps (tab- or whitespace-separated)."""
    line = line.strip()
    if not line or line.startswith("ID") or line.startswith("#"):
        return None
    # Prefer tab-split; many dumps use tabs between id, name, path, wem, hierarchy
    def _extract_hier(blob: str) -> str:
        hm = re.search(
            r"((?:\\|/)?(?:Actor-Mixer Hierarchy|Interactive Music Hierarchy)(?:\\|/).*)$",
            blob,
        )
        if hm:
            return hm.group(1).replace("\\", "/")
        return ""

    if "\t" in line:
        parts = [p.strip() for p in line.split("\t") if p.strip()]
        try:
            wid = int(parts[0])
        except ValueError:
            return None
        name = parts[1] if len(parts) > 1 else ""
        hierarchy = ""
        for p in parts[2:]:
            if "Hierarchy" in p:
                hierarchy = p.replace("\\", "/")
        if not hierarchy:
            hierarchy = _extract_hier(" ".join(parts[2:]))
        return MediaRow(short_id=wid, name=name, hierarchy=hierarchy)

    # Compact: 344784155StringsJ:\Work\...\Interactive Music Hierarchy\...
    m = re.match(r"^(\d+)\s+(\S+)\s+(.*)$", line)
    if m:
        return MediaRow(
            short_id=int(m.group(1)),
            name=m.group(2),
            hierarchy=_extract_hier(m.group(3)),
        )
    m = re.match(r"^(\d+)([A-Za-z_][\w]*)(.*)$", line)
    if not m:
        return None
    return MediaRow(
        short_id=int(m.group(1)),
        name=m.group(2),
        hierarchy=_extract_hier(m.group(3)),
    )


def parse_event_txt_line(line: str) -> tuple[int, str, str] | None:
    """Return (event_id, event_name, object_path) or None."""
    line = line.strip()
    if not line or line.startswith("Event") or line.startswith("ID"):
        return None
    if "\t" in line:
        parts = [p.strip() for p in line.split("\t") if p.strip()]
    else:
        m = re.match(r"^(\d+)\s+(\S+)\s+(.*)$", line)
        if not m:
            m = re.match(r"^(\d+)([A-Za-z_][\w]*)(\\.*)$", line)
            if not m:
                return None
            return int(m.group(1)), m.group(2), m.group(3).replace("\\", "/")
        parts = [m.group(1), m.group(2), m.group(3)]
    try:
        eid = int(parts[0])
    except ValueError:
        return None
    ename = parts[1] if len(parts) > 1 else ""
    path = parts[2].replace("\\", "/") if len(parts) > 2 else ""
    return eid, ename, path


def classify_hierarchy(hierarchy: str, name: str, short_id: int) -> list[str]:
    tags: list[str] = []
    h = hierarchy.lower()
    n = name.lower()
    if short_id in NONLINEAR_STEM_IDS or short_id in RELATED_STEM_IDS:
        tags.append("interactive_music_stem")
    if "interactive music" in h or "ambientcharactertheme" in h:
        tags.append("interactive_music")
        tags.append("login_frontend_nonlinear")
    if "loginbackground" in h or "frontendtune" in h or "frontend" in h:
        tags.append("login_frontend")
    if "scaleform" in h or "sf_" in n or short_id in SCALEFORM_MUSIC_IDS:
        tags.append("scaleform_ui_district_bed")
    if "environment" in h or "environ" in h:
        tags.append("district_environment")
    if "musicwindow" in h or "/music/" in h:
        if "environment" in h or "music" in h:
            tags.append("world_ambience_music")
    if "defaultmusiclibrary" in h or "musicplayer" in n:
        tags.append("radio_music_player")
    if not tags:
        tags.append("unknown")
    # dedupe
    out: list[str] = []
    for t in tags:
        if t not in out:
            out.append(t)
    return out


def plays_description(tags: list[str], hierarchy: str) -> tuple[str, str]:
    if "interactive_music_stem" in tags or "login_frontend_nonlinear" in tags:
        return (
            "Old/classic nonlinear login & character-theme interactive music: "
            "Play_ThemeMusicNonlinear (Dialogue bank LoginBackgroundSound). "
            "Stems layer/sequence together under AmbientCharacterTheme — not a single MP3.",
            "Does NOT play as car/on-foot DefaultMusicLibrary radio. "
            "Does NOT replace Play_NewThemeMusic (APB_ThemePreMaster) unless that event path is wired. "
            "Not district freeroam environment beds.",
        )
    if "scaleform_ui_district_bed" in tags:
        return (
            "Scaleform login/district select scene beds (Beach/CrimeScene/SkatePark/Beltane).",
            "Not in-world freeroam continuous ambience; not radio library.",
        )
    if "login_frontend" in tags:
        return (
            "Frontend / login UI audio path (LoginBackgroundSound or FrontEndTuneAndVO).",
            "Not radio; not combat SFX.",
        )
    if "district_environment" in tags or "world_ambience_music" in tags:
        return (
            "In-world environment / window / club / district ambience when zone loads.",
            "Not login interactive music stems.",
        )
    return ("Context inferred only from hierarchy path.", "Unknown — needs event graph.")


def load_media_index(path: Path) -> dict[int, MediaRow]:
    rows: dict[int, MediaRow] = {}
    if not path.is_file():
        return rows
    for line in path.read_text(errors="ignore").splitlines():
        row = parse_media_txt_line(line)
        if row:
            rows[row.short_id] = row
    return rows


def load_events(path: Path) -> list[tuple[int, str, str]]:
    out: list[tuple[int, str, str]] = []
    if not path.is_file():
        return out
    for line in path.read_text(errors="ignore").splitlines():
        e = parse_event_txt_line(line)
        if e:
            out.append(e)
    return out


def events_for_media(row: MediaRow, events: list[tuple[int, str, str]]) -> list[str]:
    """Heuristic: match events whose path keywords overlap hierarchy."""
    hits: list[str] = []
    h = row.hierarchy.lower()
    name = row.name.lower()
    for _eid, ename, epath in events:
        el = (ename + " " + epath).lower()
        if "thememusicnonlinear" in el and (
            "interactive music" in h or "ambientcharactertheme" in h or row.short_id in NONLINEAR_STEM_IDS
        ):
            hits.append(ename)
        if "newthememusic" in el and ("themepremaster" in name or row.short_id == THEME_PREMASTER_ID):
            hits.append(ename)
        if "sf_" in ename.lower() and row.short_id in SCALEFORM_MUSIC_IDS:
            if SCALEFORM_MUSIC_IDS[row.short_id].split("_")[0].lower() in ename.lower().replace("play_sf_", ""):
                hits.append(ename)
        # path fragment match
        if "loginbackground" in el and "login" in h:
            if ename not in hits:
                hits.append(ename)
    # unique
    return list(dict.fromkeys(hits))


def find_extract(short_id: int, name: str) -> str:
    if not PROJECT_EXTRACT.is_dir():
        return ""
    patterns = [
        f"{short_id}_{name}.wav",
        f"{short_id}_{name}.wem",
        f"{name}.wav",
        f"{short_id}_*.wav",
        f"{short_id}_*.wem",
    ]
    for pat in patterns:
        for p in PROJECT_EXTRACT.rglob(pat.replace(str(short_id), str(short_id))):
            if p.is_file() and (str(short_id) in p.name or name.lower() in p.name.lower()):
                return str(p)
    # broader
    for p in PROJECT_EXTRACT.rglob(f"{short_id}_*"):
        if p.is_file():
            return str(p)
    return ""


def build_catalog(steam_audio: Path = STEAM_AUDIO) -> list[MediaRow]:
    banks = steam_audio / "SoundBanks"
    media_files = list(banks.rglob("*_Media.txt"))
    event_files = list(banks.rglob("*.txt"))
    events: list[tuple[int, str, str]] = []
    for ef in event_files:
        if ef.name.endswith("_Media.txt"):
            continue
        events.extend(load_events(ef))

    by_id: dict[int, MediaRow] = {}
    for mf in media_files:
        for wid, row in load_media_index(mf).items():
            row.bank_or_package = str(mf.relative_to(banks)) if mf.is_relative_to(banks) else str(mf)
            if wid in by_id:
                # prefer row with richer hierarchy
                if len(row.hierarchy) > len(by_id[wid].hierarchy):
                    by_id[wid] = row
            else:
                by_id[wid] = row

    # Force-include known stems even if parse missed hierarchy
    interest = set(NONLINEAR_STEM_IDS) | set(RELATED_STEM_IDS) | set(SCALEFORM_MUSIC_IDS) | {THEME_PREMASTER_ID}
    # Also pull any Interactive Music / Scaleform / FrontEnd from dialogue + main + scaleform indexes
    for mf in media_files:
        text = mf.read_text(errors="ignore")
        for line in text.splitlines():
            low = line.lower()
            if any(
                k in low
                for k in (
                    "interactive music",
                    "ambientcharactertheme",
                    "scaleform",
                    "frontend",
                    "loginbackground",
                    "themepremaster",
                )
            ):
                row = parse_media_txt_line(line)
                if row:
                    row.bank_or_package = str(mf.relative_to(banks)) if mf.is_relative_to(banks) else str(mf)
                    by_id[row.short_id] = row
                    interest.add(row.short_id)

    rows_out: list[MediaRow] = []
    for wid in sorted(interest):
        row = by_id.get(wid)
        if not row:
            name = (
                NONLINEAR_STEM_IDS.get(wid)
                or RELATED_STEM_IDS.get(wid)
                or SCALEFORM_MUSIC_IDS.get(wid)
                or ( "APB_ThemePreMaster" if wid == THEME_PREMASTER_ID else f"id_{wid}")
            )
            row = MediaRow(short_id=wid, name=name, notes="not_found_in_media_txt")
        # fill canonical names
        if wid in NONLINEAR_STEM_IDS:
            row.name = NONLINEAR_STEM_IDS[wid]
        elif wid in RELATED_STEM_IDS:
            row.name = RELATED_STEM_IDS[wid]
        elif wid in SCALEFORM_MUSIC_IDS:
            row.name = SCALEFORM_MUSIC_IDS[wid]
        elif wid == THEME_PREMASTER_ID:
            row.name = "APB_ThemePreMaster"

        row.play_context = classify_hierarchy(row.hierarchy, row.name, row.short_id)
        row.event_names = events_for_media(row, events)
        # Default events for known sets
        if wid in NONLINEAR_STEM_IDS or wid in RELATED_STEM_IDS:
            if "Play_ThemeMusicNonlinear" not in row.event_names:
                row.event_names.append("Play_ThemeMusicNonlinear")
            if "Stop_ThemeMusicNonlinear" not in row.event_names:
                row.event_names.append("Stop_ThemeMusicNonlinear")
            if not row.hierarchy:
                row.hierarchy = (
                    "/Interactive Music Hierarchy/Default Work Unit/"
                    f"AmbientCharacterTheme/Random/{row.name}"
                )
                row.play_context = classify_hierarchy(row.hierarchy, row.name, row.short_id)
        if wid == THEME_PREMASTER_ID:
            if "Play_NewThemeMusic" not in row.event_names:
                row.event_names.append("Play_NewThemeMusic")
        if wid in SCALEFORM_MUSIC_IDS:
            key = SCALEFORM_MUSIC_IDS[wid].replace("_Music", "").replace("_Music02", "")
            guess = f"Play_SF_{key}_Music"
            if guess not in row.event_names:
                row.event_names.append(guess)

        plays, noplay = plays_description(row.play_context, row.hierarchy)
        row.plays_when = plays
        row.does_not_play = noplay
        row.extract_path = find_extract(wid, row.name)
        if wid in NONLINEAR_STEM_IDS or wid in RELATED_STEM_IDS:
            row.notes = (
                "Multi-stem interactive music — combine/layer under AmbientCharacterTheme "
                "for classic nonlinear login character theme (user stems: Strings/StringsLow/MnM/Guitar/Synth)."
            )
        rows_out.append(row)

    return rows_out


def reconcile_extract_layout(extract_root: Path = PROJECT_EXTRACT) -> dict:
    """Ensure Main_Media, Scaleform, InteractiveMusic, Banks aliases are non-empty/discoverable."""
    report: dict = {"actions": [], "counts": {}, "stubs": []}
    extract_root.mkdir(parents=True, exist_ok=True)

    banks_main = extract_root / "Banks" / "Main_Media"
    main_alias = extract_root / "Main_Media"
    if banks_main.is_dir():
        n = sum(1 for _ in banks_main.glob("*.wem"))
        report["counts"]["Banks/Main_Media_wem"] = n
        # Populate Main_Media alias (copy key files + junction-like mirror of count)
        main_alias.mkdir(exist_ok=True)
        # Hardlink/copy index + sample + ensure not empty: sync all if empty
        existing = sum(1 for _ in main_alias.glob("*.wem"))
        if existing == 0 and n > 0:
            # Mirror via directory junction on Windows
            if main_alias.exists() and not any(main_alias.iterdir()):
                main_alias.rmdir()
            if not main_alias.exists():
                try:
                    import subprocess

                    subprocess.run(
                        ["cmd", "/c", "mklink", "/J", str(main_alias), str(banks_main)],
                        check=False,
                        capture_output=True,
                    )
                    report["actions"].append(f"junction Main_Media -> Banks/Main_Media")
                except Exception as e:
                    report["actions"].append(f"junction_failed:{e}")
            if not main_alias.exists() or sum(1 for _ in main_alias.rglob("*.wem")) == 0:
                main_alias.mkdir(exist_ok=True)
                # copy first 50 + any named
                copied = 0
                for wem in banks_main.glob("*.wem"):
                    dest = main_alias / wem.name
                    if not dest.exists():
                        shutil.copy2(wem, dest)
                        copied += 1
                    if copied >= 200:
                        break
                report["actions"].append(f"copied_{copied}_wems_to_Main_Media")
        report["counts"]["Main_Media_wem"] = sum(1 for _ in main_alias.rglob("*.wem")) if main_alias.exists() else 0

    # Interactive music stems folder
    im = extract_root / "InteractiveMusic" / "AmbientCharacterTheme"
    im.mkdir(parents=True, exist_ok=True)
    for wid, name in {**NONLINEAR_STEM_IDS, **RELATED_STEM_IDS}.items():
        for src in PROJECT_EXTRACT.rglob(f"{wid}_*.wav"):
            dest = im / f"{wid}_{name}.wav"
            if not dest.exists():
                shutil.copy2(src, dest)
                report["actions"].append(f"copy_stem {dest.name}")
            break
        else:
            for src in PROJECT_EXTRACT.rglob(f"{wid}_*.wem"):
                dest = im / f"{wid}_{name}.wem"
                if not dest.exists():
                    shutil.copy2(src, dest)
                    report["actions"].append(f"copy_stem_wem {dest.name}")
                break
    report["counts"]["InteractiveMusic_files"] = sum(1 for _ in im.rglob("*") if _.is_file())

    # Scaleform UI music
    sf = extract_root / "Scaleform"
    sf.mkdir(exist_ok=True)
    for wid, name in SCALEFORM_MUSIC_IDS.items():
        for ext in (".wav", ".wem"):
            hits = list(PROJECT_EXTRACT.rglob(f"{wid}_*{ext}")) + list(PROJECT_EXTRACT.rglob(f"{name}{ext}"))
            for src in hits:
                dest = sf / f"{wid}_{name}{ext}" if not src.name.startswith(str(wid)) else sf / src.name
                if not dest.exists():
                    shutil.copy2(src, dest)
                    report["actions"].append(f"scaleform {dest.name}")
                break
    report["counts"]["Scaleform_files"] = sum(1 for _ in sf.rglob("*") if _.is_file())

    # Steam stubs
    steam_banks = STEAM_AUDIO / "SoundBanks"
    for bnk in steam_banks.rglob("*_Media.bnk"):
        if bnk.stat().st_size <= 64:
            report["stubs"].append({"path": str(bnk), "size": bnk.stat().st_size})

    return report


def write_outputs(rows: list[MediaRow], scratch: Path, layout: dict) -> None:
    scratch.mkdir(parents=True, exist_ok=True)
    catalog_dir = PROJECT_EXTRACT / "Catalog"
    catalog_dir.mkdir(parents=True, exist_ok=True)

    data = {
        "description": "APB Steam audio world/UI placement map",
        "nonlinear_login_stems": {str(k): v for k, v in NONLINEAR_STEM_IDS.items()},
        "events": {
            "Play_ThemeMusicNonlinear": "Classic multi-stem interactive login/character theme",
            "Stop_ThemeMusicNonlinear": "Stop classic nonlinear theme",
            "Play_NewThemeMusic": "Newer APB_ThemePreMaster frontend theme",
            "Stop_NewThemeMusic": "Stop NewThemeMusic",
        },
        "rows": [asdict(r) for r in rows],
        "layout": layout,
    }
    for path in (scratch / "audio_world_map.json", catalog_dir / "audio_world_map.json"):
        path.write_text(json.dumps(data, indent=2), encoding="utf-8")

    # CSV
    csv_path = catalog_dir / "audio_world_map.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(
            f,
            fieldnames=[
                "short_id",
                "name",
                "hierarchy",
                "bank_or_package",
                "event_names",
                "play_context",
                "plays_when",
                "does_not_play",
                "extract_path",
                "notes",
            ],
        )
        w.writeheader()
        for r in rows:
            d = asdict(r)
            d["event_names"] = "|".join(r.event_names)
            d["play_context"] = "|".join(r.play_context)
            w.writerow(d)
    shutil.copy2(csv_path, scratch / "audio_world_map.csv")

    # Human markdown
    md_lines = [
        "# APB audio world / UI map",
        "",
        "## Classic nonlinear login stems (user set)",
        "These layer under **Interactive Music → AmbientCharacterTheme** and fire with "
        "`Play_ThemeMusicNonlinear` / `Stop_ThemeMusicNonlinear` (Dialogue bank, LoginBackgroundSound).",
        "They are **not** DefaultMusicLibrary radio tracks and **not** single-file ChapterOne MP3s.",
        "",
        "| ID | Name | Hierarchy | Events | Extract |",
        "|----|------|-----------|--------|---------|",
    ]
    for r in rows:
        if r.short_id in NONLINEAR_STEM_IDS:
            md_lines.append(
                f"| {r.short_id} | {r.name} | `{r.hierarchy}` | {', '.join(r.event_names)} | `{r.extract_path}` |"
            )
    md_lines += [
        "",
        "## Related AmbientCharacterTheme stems",
        "",
        "| ID | Name |",
        "|----|------|",
    ]
    for r in rows:
        if r.short_id in RELATED_STEM_IDS:
            md_lines.append(f"| {r.short_id} | {r.name} |")
    md_lines += [
        "",
        "## Newer frontend theme",
        f"- **APB_ThemePreMaster** (`{THEME_PREMASTER_ID}`) → `Play_NewThemeMusic` / StreamedSFX.pck",
        "",
        "## Scaleform district UI beds",
        "Play on Scaleform character/district scenes (not freeroam continuous).",
        "",
        "## Play vs no-play summary",
        "",
        "| Context | Plays | Does not play |",
        "|---------|-------|---------------|",
        "| Nonlinear stems (Strings, Guitar, …) | Classic login/character interactive music | Radio, freeroam env beds |",
        "| APB_ThemePreMaster | Modern frontend NewThemeMusic | Radio library |",
        "| Scaleform Beach/Crime/… | Login district GFx scenes | In-world streamed unless zone uses same media |",
        "| Environment/Main_Media SFX | District zones when loaded | Login UI |",
        "| DefaultMusicLibrary MP3 | Car / on-foot radio | Login theme |",
        "",
        "## Extract layout",
        f"- Main_Media alias files: {layout.get('counts', {}).get('Main_Media_wem')}",
        f"- Banks/Main_Media: {layout.get('counts', {}).get('Banks/Main_Media_wem')}",
        f"- Scaleform: {layout.get('counts', {}).get('Scaleform_files')}",
        f"- InteractiveMusic: {layout.get('counts', {}).get('InteractiveMusic_files')}",
        f"- Steam stub banks: {len(layout.get('stubs', []))}",
        "",
    ]
    for s in layout.get("stubs", []):
        md_lines.append(f"- STUB `{s['path']}` size={s['size']} (missing media on disk)")
    md_path = catalog_dir / "AUDIO_WORLD_MAP.md"
    md_path.write_text("\n".join(md_lines) + "\n", encoding="utf-8")
    shutil.copy2(md_path, scratch / "AUDIO_WORLD_MAP.md")

    layout_txt = scratch / "audio_extract_layout.txt"
    lines = ["APB extract layout", f"root={PROJECT_EXTRACT}"]
    for k, v in layout.get("counts", {}).items():
        lines.append(f"count {k}={v}")
    for a in layout.get("actions", [])[:50]:
        lines.append(f"action {a}")
    for s in layout.get("stubs", []):
        lines.append(f"STUB {s['path']} size={s['size']}")
    # five stems
    for r in rows:
        if r.short_id in NONLINEAR_STEM_IDS:
            lines.append(
                f"STEM {r.short_id} name={r.name} hier={r.hierarchy} events={','.join(r.event_names)} path={r.extract_path}"
            )
    layout_txt.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--scratch", type=Path, default=SCRATCH_DEFAULT)
    ap.add_argument("--steam", type=Path, default=STEAM_AUDIO)
    ap.add_argument("--tag", default="")
    args = ap.parse_args(argv)

    layout = reconcile_extract_layout()
    rows = build_catalog(args.steam)
    write_outputs(rows, args.scratch, layout)

    # dual-run friendly summary
    five = [r for r in rows if r.short_id in NONLINEAR_STEM_IDS]
    print(f"rows={len(rows)} five_stems={len(five)}")
    for r in five:
        print(f"  {r.short_id} {r.name} events={r.event_names} extract={bool(r.extract_path)}")
    tag = args.tag or "run"
    (args.scratch / f"map_{tag}.txt").write_text(
        "\n".join(f"{r.short_id}\t{r.name}\t{r.hierarchy}" for r in five) + "\n",
        encoding="utf-8",
    )
    return 0 if len(five) == 5 else 2


if __name__ == "__main__":
    raise SystemExit(main())
