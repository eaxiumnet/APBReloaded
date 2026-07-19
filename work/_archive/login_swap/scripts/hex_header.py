from pathlib import Path


def hex_dump(path: Path, limit: int = 256) -> None:
    data = path.read_bytes()[:limit]
    print(f"=== {path.name} ({len(path.read_bytes())} bytes) ===")
    for i in range(0, len(data), 16):
        chunk = data[i:i+16]
        hex_part = ' '.join(f'{b:02x}' for b in chunk)
        ascii_part = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print(f"{i:08x}  {hex_part:<48}  {ascii_part}")


if __name__ == '__main__':
    hex_dump(Path(r"D:\APBReloaded\work\login_swap\decompressed\2011\unpacked\APBLoginLevel.apb"))
    print()
    hex_dump(Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps\APBLoginLevel.apb"))
