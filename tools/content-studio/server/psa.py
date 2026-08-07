"""ActorX v2 .psa animation parser (umodel export format).

Chunk layout (verified against umodel's Exporters/ExportPsk.cpp):

    ANIMHEAD   version 20100422 (stored in the chunk type flag)
    BONENAMES  VBone rows: name[64] + Flags(u32) + NumChildren(i32)
               + ParentIndex(i32) + Orientation(FQuat) + Position(FVector)
               + Length(f32) + Size(FVector)  -> 120 bytes
    ANIMINFO   AnimInfoBinary rows (168 bytes):
                 name[64], group[64],
                 TotalBones(i32)@128, RootInclude(i32)@132,
                 KeyCompressionStyle(i32)@136, KeyQuotum(i32)@140
                 (= frames*bones), KeyReduction(f32)@144,
                 TrackTime(f32)@148 (= num frames), AnimRate(f32)@152,
                 StartBone(i32)@156, FirstRawFrame(i32)@160,
                 NumRawFrames(i32)@164
    ANIMKEYS   VQuatAnimKey rows (32 bytes), frame-major:
                 Position(FVector)@0, Orientation(FQuat)@12, Time(f32)@28
                 ordered [frame0 bones0..N][frame1 bones0..N]...
    SCALEKEYS  empty in umodel exports

Pure stdlib (struct), no third-party deps.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass, field
from pathlib import Path

_CHUNK_HEADER = struct.Struct("<20s i i i")
_HEADER_SIZE = _CHUNK_HEADER.size  # 32

_ANIMINFO = struct.Struct("<64s64s" + "i" * 5 + "f" * 3 + "i" * 3)
# name[64] group[64] TotalBones RootInclude KeyCompressionStyle KeyQuotum
# KeyReduction(f) TrackTime(f) AnimRate(f) StartBone FirstRawFrame NumRawFrames

_ANIM_KEY = struct.Struct("<ffffffff")
# Position(3f) + Orientation(4f) + Time(1f)


def _iter_chunks(data: bytes):
    """Yield (name, dsize, count, payload) for each ActorX chunk."""
    pos = 0
    total = len(data)
    while pos + _HEADER_SIZE <= total:
        raw_name, _flags, dsize, count = _CHUNK_HEADER.unpack_from(data, pos)
        name = raw_name.split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
        pos += _HEADER_SIZE
        span = dsize * count
        if span < 0 or pos + span > total:
            raise ValueError(
                f"chunk {name!r} declares datasize*count={span} exceeding file "
                f"(pos={pos}, size={total})"
            )
        payload = data[pos : pos + span]
        pos += span
        yield name, dsize, count, payload


@dataclass
class PsaBone:
    name: str
    parent: int
    # bind pose (parent-relative) — usually zeroed in umodel PSA exports
    position: tuple[float, float, float]
    quat: tuple[float, float, float, float]


@dataclass
class PsaKey:
    position: tuple[float, float, float]
    quat: tuple[float, float, float, float]


@dataclass
class PsaAnimation:
    name: str
    rate: float
    num_frames: int
    # per-bone key lists: tracks[b] = [PsaKey(frame0..N-1)]
    tracks: list[list[PsaKey]] = field(default_factory=list)

    @property
    def duration(self) -> float:
        return self.num_frames / self.rate if self.rate > 0 else 0.0


@dataclass
class PsaAnimSet:
    bones: list[PsaBone]
    animations: list[PsaAnimation]

    @property
    def bone_names(self) -> list[str]:
        return [bone.name for bone in self.bones]


def parse_psa(data: bytes) -> PsaAnimSet:
    """Parse ActorX v2 PSA bytes into an :class:`PsaAnimSet`."""
    chunks = list(_iter_chunks(data))
    head = next((c for c in chunks if c[0] == "ANIMHEAD"), None)
    if head is None:
        raise ValueError("not an ActorX PSA (no ANIMHEAD chunk)")

    bones: list[PsaBone] = []
    for name, dsize, count, payload in chunks:
        if not name.startswith("BONENAMES") or dsize < 120:
            continue
        for i in range(count):
            off = i * dsize
            bone_name = payload[off : off + 64].split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
            parent = struct.unpack_from("<i", payload, off + 72)[0]
            qx, qy, qz, qw = struct.unpack_from("<ffff", payload, off + 76)
            px, py, pz = struct.unpack_from("<fff", payload, off + 92)
            bones.append(PsaBone(bone_name, parent, (px, py, pz), (qx, qy, qz, qw)))
        break

    anims_raw: list[tuple[str, int, int, float, float]] = []
    for name, dsize, count, payload in chunks:
        if name != "ANIMINFO" or dsize < 168:
            continue
        for i in range(count):
            off = i * dsize
            rec = payload[off : off + 168]
            anim_name = rec[0:64].split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
            total_bones, _root, _comp, key_quotum = struct.unpack_from("<iiii", rec, 128)
            _reduction, track_time, anim_rate = struct.unpack_from("<fff", rec, 144)
            _start, _first_frame, num_raw_frames = struct.unpack_from("<iii", rec, 156)
            anims_raw.append((anim_name, key_quotum, total_bones, anim_rate, track_time))
        break

    keys_payload = b""
    for name, dsize, count, payload in chunks:
        if name == "ANIMKEYS":
            keys_payload = payload
            break

    num_bones = len(bones)
    animations: list[PsaAnimation] = []
    cursor = 0
    for anim_name, key_quotum, total_bones, anim_rate, track_time in anims_raw:
        total_bones = total_bones or num_bones
        if total_bones == 0:
            continue
        if key_quotum > 0 and key_quotum % total_bones == 0:
            num_frames = key_quotum // total_bones
        else:
            num_frames = int(round(track_time)) if track_time > 0 else 0
        anim = PsaAnimation(name=anim_name, rate=anim_rate, num_frames=num_frames)
        for _b in range(total_bones):
            anim.tracks.append([])
        for f in range(num_frames):
            for b in range(total_bones):
                off = cursor * _ANIM_KEY.size
                if off + _ANIM_KEY.size > len(keys_payload):
                    raise ValueError(f"ANIMKEYS exhausted in anim {anim_name!r}")
                px, py, pz, qx, qy, qz, qw, _t = _ANIM_KEY.unpack_from(keys_payload, off)
                anim.tracks[b].append(PsaKey((px, py, pz), (qx, qy, qz, qw)))
                cursor += 1
        if anim.num_frames > 0:
            animations.append(anim)

    return PsaAnimSet(bones=bones, animations=animations)


def parse_psa_file(path: str | Path) -> PsaAnimSet:
    return parse_psa(Path(path).read_bytes())


def parse_psa_header(data: bytes) -> PsaAnimSet:
    """Parse only bones + AnimInfo records, skipping the ANIMKEYS payload.

    Same :class:`PsaAnimSet` shape but with empty tracks — used by the catalog
    where only clip names/frames/rate matter (65 animsets parse in ~2s instead
    of ~20s).
    """
    chunks = list(_iter_chunks(data))
    if not any(c[0] == "ANIMHEAD" for c in chunks):
        raise ValueError("not an ActorX PSA (no ANIMHEAD chunk)")

    bones: list[PsaBone] = []
    for name, dsize, count, payload in chunks:
        if not name.startswith("BONENAMES") or dsize < 120:
            continue
        for i in range(count):
            off = i * dsize
            bone_name = payload[off : off + 64].split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
            parent = struct.unpack_from("<i", payload, off + 72)[0]
            qx, qy, qz, qw = struct.unpack_from("<ffff", payload, off + 76)
            px, py, pz = struct.unpack_from("<fff", payload, off + 92)
            bones.append(PsaBone(bone_name, parent, (px, py, pz), (qx, qy, qz, qw)))
        break

    animations: list[PsaAnimation] = []
    for name, dsize, count, payload in chunks:
        if name != "ANIMINFO" or dsize < 168:
            continue
        for i in range(count):
            off = i * dsize
            rec = payload[off : off + 168]
            anim_name = rec[0:64].split(b"\0", 1)[0].decode("latin-1", errors="replace").strip()
            total_bones, _root, _comp, key_quotum = struct.unpack_from("<iiii", rec, 128)
            _reduction, track_time, anim_rate = struct.unpack_from("<fff", rec, 144)
            _start, _first_frame, _num_raw = struct.unpack_from("<iii", rec, 156)
            num_bones = total_bones or len(bones)
            if num_bones == 0:
                continue
            if key_quotum > 0 and key_quotum % num_bones == 0:
                num_frames = key_quotum // num_bones
            else:
                num_frames = int(round(track_time)) if track_time > 0 else 0
            anim = PsaAnimation(name=anim_name, rate=anim_rate, num_frames=num_frames)
            for _b in range(num_bones):
                anim.tracks.append([])
            if anim.num_frames > 0:
                animations.append(anim)
        break
    return PsaAnimSet(bones=bones, animations=animations)


def parse_psa_header_file(path: str | Path) -> PsaAnimSet:
    return parse_psa_header(Path(path).read_bytes())
