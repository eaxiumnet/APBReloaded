"""APB UPK inventory + name-hint miner. Run: python inventory_packages.py"""
from pathlib import Path
import json, re, struct, csv, time
from collections import Counter

PKG = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages")
OUT = Path(r"D:\APBReloaded\Content\Extracted")
INTERESTING = re.compile(r"(Mesh|Wheel|Body|Chassis|Weapon|Character|Contact|Vehicle|Socket|LOD|Anim|Skel|Texture|Material|Hub|Door|Bumper)", re.I)

def header(p: Path):
    with p.open("rb") as f:
        head = f.read(16)
    if len(head) < 8 or struct.unpack_from("<I", head, 0)[0] != 0x9E2A83C1:
        return None
    fv, lv = struct.unpack_from("<HH", head, 4)
    return fv, lv

def mine(p: Path, n=60000):
    data = p.read_bytes()[:n]
    out = set()
    for m in re.finditer(rb"[A-Za-z_][A-Za-z0-9_]{4,64}", data):
        s = m.group().decode("ascii", "ignore")
        if INTERESTING.search(s):
            out.add(s)
    return sorted(out)[:60]

def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rows = []
    t0 = time.time()
    for p in PKG.rglob("*.upk"):
        rel = str(p.relative_to(PKG)).replace("\\", "/")
        h = header(p)
        hints = mine(p) if p.stat().st_size >= 50000 else []
        rows.append((rel, Path(rel).parts[0], p.stat().st_size, h[0] if h else "", h[1] if h else "", "|".join(hints)))
    with (OUT / "package_inventory.csv").open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["rel_path", "folder", "size", "file_version", "licensee_version", "name_hints"])
        w.writerows(rows)
    print("rows", len(rows), "sec", round(time.time() - t0, 2))

if __name__ == "__main__":
    main()
