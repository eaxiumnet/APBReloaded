from pathlib import Path


def main():
    retail = Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Maps\APBLoginLevel.apb").read_bytes()
    print(f"Retail size: {len(retail)}")
    for off in [0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D, 0x61]:
        print(f"Offset {off:08x}: {retail[off:off+16].hex()}")


if __name__ == '__main__':
    main()
