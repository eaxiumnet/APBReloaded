# Morph spike — runtime face morph (D4, M5)

Status: **ABANDONED after 2 attempts** (AGENTS.md rule 5). Fallback adopted.
Date: 2026-07-20

## Goal
Extract retail APB's runtime facial morph (Golem) system so char-create face
sliders drive real vertex morphs, matching retail 1:1.

## Attempt 1 — locate the morph surface
Searched retail reference for morph/golem/facial assets.
Found: `APBGame\Config\APBLCC.ini`, `APB_CharacterTool\GolemMat*.upk`,
`MaterialDatabase\Golemobile*.upk`, `Interface\APBMenus_Character*.upk`.
=> The facial rig is the **Golem** system, shipped as **cooked UE3 packages**.

## Attempt 2 — reachability + project inventory
- Golem/CharacterTool morph data exists only inside cooked `.upk` (UE3 pkg).
- Project `Content\Imported` = 4,499 uassets, **0** match morph/golem/facial —
  morph targets were never successfully extracted (umodel exports static/
  skeletal geometry, not the cooked Golem morph deltas).
- `APBLCC.ini` contains only `[APBGame.GolemSpawnerActor]` (a spawner class
  ref), **no** slider/morph/bone tables to reconstruct from plain data.

## Verdict
Runtime Golem morph extraction is out of reach without bespoke UE3 morph-cook
reverse engineering — unbounded R&D, explicitly the D4 non-goal. Blocked, not
worth a 3rd attempt.

## Fallback in place (D4 "port behavior" contract)
Hand-built parametric morph via the existing Domain + preview path:
- `ApplyBodyProfile(Height, Bulk, SkinTone, FacePreset)` — height/bulk scale
  the preview mesh live; `SkinTone`/`FacePreset` are integer selectors carried
  in Domain `CharacterAppearance` (serialized/persisted).
- Face variety is delivered by discrete FacePreset selection rather than continuous
  vertex morph. Continuous-slider morph is deferred to M17 polish IF a
  hand-authored UE5 morph-target set is later built for the base heads.

## Reopen criteria
Only if a UE5 morph-target set is hand-authored on the imported base heads
(m_contact_*, F_Contact_*), at which point FacePreset maps to morph weights.
