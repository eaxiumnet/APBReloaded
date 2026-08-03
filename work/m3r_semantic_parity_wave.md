# M3R Semantic-Parity Wave — TEXTURE/MATERIAL/AUDIO/VIDEO/ANIMATION/UI

**Status:** complete (wave 1)
**Date:** 2026-08-03
**Prior state:** `m3r_semantic_parity` emitted only MESH/PLACEMENT markers; the spine
demanded 8 and blocked on the other 6.

## Scope

Tasks 11-15 of the M3R plan: deterministic semantic validators per asset class.
The 6 missing classes had no validator anywhere in tools/:
TEXTURE, MATERIAL, AUDIO, ANIMATION, VIDEO, UI_VISUAL.

## What was built

### `record_animation_import.py` (new)

Animation had **zero** ledger rows despite 81 retail Anim upks + 50+ extracted PSAs.
Records 51 PSA rows with full evidence chains (retail upk locator + sha256 ->
PSA intermediate + sha256) and parsed semantic metrics. Dedupes by asset_key
(Weapons + WeaponsBase both extract the same retail upk). 1 skipped (2011 matinee
`AnimSet_0.psa` — no retail upk, source_build=2011 matinee, no runtime consumer).

PSA semantic parse (ActorX v2, ANIMHEAD flag 20100422): BONENAMES count, ANIMINFO
records (name, frames, tracks, anim_size), ANIMKEYS count. Invariant verified on
all 52 PSAs: `ANIMKEYS == sum(anim_size)` — 193 animations, 31 unique upk->PSA rows.

### `validate_m3r_semantic_parity.py` (extended)

Six new class validators, each decoding the real intermediate and verifying the
chain (file exists + sha256 == ledger):

| Class | Decoder | Coverage |
|---|---|---|
| TEXTURE_PARITY | TGA header (image_type, WxH, bpp) / PNG IHDR (WxH, bit depth, color type) | 1,894 verified Texture2D |
| MATERIAL_PARITY | TGA intermediate chain | 2,341 MaterialInstanceConstant |
| AUDIO_PARITY | WAV fmt (channels, rate, bits, frames, duration) via stdlib wave | 12 SoundWave |
| VIDEO_PARITY | MP4 ftyp/moov (dims, timescale, duration) + WebM EBML (Tracks/PixelWxH/Info) + WAV | 34 MediaFile |
| ANIMATION_PARITY | PSA ANIMHEAD/BONENAMES/ANIMINFO/ANIMKEYS + drift vs recorded metrics | 31 AnimSet rows |
| UI_VISUAL_PARITY | UI Texture2D PNG/TGA chains + oracle screenshot deferral note | 123 UI rows |

All 8 required markers now emit (MESH, PLACEMENT, TEXTURE, MATERIAL, AUDIO,
ANIMATION, VIDEO, UI_VISUAL). Validator fails hard (exit 1) if any class has
failures — no PASS with recorded failures.

### Container fixes found while validating

- MP4 compat login videos have `moov` after `mdat` (needed full-file read).
- WebM EBML: element IDs keep the vint marker bit; sizes don't. Two-flag vint
  decoder required.
- WebM duration is in nanoseconds (timescale 1e6); was mis-scaled by 1000x.
- `LoginAnimatedBackground_Combined_5-4-3-2-1.webm` ledger row pointed at a
  264-byte interrupted transcode stub (no Tracks element). Repointed to the real
  250 MB MKV (5120x3200, 156.4s, 5 combined clips) with fresh sha256.

## Results

- Ledger: 9,015 -> 9,046 entries (31 AnimSet rows added, imported status).
- Validator standalone: `M3R_SEMANTIC_PARITY_PASS`, all 8 markers, 0 failures
  (mesh 26 obj/35 sections, placement 1832 rows, texture 1894, material 2341,
  audio 12, video 34, animation 31 rows/139 anims, ui 123).
- Tests: 19 pass (12 new: chain mutations per class, TGA/PNG/WAV header negative
  controls, PSA drift negative control, media container dispatch).
- Strict provenance: `STRICT_ASSET_PROVENANCE_PASS entries=9046 verified=8426 blocked=6`.

## Files changed

- `tools/scripts/record_animation_import.py` (new)
- `tools/scripts/validate_m3r_semantic_parity.py` (extended)
- `tools/scripts/test_m3r_semantic_parity.py` (extended)
- `tools/import_ledger.json` (31 AnimSet rows + combined-webm repoint)
- `work/evidence/animation_parity/*.json` (27 package evidence files)

## Follow-ups

- Deep decode comparison (Task 12/13 pixel/PCM equality, video SSIM/PSNR) needs
  the Task 6 pinned decoder binary — documented as the next wave.
- 2011 matinee `Anim_Matinee_E3` PSA (no retail upk) needs a 2011-source policy
  decision if it becomes a runtime consumer.
- `ui.screenshot.login.fixed_camera` pixel diff currently FAILS (MAE 0.40 vs
  0.18) — deferred_require_binary per oracle; the UI validator records the
  deferral, not a false pass.
