# M18b — Character-create promotion batch

Date: 2026-08-02

## Result

24 rows promoted: verified 197 -> 221. Allowlist 161 -> 185 entries (+ 24).
Frontend routing probe extended with character-create assertions — all 12 markers
green, `FRONTEND_RUNTIME_ROUTING_PASS`. `M3R_ASSET_QA_PASS`.

## Batch

- Base meshes (3): `Contact_LaRocha/m_contact_enforcement_larocha`,
  `Contact_Bloodrose/F_Contact_Criminal_Bloodrose`,
  `Contact_Sofia/F_Contact_Enforcement_Sofia` (StaticMesh, retail).
- Wardrobe (20): `Characters/Wardrobe/*` — per-contact and per-LC retail upks
  (e.g. `Contact_Bloodrose_BritneyBloodrose.upk`, `LC_M_Business_City_AsianDark_01.upk`).
- Preview material (1): `MI_DisplyPoint_CharacterMesh` (MaterialInstanceConstant)
  from `MaterialDatabase/GenericDistrict/DisplayPoint_CharacterMesh.upk` +
  extracted `Statue_Norm.tga`.

## Evidence chain

retail upk (sha256) -> staged OBJ / extracted TGA (sha256) -> uasset.
Ownership proven via `model_reference_catalog.json` name hints for the 3 base
meshes; wardrobe uses exact package-name prefix match (disclosed in
conversion.ownership). Source locators `$ {retail_steam}/APBGame/Content/Release/Packages/...`.

## Files

- `tools/scripts/record_character_create_import.py` (+ 8 unit tests)
- `work/evidence/character_create_batch/{base_meshes,wardrobe,material}_exact.json`
- `tools/scripts/test_promote_verified_batch.py` — 2 stale hardcoded-26 tests
  made ledger-dynamic (would fail on every future promotion otherwise).
- `APBSessionProbeSubsystem.cpp` — probe now asserts character-create mesh +
  material allow/load and wrong-source/unlisted rejects.

## Gates

strict verified=221 PASS; cook audit unverified 15,759 -> 15,758 (oracle
`Contact_Bloodrose_F_Contact_Criminal_Bloodrose` wardrobe row leaf-matches);
catalog/parity/static-audit PASS. Editor target builds.

## Remaining

Cook audit still counts 15,758 (district/vehicle/character dir-level refs).
Character-create is runtime-allowed; the 102 catalog dir-level hints are task-19
modeling territory.
