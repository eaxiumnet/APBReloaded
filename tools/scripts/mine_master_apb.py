"""String-mine and light-parse APB MASTER maps for LevelStreaming package names."""
import re
import struct
import sys
from pathlib import Path

MAPS = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps")


def mine(path: Path):
    data = path.read_bytes()
    print(f"=== {path.name} size={len(data)} ===")
    strings = re.findall(rb"[\x20-\x7e]{4,120}", data)
    interesting = []
    seen = set()
    for s in strings:
        t = s.decode("ascii", errors="ignore")
        if any(
            k in t
            for k in (
                "Block",
                "Financial",
                "Waterfront",
                "Package",
                "District",
                "Road",
                "Terrain",
                "Building",
                "Asylum",
                "Beacon",
                "Crate",
                "Social",
                "LevelStreaming",
            )
        ):
            if t not in seen:
                seen.add(t)
                interesting.append(t)
    print(f"unique interesting strings: {len(interesting)}")
    for t in interesting:
        print(" ", t)

    # Heuristic: find FString-like package refs near float triplets (Location)
    # Look for BlockNN patterns and index of occurrence; scan nearby floats
    block_hits = []
    for m in re.finditer(rb"[\w]+Block\d{2}[\w]*", data):
        name = m.group().decode("ascii", "ignore")
        off = m.start()
        # scan 256 bytes after for 3 consecutive floats that look like world coords
        window = data[off : off + 256]
        floats = []
        for i in range(0, len(window) - 12, 4):
            try:
                f = struct.unpack_from("<f", window, i)[0]
            except struct.error:
                continue
            if abs(f) < 1e6 and (abs(f) > 10 or f == 0.0):
                floats.append((i, f))
        # find triples of reasonable spacing
        for i in range(len(floats) - 2):
            a, b, c = floats[i][1], floats[i + 1][1], floats[i + 2][1]
            if floats[i + 2][0] - floats[i][0] == 8:  # consecutive
                if max(abs(a), abs(b), abs(c)) > 50:
                    block_hits.append((name, a, b, c, off))
                    break
    print(f"block float-heuristic hits: {len(block_hits)}")
    for h in block_hits[:30]:
        print(" ", h)


def main():
    targets = [
        "FinancialDistrict_MASTER.APB",
        "WaterfrontDistrict_MASTER.APB",
        "AsylumDistrict_MASTER.APB",
        "PGBeaconDistrict_MASTER.APB",
        "PGCrateDistrict_MASTER.apb",
        "RWorldSocialDistrict_MASTER.APB",
    ]
    for name in targets:
        p = MAPS / name
        if p.exists():
            mine(p)
        else:
            print("missing", name)


if __name__ == "__main__":
    main()
