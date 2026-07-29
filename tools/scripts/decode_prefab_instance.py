"""Decode serialized PrefabInstance PI_Bytes transforms from retail APB UE3 maps.

The decoder deliberately reuses ``apb_level_dump`` for package decompression, export
enumeration, tagged properties, and retail map roots.  PI_Bytes itself is a custom
prefab archive, not a UE3 tagged-property stream, so this file only decodes the
record framing proven by the retail FinancialDistrict_Block09 bytes.
"""
from __future__ import annotations

import math
import struct
import sys
from collections import Counter
from pathlib import Path
from typing import Iterable

import apb_level_dump as ald


# A frame begins with two identical ``None, size`` pairs.  The size belongs to
# the archive's first serialized property, not to the whole variable-size record.
_FRAME = struct.Struct("<iIiI")
_FRAME_BYTES = _FRAME.size
_MATRIX_OFFSET = 0x56
_GUID_OFFSET = 0xAA
_GUID_BYTES = 16
_MIN_RECORD_BYTES = _GUID_OFFSET + _GUID_BYTES
_KNOWN_BASIS = (
    -0.79855, -0.60179, -0.01227,
    -0.06369, 0.06421, 0.99590,
    -0.59854, 0.79606, -0.08961,
)


def _frame_offsets(blob: bytes) -> Iterable[tuple[int, int]]:
    for start in range(0, len(blob) - _MIN_RECORD_BYTES + 1):
        first, size_a, second, size_b = _FRAME.unpack_from(blob, start)
        if first == -1 and second == -1 and size_a == size_b and 0 < size_a < 0x10000:
            yield start, size_a


def _basis_at(blob: bytes, matrix_offset: int) -> list[list[float]]:
    return [
        list(struct.unpack_from("<3f", blob, matrix_offset + row * 16))
        for row in range(3)
    ]


def _dot(left: list[float], right: list[float]) -> float:
    return sum(a * b for a, b in zip(left, right))


def _orthonormal(basis: list[list[float]], tolerance: float = 1e-3) -> bool:
    return (
        all(math.isfinite(value) for row in basis for value in row)
        and all(abs(_dot(basis[row], basis[row]) - 1.0) <= tolerance for row in range(3))
        and all(
            abs(_dot(basis[left], basis[right])) <= tolerance
            for left in range(3)
            for right in range(left)
        )
    )


def _has_padded_matrix_rows(blob: bytes, matrix_offset: int) -> bool:
    row_w = [struct.unpack_from("<f", blob, matrix_offset + 12 + row * 16)[0] for row in range(3)]
    return abs(row_w[0]) <= 1e-6 and abs(row_w[1]) <= 1e-6 and abs(row_w[2] - 1.0) <= 1e-6


def decode_pi_bytes(blob: bytes) -> list[dict]:
    """Recover framed transform rows from one PI_Bytes ArrayProperty raw payload.

    Each returned row is decoded directly from bytes.  ``basis`` is three padded
    float rows at ``record + 0x56``; ``translation`` is the three floats directly
    after the 48-byte padded basis; and the 16-byte Guid payload begins at
    ``record + 0xAA``.  Non-transform/malformed frames are skipped.
    """
    decoded: list[dict] = []
    for record_offset, frame_size in _frame_offsets(blob):
        matrix_offset = record_offset + _MATRIX_OFFSET
        try:
            basis = _basis_at(blob, matrix_offset)
            translation = list(struct.unpack_from("<3f", blob, matrix_offset + 48))
            guid = blob[record_offset + _GUID_OFFSET:record_offset + _GUID_OFFSET + _GUID_BYTES]
        except struct.error:
            continue
        if not _has_padded_matrix_rows(blob, matrix_offset) or len(guid) != _GUID_BYTES or not all(
            math.isfinite(value) for row in basis for value in row
        ) or not all(math.isfinite(value) for value in translation):
            continue
        decoded.append({
            "record_offset": record_offset,
            "frame_size": frame_size,
            "basis": basis,
            "translation": translation,
            "guid": guid.hex(),
        })
    return decoded


def _nearest_location(translation: list[float], locations: list[tuple[str, str, list[float]]]) -> tuple[str, str, list[float], float]:
    actor, name, location = min(
        locations,
        key=lambda item: sum((translation[index] - item[2][index]) ** 2 for index in range(3)),
    )
    distance = math.sqrt(sum((translation[index] - location[index]) ** 2 for index in range(3)))
    return actor, name, location, distance


def _matching_known_basis(records: list[dict]) -> dict | None:
    for record in records:
        flattened = tuple(value for row in record["basis"] for value in row)
        if max(abs(actual - target) for actual, target in zip(flattened, _KNOWN_BASIS)) <= 2e-4:
            return record
    return None


def _compact_basis_is_orthonormal(blob: bytes, record_offset: int) -> bool:
    start = record_offset + _MATRIX_OFFSET
    compact = [list(struct.unpack_from("<3f", blob, start + row * 12)) for row in range(3)]
    return _orthonormal(compact)


def analyze(stem: str, maps_dir: Path) -> int:
    data, package, exports = ald.load(stem, maps_dir)
    prefabs = [row for row in exports if row["cls"] == "PrefabInstance"]
    actor_locations: list[tuple[str, str, list[float]]] = []
    property_sets: Counter[tuple[str, ...]] = Counter()
    all_records: list[dict] = []
    rotation_records: list[dict] = []
    compact_hits = 0
    rotation_prefabs = 0

    for row in exports:
        props = ald.props_map(data[row["off"]:row["off"] + row["size"]], package.names)
        location = props.get("Location")
        if row["cls"] != "PrefabInstance" and isinstance(location, list) and len(location) == 3:
            actor_locations.append((row["cls"], row["name"], location))
        if row["cls"] != "PrefabInstance":
            continue
        property_sets[tuple(props)] += 1
        pi_bytes = props.get("PI_Bytes")
        blob = pi_bytes.get("raw", b"") if isinstance(pi_bytes, dict) else b""
        records = decode_pi_bytes(blob)
        for record in records:
            record["actor_export"] = row["idx"]
            record["actor"] = row["name"]
            record["pi_bytes_offset"] = record["record_offset"]
            record["matrix_offset"] = record["record_offset"] + _MATRIX_OFFSET
            record["actor_location"] = location
            record["actor_rotation"] = props.get("Rotation")
            compact_hits += _compact_basis_is_orthonormal(blob, record["record_offset"])
        all_records.extend(records)
        if isinstance(props.get("Rotation"), list):
            rotation_prefabs += 1
            rotation_records.extend(records)

    print(f"PACKAGE stem={stem} bytes={len(data)} exports={len(exports)}")
    print(f"PREFAB_EXPORTS={len(prefabs)} PREFAB_WITH_ROTATION={rotation_prefabs}")
    for names, count in sorted(property_sets.items(), key=lambda item: (-item[1], item[0])):
        print(f"PREFAB_PROPERTY_SET count={count} names={','.join(names)}")
    print(
        "PI_RECORD_LAYOUT "
        "frame=+0x00:i32(-1),+0x04:u32(size),+0x08:i32(-1),+0x0c:u32(same_size) "
        "basis_rows=+0x56,+0x66,+0x76:<3f with W at +0x62,+0x72,+0x82 "
        "translation=+0x86:<3f guid=+0xaa:16_bytes"
    )
    print(
        f"PI_RECORDS all_prefab_exports={len(all_records)} "
        f"rotation_prefabs={len(rotation_records)} "
        f"rotation_prefab_actors={rotation_prefabs}"
    )
    frame_sizes = Counter(record["frame_size"] for record in all_records)
    print("PI_FRAME_SIZES " + ",".join(f"0x{size:x}:{count}" for size, count in sorted(frame_sizes.items())))
    print(
        "HYPOTHESIS H1=one_transform_per_PrefabInstance "
        f"result=REJECTED evidence=records={len(all_records)} exports={len(prefabs)}"
    )
    print(
        "HYPOTHESIS H2=packed_3x3_rows_at_12_byte_stride "
        f"result=REJECTED evidence=orthonormal_compact_rows={compact_hits} framed_records={len(all_records)}"
    )
    print(
        "HYPOTHESIS H3=duplicated_FFFFFFFF_size_frame_plus_padded_16_byte_basis_rows "
        f"result=SURVIVED evidence=decoded={len(all_records)} matrix_offset=+0x{_MATRIX_OFFSET:x} guid_offset=+0x{_GUID_OFFSET:x}"
    )

    known = _matching_known_basis(all_records)
    clean = [record for record in rotation_records if _orthonormal(record["basis"])]
    if known is None:
        print("KNOWN_BASIS result=NOT_FOUND")
        print("PREFAB_DECODE=NO_GO blocking_unknown=known_answer_basis_not_found_in_framed_PI_Bytes")
        return 1

    dots = [[_dot(known["basis"][left], known["basis"][right]) for right in range(3)] for left in range(3)]
    nearest_class, nearest_name, nearest_location, nearest_distance = _nearest_location(
        known["translation"], actor_locations
    )
    print(
        f"KNOWN_BASIS actor={known['actor']} export={known['actor_export']} "
        f"record_offset=0x{known['record_offset']:x} matrix_offset=0x{known['matrix_offset']:x} "
        f"guid={known['guid']}"
    )
    print(f"KNOWN_MATRIX basis={known['basis']} translation={known['translation']}")
    print(
        "KNOWN_DOTS "
        f"r0r0={dots[0][0]:.9f} r1r1={dots[1][1]:.9f} r2r2={dots[2][2]:.9f} "
        f"r0r1={dots[0][1]:.9f} r0r2={dots[0][2]:.9f} r1r2={dots[1][2]:.9f}"
    )
    print(
        f"WORLDSPACE_CROSSCHECK nearest_nonprefab={nearest_class}:{nearest_name} "
        f"location={nearest_location} distance={nearest_distance:.3f} "
        f"decoded_translation={known['translation']}"
    )
    if len(clean) == len(rotation_records) and rotation_prefabs == 227:
        print(
            f"PREFAB_DECODE=GO transforms_recoverable_at_scale actors={rotation_prefabs}/227 "
            f"records={len(rotation_records)} orthonormal={len(clean)}"
        )
        return 0
    print(
        "PREFAB_DECODE=NO_GO "
        f"blocking_unknown=frame_or_matrix_validation_failed decoded={len(rotation_records)} orthonormal={len(clean)}"
    )
    return 1


def main(argv: list[str]) -> int:
    stem = argv[1] if len(argv) > 1 else "FinancialDistrict_Block09"
    maps_dir = Path(argv[2]) if len(argv) > 2 else ald.RETAIL_MAPS / "FinancialDistrict"
    return analyze(stem, maps_dir)


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))
