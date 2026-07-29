# Debugging/Status Note — 5 external-texture weapons + editor-build gate

Date: 2026-07-21 ~10:05  ·  Author: audit/ulw pass  ·  Per AGENTS.md rule 5 (STOP after 2 loops, write note)

Two todos remained (7/9 done). Both are **externally blocked**, not incomplete. Neither can be
truthfully marked complete, and neither can be forced without violating standing constraints.

## Task 1 — 5 external-texture weapons (VAS ×4 + MobilePhone): "broad-path re-extract"

**Ground truth established (read-only, umodel `-list`, bounded FS checks):**

- Manifest `work/weapon_import_manifest.json` has **235 entries**; exactly **5** carry
  `external_textures=true`:
  - `Weapon_AssaultRifle_VAS` — 4 mesh variants: `VASSW2_LOD0`, `VASC2_LOD0`, `VASR2_LOD0`, `VASPR2_LOD0`
  - `Weapon_MobilePhone` — `MobilePhone_LOD0`
- These 5 have **empty** `material_slots` / `spec_power` / `textures` (that is *why* they're flagged external).
- **Meshes already extracted**: `.obj` present under `Content\Extracted\Weapons_obj\<pkg>\` (4 objs each). No `.mtl` beside them.
- **Textures are NOT in the mesh packages** — `umodel -list` confirms:
  - `Weapon_AssaultRifle_VAS.upk` (Ver 564/33, 47 exports) — AnimNodeSequence/mesh, **no Texture2D**.
  - `Weapon_MobilePhone.upk` (Ver 564/33, 9 exports) — **no Texture2D**.
  - `Weapon_Vas.upk` — holds `Weapon_Vas_UrbanSpecOps_MAT_INST` / `Weapon_Vas_WinterSpecOps_MAT_INST`
    (skin-variant MICs) + SkeletalMeshActors; the actual Texture2D objects are in a **separate
    shared package not yet located**.
- Dest `.uasset` **not on disk** (`/Game/Imported/Weapons/Weapon_AssaultRifle_VAS`, `.../Weapon_MobilePhone` both MISSING).

**Why no forward progress is warranted now:**
1. Re-`-export`ing these packages yields meshes already in hand + **no textures** → theater, not progress.
2. Locating the shared texture package = the "broad-path re-extract" the master plan explicitly
   labels **deferred / low value** (4 of 5 are Urban/Winter SpecOps skin variants of one AR family + a phone prop; 5 of 792 weapons).
3. Even once extracted, the **import** step is gated behind the same editor build as Task 2.
4. 564/33 cooked packages "often fail stock umodel decompress" (per `extract_with_umodel.ps1` note).

**Unblock condition:** locate shared VAS/MobilePhone Texture2D package (broad-path umodel scan of
`APBGame\Content\Release\Packages\**` for the diffuse/normal/spec names referenced by the two SpecOps MICs),
THEN import via the gated commandlet. Low priority.

## Task 2 — GATE: editor DLL stale, blocked until clean editor build

- Editor DLL: `UnrealEditor-APBReloaded.dll` **07-20 20:43**.
- Newest source: `APBPersistence.h` **07-21 09:51** (edited ~6 min before this note).
- **14 Qoder processes live** → C++ still actively moving.
- Running the editor build now would (a) race Qoder's in-flight compile and (b) violate standing
  constraints (never run editor build / never clobber concurrent Qoder work). **Cannot proceed.**
- **Unblock condition:** Qoder idle + source mtime settled → clean editor build → DLL newer than
  source → then run `A2b -> B3b -> assign` commandlets + editor QA.

## Verdict
Both remaining items are hard-gated by external factors (active concurrent editing + an editor build
I'm not permitted to force). No constraint-violating action taken. Items stay **pending** with the
precise unblock conditions above; forcing either would be a violation, not completion.
