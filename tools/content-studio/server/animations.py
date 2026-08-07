"""Animation catalog + clip alignment for the content studio backend.

APB character anims live as ActorX .psa files under
``Content/Extracted/Retail/Animations/<Group>/AnimSet/*.psa``. Each PSA holds
one animset: a bone-name table (BONENAMES) plus one animation per AnimInfo
record, each with per-bone QuatPos keyframes (see psa.py).

Skeleton compatibility: a character body PSK (e.g. F_Body_Base) carries a
REFSKELT with ~139 bones; the animset has ~143 (extra hair/beard bones). The
orders diverge, so clips are aligned to the body skeleton by bone *name*:
bones with a matching PSA track animate, everything else holds its bind pose.
"""

from __future__ import annotations

from functools import lru_cache
from pathlib import Path

from psa import PsaAnimSet, PsaKey, parse_psa_file, parse_psa_header_file

_PSA_EXTS = {".psa"}


def _find_animsets(root: Path) -> list[Path]:
    """Directories named ``AnimSet`` that contain at least one .psa file."""
    animsets = []
    if not root.is_dir():
        return animsets
    for animset_dir in root.rglob("AnimSet"):
        if not animset_dir.is_dir():
            continue
        if any(p.suffix.casefold() in _PSA_EXTS for p in animset_dir.iterdir()):
            animsets.append(animset_dir)
    return sorted(animsets, key=lambda p: p.as_posix())


@lru_cache(maxsize=64)
def load_animset_cached(path: str | Path) -> PsaAnimSet:
    """Full PSA parse (keyframes included), cached across clip requests.

    The viewer fetches one clip per request; without the cache every clip
    click re-parses the whole animset (up to ~0.5s for locomotion sets).
    """
    return parse_psa_file(Path(path))


@lru_cache(maxsize=64)
def _parse_header_cached(path: str) -> PsaAnimSet:
    return parse_psa_header_file(path)


def _animset_display_name(animset_dir: Path, root: Path) -> str:
    rel = animset_dir.relative_to(root)
    parts = rel.parts
    # Retail/Animations/<Group>/AnimSet -> <Group>
    return parts[0] if parts else animset_dir.name


def build_animation_catalog(root: Path) -> list[dict]:
    """List every animset with clip metadata (no keyframes)."""
    catalog = []
    for animset_dir in _find_animsets(root):
        psa_files = sorted(p for p in animset_dir.iterdir() if p.suffix.casefold() in _PSA_EXTS)
        if not psa_files:
            continue
        psa_path = psa_files[0]
        try:
            animset = _parse_header_cached(str(psa_path))
        except Exception:
            continue
        catalog.append({
            "id": animset_dir.name,
            "display": _animset_display_name(animset_dir, root),
            "relpath": str(psa_path.relative_to(root)).replace("\\", "/"),
            "bone_count": len(animset.bones),
            "clips": [
                {
                    "name": anim.name,
                    "frames": anim.num_frames,
                    "rate": anim.rate,
                    "duration": round(anim.duration, 3),
                    "tracks": sum(1 for t in anim.tracks if t),
                }
                for anim in animset.animations
            ],
        })
    return sorted(catalog, key=lambda entry: entry["display"].casefold())


def resolve_animset(root: Path, relpath: str) -> Path:
    """Resolve a catalog relpath to an absolute PSA path, rejecting traversal."""
    base = root.resolve()
    target = (base / relpath).resolve()
    if target != base and not str(target).startswith(str(base) + "/") \
            and not str(target).startswith(str(base) + "\\"):
        raise ValueError("path escapes animation root")
    if target.suffix.casefold() not in _PSA_EXTS:
        raise ValueError(f"not a PSA file: {relpath}")
    if not target.is_file():
        raise FileNotFoundError(relpath)
    return target


def align_clips_to_skeleton(
    animset: PsaAnimSet, skeleton_names: list[str]
) -> list[dict]:
    """Convert every animation to the exporter clip shape, aligned by name.

    Each returned clip: {name, rate, num_frames, tracks: [per-skeleton-bone
    list of PsaKey | None]}. PSA bones with no skeleton counterpart (hair,
    props) are dropped; skeleton bones without a PSA track hold bind pose.
    """
    clips = []
    for anim in animset.animations:
        track_by_name = dict(zip((b.name for b in animset.bones), anim.tracks))
        tracks = [track_by_name.get(name) for name in skeleton_names]
        clips.append({
            "name": anim.name,
            "rate": anim.rate,
            "num_frames": anim.num_frames,
            "tracks": tracks,
        })
    return clips


def rebase_clips_to_skeleton(clips: list[dict], skeleton) -> list[dict]:
    """Re-anchor animation keys from the clip start pose to the skeleton bind.

    PSA keys are absolute parent-relative transforms authored against the
    animation reference rig, whose start pose sits 50-160cm from the character
    geometry (the PSK REFSKELT and even the reconstructed bind differ from it).
    Playing the raw keys makes the mesh jump to the rig rest pose and stretch
    into ribbons. Expressing each key as a delta from the clip frame 0 and
    composing that delta onto the skeleton's bind locals keeps the mesh at its
    authored geometry pose at frame 0 and plays the clip's relative motion
    cleanly. The root translation is pinned to the bind so locomotion clips
    animate in place.
    """

    def qconj(q):
        return (-q[0], -q[1], -q[2], q[3])

    def qmul(a, b):
        ax, ay, az, aw = a
        bx, by, bz, bw = b
        return (
            aw * bx + ax * bw + ay * bz - az * by,
            aw * by + ay * bw + az * bx - ax * bz,
            aw * bz + az * bw + ax * by - ay * bx,
            aw * bw - ax * bx - ay * by - az * bz,
        )

    def qrot(v, q):
        qx, qy, qz, qw = q
        ix = qw * v[0] + qy * v[2] - qz * v[1]
        iy = qw * v[1] + qz * v[0] - qx * v[2]
        iz = qw * v[2] + qx * v[1] - qy * v[0]
        iw = -qx * v[0] - qy * v[1] - qz * v[2]
        return (
            ix * qw - iw * qx - iy * qz + iz * qy,
            iy * qw - iw * qy - iz * qx + ix * qz,
            iz * qw - iw * qz - ix * qy + iy * qx,
        )

    def vsub(a, b):
        return (a[0] - b[0], a[1] - b[1], a[2] - b[2])

    def vadd(a, b):
        return (a[0] + b[0], a[1] + b[1], a[2] + b[2])

    out = []
    for clip in clips:
        tracks = []
        for i, bone in enumerate(skeleton):
            track = clip["tracks"][i] if i < len(clip["tracks"]) else None
            if not track:
                tracks.append(None)
                continue
            q0, p0 = track[0].quat, track[0].position
            qb, pb = bone.quat, bone.position
            q0c = qconj(q0)
            root_bone = bone.parent < 0 or bone.parent >= len(skeleton) or bone.parent == i
            keys = []
            for key in track:
                qrel = qmul(q0c, key.quat)
                prel = qrot(vsub(key.position, p0), q0c)
                qnew = qmul(qb, qrel)
                pnew = pb if root_bone else vadd(pb, qrot(prel, qb))
                keys.append(PsaKey(pnew, qnew))
            tracks.append(keys)
        out.append({**clip, "tracks": tracks})
    return out
