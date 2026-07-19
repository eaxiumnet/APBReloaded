#!/usr/bin/env python3
"""Extract WEM media from a Wwise .bnk that uses DIDX+DATA (or DATA-only chunks)."""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path


def read_chunks(data: bytes) -> list[tuple[str, int, bytes]]:
    """Parse top-level BKHD-style chunks: 4cc + u32 size + payload."""
    chunks: list[tuple[str, int, bytes]] = []
    off = 0
    n = len(data)
    while off + 8 <= n:
        tag = data[off : off + 4]
        if tag == b"\x00\x00\x00\x00":
            break
        try:
            tag_s = tag.decode("ascii")
        except UnicodeDecodeError:
            # Some banks pad; stop if we leave printable fourcc land
            if not all(32 <= b < 127 for b in tag):
                break
            tag_s = tag.decode("latin-1")
        size = struct.unpack_from("<I", data, off + 4)[0]
        start = off + 8
        end = start + size
        if end > n:
            # truncated / wrong size — clamp
            end = n
        chunks.append((tag_s, off, data[start:end]))
        off = end
        # align? Wwise usually packs tightly without align for these chunks
    return chunks


def extract_didx_data(chunks: list[tuple[str, int, bytes]], out_dir: Path, only_ids: set[int] | None) -> int:
    didx = next((p for t, _, p in chunks if t == "DIDX"), None)
    data = next((p for t, _, p in chunks if t == "DATA"), None)
    if didx is None or data is None:
        return 0
    count = 0
    # Each entry: u32 id, u32 offset, u32 size
    for i in range(0, len(didx), 12):
        if i + 12 > len(didx):
            break
        wid, woff, wsize = struct.unpack_from("<III", didx, i)
        if only_ids is not None and wid not in only_ids:
            continue
        blob = data[woff : woff + wsize]
        if len(blob) != wsize:
            print(f"WARN short read id={wid} expected={wsize} got={len(blob)}", file=sys.stderr)
        out = out_dir / f"{wid}.wem"
        out.write_bytes(blob)
        print(f"wrote {out.name} ({len(blob)} bytes)")
        count += 1
    return count


def extract_raw_data_scan(data_payload: bytes, out_dir: Path, only_ids: set[int] | None) -> int:
    """Fallback: scan for RIFF/WAVE or pure WEM headers if DIDX missing."""
    # Not implemented deeply — return 0
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("bnk", type=Path)
    ap.add_argument("-o", "--out", type=Path, required=True)
    ap.add_argument("--id", type=int, action="append", dest="ids", help="Only extract this short ID (repeatable)")
    ap.add_argument("--list", action="store_true", help="List DIDX entries only")
    args = ap.parse_args()

    raw = args.bnk.read_bytes()
    if raw[:4] != b"BKHD":
        print(f"WARN: expected BKHD magic, got {raw[:4]!r}", file=sys.stderr)

    chunks = read_chunks(raw)
    print("chunks:", ", ".join(f"{t}({len(p)})" for t, _, p in chunks))

    didx = next((p for t, _, p in chunks if t == "DIDX"), None)
    if didx is not None:
        entries = []
        for i in range(0, len(didx), 12):
            if i + 12 > len(didx):
                break
            wid, woff, wsize = struct.unpack_from("<III", didx, i)
            entries.append((wid, woff, wsize))
        print(f"DIDX entries: {len(entries)}")
        if args.list:
            for wid, woff, wsize in sorted(entries, key=lambda e: -e[2])[:40]:
                print(f"  id={wid} size={wsize} off={woff}")
            # also show specific id if requested
            if args.ids:
                for want in args.ids:
                    hit = [e for e in entries if e[0] == want]
                    print(f"lookup {want}: {hit}")
            return 0

    only = set(args.ids) if args.ids else None
    args.out.mkdir(parents=True, exist_ok=True)
    n = extract_didx_data(chunks, args.out, only)
    if n == 0 and only:
        print("No matching DIDX entries for requested IDs", file=sys.stderr)
        return 2
    if n == 0:
        print("No DIDX+DATA extraction performed", file=sys.stderr)
        return 1
    print(f"extracted {n} file(s) -> {args.out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
