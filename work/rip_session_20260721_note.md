# Asset Ripping Session — chars/NPCs/vehicles/weapons + base↔skin model (2026-07-21)

> Editor-free umodel extraction wave. Import into UE5 is a SEPARATE wave, GATED
> (see §Import Wave). Nothing under `Source/` or `Content/Imported/` was modified;
> all output is under `Content/Extracted/`. Baseline `Content/Imported` = 4499
> uasset before AND after (verified).

## Domain insight actioned
Retail APB weapons = BASE meshes + textures; most catalog "weapons" are SKINS/reskins
on a base (confirmed on disk: shared `WeaponSkins.upk`). apbdb.com documents the
base↔skin graph. → derived a machine-authoritative map and deduped extraction.

## Base↔skin derivation (apbdb.com beacon API)
- Script: `tools/scripts/build_weapon_base_skin_map.py` (resumable cache `work/apbdb_cache/`).
- API: `https://api.apbdb.com/beacon` (headers Origin/Referer = https://apbdb.com required).
  - `/items?cat=Weapon&limit=200&page=N` (792 total), `/items/:sAPBDB` (detail),
    `/items/:sAPBDB/reskins` (bidirectional family).
- Authoritative mesh key = `detail.eWeaponTypeLink.sAPBDB` (UE3 class name), finer than
  the `/reskins` marketing family (e.g. NTEC splits into 6 real meshes).
- Output: `work/weapon_base_skin_map.json` — **792/792 mapped, 202 mesh families, 590 skins**
  (388 Armas, 170 cosmetic), 0 unlinked.
- Catalog augmented (data-only): `Content/Data/weapons.json` + `weapons_catalog.json` each
  entry now has `base_weapon_id`, `mesh_key`, `is_skin`, `cosmetic`. Backups: `*.prebaseskin.bak`.

## Proven extraction method (retail 564/33)
`tools/UEViewer/umodel_64.exe -path="<retail>\APBGame\Content\Release\Packages" -game=apb -export -out=<dest> <Pkg>`
- Local umodel is a REBUILT FORK with APB 564/33 fixes. No AES, standard LZO.
- Bulk driver: `tools/scripts/extract_batch.ps1 -ListFile -OutRoot -PathRoot -LogFile`.
- Scope lists: `tools/scripts/build_rip_lists.py` → `work/rip_lists/*.txt`.
  Retail Packages = 6620 upk → vehicles 3080, characters 2010, contacts_npc 102,
  weapons_base 96, symbols 23, clothing 4, weapon_skins 2 (anim 81 + other 1222 excluded).

## Extraction results (all under Content/Extracted/)
| Wave | dir | tally | notes |
|---|---|---|---|
| Weapon skins | `WeaponSkins/` | 216 obj, 209 .tga | shared skin pool (Camo/Aggression/flags…) |
| Weapon bases | `WeaponsBase/` | ok 91/96, 235 psk, 10366 tga | 5 fails = benign `WeaponCurves*`/`*_Design` (no mesh) |
| Vehicles | `VehiclesBulk/` | ok 2793/3080, 2532 psk, 6047 tga | 287 empties = benign `AudioSiren*`/`SirenFlashPattern*`/BRDF/Design |
| Characters | `CharactersBulk/` | ok 1872/2010, 1182 psk, 11544 tga | incl. clothing (F_Dress/M_Trousers/LC_*) → supersedes old empty ClothingBulk3 |
| Symbols | `SymbolsBulk/` | 23/23, 1461 tga | SymbolEditor decal library |
| Clothing menus | `ClothingMenus/` | 4/4, 784 tga | APBMenus wardrobe art |
| Morph-safe clothing | `MorphSafeClothing/` | ok 28/30, 28 psk, 211 tga | recovered morph-crash meshes (see below) |

## Golem morph crash — root cause + fix (NEW, important)
- Whole-package export of morph-bearing clothing crashes: `TArray index out of range`
  in `USkeletalMesh3::ConvertMesh` ← `UMorphTargetSet::PostLoad` on the runtime
  `*_ReChunkified` mesh variant. Kills the whole-package export → files=0.
- `-nomorph` does NOT help (crash is in mesh convert, not morph load).
- **FIX (works):** object-targeted export skipping `*_ReChunkified` — enumerate via `-list`,
  export base `SkeletalMesh`/`StaticMesh` + all `Texture2D` by explicit `-obj=` names.
  Tool: `tools/scripts/extract_morph_safe.ps1`. Recovered 28/30.
- 2 residual (`F_Footwear_Shoes_Functional_HighHeels`, `F_Footwear_Trainers_Urban_Plain`):
  their SINGLE SkeletalMesh carries the morph itself → mesh unextractable by umodel
  (the documented Golem limitation, `morph_spike.md`, deferred to M17). Textures WERE
  recovered (6 + 8 tga). These 2 meshes need a hand-authored UE5 morph target at M17.

## Import Wave — BLOCKED (do NOT run now)
- Editor import commandlets (e.g. `import_weapons.py`) require a clean editor build.
- Qoder is actively editing C++ (14 processes at session end); editor DLL stale vs source.
- Running the editor build now would race Qoder + violate standing constraints.
- Unblock: Qoder idle + source mtime settled → clean editor build → run import commandlets
  for WeaponsBase/VehiclesBulk/CharactersBulk/MorphSafeClothing + editor QA.
- The base_skin_map should drive weapon import: import 202 base meshes + WeaponSkins pool
  once; 590 skins become material/catalog mappings, NOT duplicate mesh imports.

## Verification (this session)
- V1 `Content/Imported` = 4499 (unchanged baseline). V2 map 792/792 + catalog tagged 792/792.
- V3 tallies logged in `work/logs/extract_*_full.log` + `extract_morphsafe.log`.
- V4 spot-check: `Baked_A_2DrCoupe` psk=1/tga=16; recovered `M_Facewear_Mask_Clown` psk=1/tga=7.
