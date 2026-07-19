import struct
from pathlib import Path

for path in [r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\Binaries\APB.exe",
             r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries\APB.exe"]:
    with open(path, 'rb') as f:
        f.seek(60)
        pe_offset = struct.unpack('<I', f.read(4))[0]
        f.seek(pe_offset + 4)
        machine = struct.unpack('<H', f.read(2))[0]
    arch = {0x14c: 'x86 (32-bit)', 0x8664: 'x64 (64-bit)'}.get(machine, f'0x{machine:04X}')
    print(f"{path}: {arch}")
