import struct
from pathlib import Path


def dump(path: Path) -> None:
    data = path.read_bytes()
    print(f"=== {path.name} ({len(data)} bytes) ===")
    pos = 0
    tag, = struct.unpack_from('<I', data, pos); pos += 4
    file_version, = struct.unpack_from('<H', data, pos); pos += 2
    licensee_version, = struct.unpack_from('<H', data, pos); pos += 2
    print(f"Magic: 0x{tag:08X}, Version: {file_version}, Licensee: {licensee_version}")

    headers_size, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"HeadersSize: {headers_size}")

    pkg_group_len, = struct.unpack_from('<i', data, pos)
    print(f"PackageGroupLen: {pkg_group_len}")
    pos += 4
    if pkg_group_len < 0:
        pos += -pkg_group_len * 2
    elif pkg_group_len > 0:
        pos += pkg_group_len

    package_flags, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"PackageFlags: 0x{package_flags:08X}")

    name_count, = struct.unpack_from('<I', data, pos); pos += 4
    name_offset, = struct.unpack_from('<I', data, pos); pos += 4
    export_count, = struct.unpack_from('<I', data, pos); pos += 4
    export_offset, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"Names: {name_count} @ 0x{name_offset:08X}")
    print(f"Exports: {export_count} @ 0x{export_offset:08X}")

    if licensee_version >= 29:
        pos += 4
    if licensee_version >= 28:
        pos += 20

    import_count, = struct.unpack_from('<I', data, pos); pos += 4
    import_offset, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"Imports: {import_count} @ 0x{import_offset:08X}")

    if file_version >= 415:
        depends_offset, = struct.unpack_from('<I', data, pos); pos += 4
        print(f"DependsOffset: 0x{depends_offset:08X}")
    if file_version >= 584:
        unk38, = struct.unpack_from('<i', data, pos); pos += 4
        print(f"Unk38: {unk38}")

    guid = data[pos:pos+16]; pos += 16
    print(f"GUID: {guid.hex()}")

    generation_count, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"GenerationCount: {generation_count}")

    if licensee_version >= 32:
        guid2 = data[pos:pos+16]; pos += 16
        print(f"GUID2: {guid2.hex()}")

    pos += generation_count * 12

    if file_version >= 245:
        engine_version, = struct.unpack_from('<I', data, pos); pos += 4
        print(f"EngineVersion: {engine_version}")
    if file_version >= 277:
        cooker_version, = struct.unpack_from('<I', data, pos); pos += 4
        print(f"CookerVersion: {cooker_version}")

    compression_flags, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"CompressionFlags: {compression_flags}")
    chunk_count, = struct.unpack_from('<I', data, pos); pos += 4
    print(f"ChunkCount: {chunk_count}")


if __name__ == '__main__':
    dump(Path(r"C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages\Interface\APBMenus_Art_GameFlowScenes.upk"))
    print()
    dump(Path(r"D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America\APBGame\Content\Interface\APBMenus_Art_GameFlowScenes.upk"))
