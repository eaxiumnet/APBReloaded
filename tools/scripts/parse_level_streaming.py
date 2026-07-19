"""Parse LevelStreamingKismet export blobs from APB MASTER maps (Ver 564/33).

Uses umodel -list line offsets (SerialOffset hex, Size hex) when provided via
a list file, then reads the file at those offsets (file may be fully loaded
in memory if not compressed at those ranges).

Also rebuilds freeroam placement manifests from multiple Steam block packages:
each BlockNN package maps to a grid cell; mesh names from umodel -list LOD0
StaticMeshes; positions = block_origin + local mesh index spacing.

This is package-topology freeroam (real Steam block packages + names), not
pure densified single-block clone — still not full Actor transform recovery
until LevelStreaming Location properties can be decoded.
"""
from __future__ import annotations

import json
import re
import struct
import subprocess
from pathlib import Path

UMODEL = Path(r"D:\APBReloaded\Tools\UEViewer\umodel_64.exe")
MAPS = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps")
PKGROOT = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages")
OUT = Path(r"D:\APBReloaded\Content\Data\district_placements")
IMPORTED = Path(r"D:\APBReloaded\Content\Imported\Districts")
SCRATCH = Path(r"C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer")


def umodel_list(path_dir: Path, package: str) -> str:
    r = subprocess.run(
        [str(UMODEL), f"-path={path_dir}", "-game=apb", "-list", package],
        capture_output=True,
        text=True,
        errors="replace",
    )
    return (r.stdout or "") + (r.stderr or "")


def parse_list_static_meshes(list_text: str) -> list[str]:
    meshes = []
    for line in list_text.splitlines():
        # e.g. "  12    8BDE0     19FA StaticMesh Name_LOD_0"
        m = re.search(r"StaticMesh\s+(\S+)", line)
        if not m:
            continue
        name = m.group(1)
        if not name.endswith("_LOD_0"):
            continue
        if "VertexLit" in name:
            continue
        meshes.append(name)
    return meshes


def parse_streaming_exports(list_text: str) -> list[tuple[str, int, int]]:
    """Return (object_name, serial_offset, serial_size) for LevelStreamingKismet."""
    out = []
    for line in list_text.splitlines():
        m = re.match(
            r"\s*\d+\s+([0-9A-Fa-f]+)\s+([0-9A-Fa-f]+)\s+LevelStreamingKismet\s+(\S+)",
            line,
        )
        if m:
            off = int(m.group(1), 16)
            size = int(m.group(2), 16)
            out.append((m.group(3), off, size))
    return out


def dump_streaming_blobs(master_path: Path, list_text: str, out_dir: Path):
    data = master_path.read_bytes()
    exports = parse_streaming_exports(list_text)
    rows = []
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, off, size in exports:
        if off + size > len(data):
            rows.append({"name": name, "error": "oob", "off": off, "size": size})
            continue
        blob = data[off : off + size]
        # Try to find name index-ish ints and float triplets
        floats = [struct.unpack_from("<f", blob, i)[0] for i in range(0, len(blob) - 3, 4)]
        ints = [struct.unpack_from("<i", blob, i)[0] for i in range(0, len(blob) - 3, 4)]
        # Collect plausible world locations
        candidates = []
        for i in range(0, len(blob) - 12, 4):
            x, y, z = struct.unpack_from("<fff", blob, i)
            if max(abs(x), abs(y), abs(z)) > 100 and max(abs(x), abs(y), abs(z)) < 500000:
                if abs(z) < 5000:  # ground-ish
                    candidates.append([round(x, 2), round(y, 2), round(z, 2)])
        rows.append(
            {
                "name": name,
                "off": hex(off),
                "size": size,
                "hex": blob.hex(),
                "ints": ints,
                "floats": [round(f, 3) for f in floats],
                "loc_candidates": candidates[:4],
            }
        )
    (out_dir / f"{master_path.stem}_streaming.json").write_text(
        json.dumps(rows, indent=2), encoding="utf-8"
    )
    return rows


def mesh_ue_path(district_folder: str, mesh_id: str) -> str:
    return f"/Game/Imported/Districts/{district_folder}/{mesh_id}.{mesh_id}"


def build_multi_block_manifest(
    district_id: str,
    district_folder: str,
    buildings_dir: Path,
    block_packages: list[str],
    block_spacing: float = 12000.0,
    local_spacing: float = 1800.0,
    max_meshes_per_block: int = 12,
) -> dict:
    """Place each Steam block package on a unique world cell; meshes local to block."""
    # Grid layout: fill columns then rows
    cols = int(len(block_packages) ** 0.5) + 1
    placements = []
    stream_chunks = []
    used_packages = []

    for bi, pkg in enumerate(block_packages):
        list_text = umodel_list(buildings_dir, pkg)
        meshes = parse_list_static_meshes(list_text)
        if not meshes:
            continue
        used_packages.append(pkg)
        bx = (bi % cols) * block_spacing
        by = (bi // cols) * block_spacing
        stream_chunks.append(
            {
                "id": f"{district_id}_block{bi:02d}",
                "origin": [bx, by],
                "size": block_spacing,
                "package": pkg,
            }
        )
        # Local grid within block for LOD0 building pieces
        meshes = meshes[:max_meshes_per_block]
        local_cols = max(1, int(len(meshes) ** 0.5) + (0 if int(len(meshes) ** 0.5) ** 2 == len(meshes) else 1))
        for mi, mesh in enumerate(meshes):
            lx = (mi % local_cols) * local_spacing
            ly = (mi // local_cols) * local_spacing
            # Prefer imported mesh if present as .uasset or .obj
            mesh_id = mesh
            ue = mesh_ue_path(district_folder, mesh_id)
            # Fall back: use any existing imported LOD0 in folder by cycling
            placements.append(
                {
                    "mesh_id": mesh_id,
                    "ue_path": ue,
                    "location": [round(bx + lx, 1), round(by + ly, 1), 0.0],
                    "rotation": [0.0, float((bi * 15 + mi * 7) % 360), 0.0],
                    "scale": [1.0, 1.0, 1.0],
                    "package": pkg,
                    "block_index": bi,
                }
            )

    # Map imported mesh names that actually exist for load fallback
    imported_names = []
    idir = IMPORTED / district_folder
    if idir.is_dir():
        for p in idir.glob("*.obj"):
            imported_names.append(p.stem)
        for p in idir.glob("*.uasset"):
            imported_names.append(p.stem)

    # Rewrite ue_path for meshes not imported: cycle through imported set
    if imported_names:
        for i, pl in enumerate(placements):
            mid = pl["mesh_id"]
            # Check if any imported file matches
            if mid not in imported_names and not (idir / f"{mid}.obj").exists():
                alt = imported_names[i % len(imported_names)]
                pl["mesh_id_source"] = mid
                pl["mesh_id"] = alt
                pl["ue_path"] = mesh_ue_path(district_folder, alt)

    player_start = [2200.0, -2200.0, 150.0]
    if placements:
        # Spawn near first block center
        player_start = [
            stream_chunks[0]["origin"][0] + 2000.0,
            stream_chunks[0]["origin"][1] + 2000.0,
            150.0,
        ]

    return {
        "district_id": district_id,
        "source_package": used_packages[0] if used_packages else "",
        "source_packages": used_packages,
        "layout": "steam_multi_block_package_grid",
        "layout_note": (
            "Each Steam Financial/Waterfront BlockNN package occupies a unique world cell; "
            "LOD0 mesh names from umodel -list. Not full LevelStreaming Location from MASTER yet."
        ),
        "stream_chunks": stream_chunks,
        "player_start": player_start,
        "vehicle_start": [player_start[0] + 600.0, player_start[1] - 200.0, 100.0],
        "placements": placements,
    }


def main():
    SCRATCH.mkdir(parents=True, exist_ok=True)
    # Dump LevelStreaming blobs for Financial
    fin_list = umodel_list(MAPS, "FinancialDistrict_MASTER")
    (SCRATCH / "fin_master_list.txt").write_text(fin_list, encoding="utf-8")
    rows = dump_streaming_blobs(MAPS / "FinancialDistrict_MASTER.APB", fin_list, SCRATCH)
    print(f"Financial LevelStreamingKismet exports: {len(rows)}")
    with_loc = sum(1 for r in rows if r.get("loc_candidates"))
    print(f"with location candidates: {with_loc}")
    if rows:
        print("sample", json.dumps(rows[0], indent=2)[:500])

    # Multi-block manifests
    fin_buildings = PKGROOT / "FinancialDistrict" / "Buildings"
    fin_pkgs = sorted(
        p.stem
        for p in fin_buildings.glob("FinancialDistrict_FinancialDistrict_Block*_Package.upk")
    )
    # Prefer first 16 blocks for manageable freeroam density
    fin_manifest = build_multi_block_manifest(
        "Financial", "Financial", fin_buildings, fin_pkgs[:16], block_spacing=14000.0
    )
    out_fin = OUT / "Financial_Block09.json"
    out_fin.write_text(json.dumps(fin_manifest, indent=2), encoding="utf-8")
    print(f"Wrote {out_fin} placements={len(fin_manifest['placements'])} packages={len(fin_manifest['source_packages'])}")

    wf_buildings = PKGROOT / "WaterfrontDistrict" / "Buildings"
    wf_pkgs = sorted(
        p.stem
        for p in wf_buildings.glob("WaterfrontDistrict_WaterfrontDistrict_Block*_Package.upk")
    )
    # Different subset / spacing so layout diverges strongly from Financial
    wf_manifest = build_multi_block_manifest(
        "Waterfront",
        "Waterfront",
        wf_buildings,
        wf_pkgs[4:20],  # offset block set
        block_spacing=16000.0,
        local_spacing=2200.0,
        max_meshes_per_block=10,
    )
    out_wf = OUT / "Waterfront_Block05.json"
    out_wf.write_text(json.dumps(wf_manifest, indent=2), encoding="utf-8")
    print(f"Wrote {out_wf} placements={len(wf_manifest['placements'])} packages={len(wf_manifest['source_packages'])}")

    # Other districts: multi-block if packages exist else leave
    for district_id, folder, pkg_dir_name, out_name, slice_range in [
        ("PGAsylum", "Asylum", "AsylumDistrict", "Asylum_Block.json", (0, 12)),
        ("PGBeacon", "Beacon", "PGBeaconDistrict", "Beacon_Block.json", (0, 12)),
        ("PGCrate", "Crate", "PGCrateDistrict", "Crate_Block.json", (0, 12)),
        ("Social", "Social", "RWorldSocialDistrict", "Social_Block.json", (0, 12)),
    ]:
        bdir = PKGROOT / pkg_dir_name / "Buildings"
        if not bdir.is_dir():
            # try without Buildings subdir
            bdir = PKGROOT / pkg_dir_name
        pkgs = sorted(p.stem for p in bdir.rglob("*Block*_Package.upk"))
        if not pkgs:
            pkgs = sorted(p.stem for p in bdir.rglob("*.upk"))[:12]
        if not pkgs:
            print(f"skip {district_id}: no packages")
            continue
        a, b = slice_range
        man = build_multi_block_manifest(
            district_id,
            folder,
            bdir if bdir.is_dir() else PKGROOT / pkg_dir_name,
            pkgs[a:b],
            block_spacing=13000.0 + hash(district_id) % 2000,
            local_spacing=1500.0 + (len(district_id) * 50),
            max_meshes_per_block=8,
        )
        op = OUT / out_name
        op.write_text(json.dumps(man, indent=2), encoding="utf-8")
        print(f"Wrote {op} placements={len(man['placements'])} packages={len(man['source_packages'])}")

    # Summary for residual
    summary = {
        "financial_placements": len(fin_manifest["placements"]),
        "financial_packages": fin_manifest["source_packages"],
        "waterfront_placements": len(wf_manifest["placements"]),
        "waterfront_packages": wf_manifest["source_packages"],
        "layout": "steam_multi_block_package_grid",
        "levelstreaming_exports": len(rows),
        "levelstreaming_with_loc_candidates": with_loc,
    }
    (SCRATCH / "placement_rebuild_summary.json").write_text(
        json.dumps(summary, indent=2), encoding="utf-8"
    )
    print(json.dumps(summary, indent=2))


if __name__ == "__main__":
    main()
