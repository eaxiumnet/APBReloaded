# M16 — Reference Source Infrastructure Plan

**Created:** 2026-07-26
**Status:** ready for execution
**Origin:** 3-round adversarial planning team (minimalist-skeptic, rigor-maximalist,
systems-analyst, lateral-thinker), converged unanimously on rulings C1-C5.

## Scope

Repairs the reference-source contract: a discoverable source registry, the live umodel
capability footgun, ledger provenance, a catalog-provenance gate, discoverability edits,
shadow-generator cleanup, a non-extracting archive inventory, and a missions.json note.

Zero Domain/UE C++ source changes. No UE target build required. `Content/Data/*.json`
runtime shapes are NOT altered. No commit happens unless the user explicitly asks.
Never edit `D:\UE58\UE_5.8`.

## What the original 5-item proposal became

| Original item | Outcome |
|---|---|
| 1. Authorization prose | LARGELY DISSOLVED — null hypothesis held (see C-notes) |
| 2. umodel documentation | UPGRADED to top-tier live defect (T1C) |
| 3. Require `source` field | REPLACED by manifest + failing gate (C5) |
| 4. 6.45 GB archive roadmap item | DOWNGRADED to non-extracting inventory report (T6A) |
| 5. Ledger schema split | SURVIVES with mandatory consumer-first ordering (C4) |
| NEW | Source registry + resolver (T1A/T1B) |
| NEW | Shadow generator deletion (T5A) |
| NEW | ninjaripper BattlEye anti-pattern, ApbPrivateServer entry (T4B) |

## Settled rulings

- **C1 mirror policy** — Steam is *policy*-canonical, declared not fingerprint-selected.
  Preflight validates the Steam root and reader capability. A mirror mismatch WARNS and
  marks the mirror non-authoritative; it must NOT block Steam extraction. Explicit mirror
  selection fails closed until its inventory is recorded. Rationale: bit-identity was
  never established, so a fail-closed exact-match gate would fail on first run.
- **C2 shadow generator** — parallel cleanup, not a blocker, does not outrank the five.
- **C3 missions.json** — dead data, NOT a runtime defect; lowest rank.
- **C4 ledger** — consumer-first ordering is a hard edge: `build_import_status.py` BEFORE
  `import_ledger.json`. Not cut; leaving it preserves a schema lie across 569 entries.
- **C5 catalog provenance** — one failing gate over a manifest keyed by catalog output
  path, with a temporary allowlist the gate forbids growing.

## Verified ground facts

1. `tools/UEViewer/` holds BOTH binaries, and they DIVERGE:
   `umodel.exe` = 1,218,048 bytes @ 2026-07-16 14:13:40;
   `umodel_64.exe` = 1,218,560 bytes @ 2026-07-16 16:10:34.
   `umodel.exe` is a STALE copy predating the patched build by ~2 hours.
2. `tools/fidelity_oracle_manifest.json` and `tools/fidelity_source_precedence.json`
   are both ABSENT — the shadow generator has never produced consumable output.
3. `validate_fidelity_oracle.ps1:42-45,137-149` reads only `asset_key` and `status`.
   `run_verification_gates.ps1:1-85` has NO fidelity-oracle step. `Source/` and `tests/`
   contain no ledger readers. So `build_import_status.py:69-88` is the SOLE consumer.
4. `ASSET_PIPELINE.md:36-37` documents stock umodel build 1590 FAILING on APB;
   lines 49-55 name the 3 required UEViewer patches; line 97 notes the ninjaripper
   BattlEye live-client ban risk. Referenced from ZERO AGENTS.md.
5. `tools/AGENTS.md:24-28` REQUIRES parameterized reported roots;
   `extract_with_umodel.ps1:16-18` and `parse_privateserver.py:14-22` violate it.
6. `import_ledger.json` = 581 entries; 12 carry a build tag, 569 hold a path in `source`.
   `ARCHITECTURE.md:132-135` documents `source` as `"retail|2011"` — false for 98%.
7. `work/IMPORT_STATUS.md` is already stale (generated 2026-07-20, 11 rows).
8. `APBWorldService.cpp:54-63` + `APBMission.cpp:123-129` overlay canonical retail
   titles; `APBCatalog.cpp:71-77` never reads `description`.

## Notepad init (before first edit)

```powershell
$NOTE = Join-Path $env:TEMP ("ulw-m16-" + (Get-Date -Format 'yyyyMMdd-HHmmss') + ".md")
Set-Content -LiteralPath $NOTE -Value "# M16 Notepad`nStarted: $(Get-Date -Format o)"
Write-Host "NOTEPAD: $NOTE"
```

Sections to append (never rewrite): `## Plan`, `## Scenarios`, `## Now`, `## Todo`,
`## Findings`, `## Learnings`.

## Scenario contract (binding)

| # | Scenario | Pass condition (binary) | Real surface |
|---|---|---|---|
| S1 | Registry preflight, happy path | `check_source_registry.ps1` exit 0 + prints `SOURCE_REGISTRY_PASS` | terminal |
| S2 | Registry preflight, missing root (edge) | temporarily point an alias at a bogus path -> exit 1 + `SOURCE_REGISTRY_FAIL` | terminal |
| S3 | umodel binary identity | resolved reader is `umodel_64.exe`, never stale `umodel.exe` | terminal echo of path |
| S4 | umodel capability probe | `-ListOnly` on a known retail package exits 0 and lists objects | terminal |
| S5 | umodel recovery text | failure message leads with the APB-patched-rebuild requirement, not bare "Try updating umodel" | terminal |
| S6 | Ledger migration dry-run (edge) | prints per-bucket counts summing to exactly 581, writes nothing | terminal + unchanged mtime |
| S7 | Consumer-first ordering | `build_import_status.py` renders `source_build` BEFORE migration runs | generated md |
| S8 | No raw paths in report | structural parse of the ledger table: NO cell in the `source_build` column matches `^[A-Za-z]:\\`, and every value is in the closed enum | parser stdout |
| S9 | Catalog provenance gate, happy | `check_catalog_provenance.ps1` exit 0 + `CATALOG_PROVENANCE_PASS` | terminal |
| S10 | Gate rejects new unregistered catalog (edge) | drop a temp catalog -> exit 1 + `CATALOG_PROVENANCE_FAIL`, then delete temp | terminal |
| S11 | Gate rejects allowlist growth (edge) | append a bogus allowlist row -> exit 1, then revert | terminal |
| S12 | Regression: fidelity oracle | `validate_fidelity_oracle.ps1` still prints `FIDELITY_ORACLE_PASS` | terminal |
| S13 | Regression: port contract | `check_port_contract.ps1` still prints `APB_PORT_CONSISTENCY_PASS` | terminal |
| S14 | Regression: domain tests | `tests\build_and_run.ps1` reports 0 failures | terminal |
| S15 | Archive inventory, non-extracting | report lists archive-unique paths; pre/post directory snapshot diff is EMPTY outside the report file itself | report + snapshot diff |

**Evidence class per scenario (Oracle blocker 7).** RED→GREEN is required only for scenarios
that assert NEW behavior — S1-S11 and S15. S12-S14 are pre-existing regression gates: they
are already green, so demanding a RED state is impossible. For those, capture
**before-GREEN and after-GREEN** with the same marker, and record both terminal excerpts.
Faking a RED on a regression gate by deliberately breaking unrelated code is forbidden.

## Wave structure

```
WAVE 1 (parallel, no interdependencies)
  T1A registry data      T2A ledger CONSUMER      T5A delete shadow gen
  T6A archive inventory
WAVE 2 (needs W1)
  T1B resolver     <- T1A
  T1D parse_privateserver params <- T1A
  T2B ledger migration <- T2A            [HARD EDGE: consumer before data]
  T4A root AGENTS.md   <- T1A            [cites registry filename]
  T4B tools/AGENTS.md  <- T1A            [cites registry + 66-file deferral]
WAVE 3 (needs W2)
  T1C umodel wrapper fix <- T1B
  T2C ARCHITECTURE.md schema <- T2B
  T3A provenance manifest bootstrap
  T4C ASSET_PIPELINE link <- T1A,T1B     [cites registry AND resolver usage]
WAVE 4 (needs W3)
  T1E registry gate <- T1C
  T2D regenerate IMPORT_STATUS.md <- T2B,T2C
  T3B provenance gate script <- T3A
WAVE 5
  T7 missions.json dead-data note
```

Dependency justification: T2A before T2B is the C4 hard edge — migrating the ledger
first blanks all 581 source cells on next regeneration. T1E last in its chain because it
exercises registry + resolver + wrapper end to end. T2D is a GENERATED artifact and must
be regenerated by script, never hand-edited.

**Wave corrections (Oracle non-blocking note, accepted).** T4A/T4B/T4C were originally
placed in WAVE 1 but every one of them cites `source_registry.json` by name, and T4C also
documents the resolver's invocation — documenting a file that does not exist yet is a
forward reference a reviewer cannot verify. T4A/T4B moved to WAVE 2 (after T1A), T4C to
WAVE 3 (after T1B). Confirmed there is NO file-edit collision between T4C
(`ASSET_PIPELINE.md:13`) and T1C (`extract_with_umodel.ps1:16`) — different files. T5A
stays in WAVE 1 and is independent of T3A/T3B: the deleted generator's outputs
(`tools/fidelity_oracle_manifest.json`, `tools/fidelity_source_precedence.json`) are absent
from disk and are DIFFERENT files from the new `Content/Data/catalog_provenance_manifest.json`.

## T1A — create `tools/source_registry.json`

Canonical root registry for the MIGRATED CALLERS ONLY (T1C, T1D, T4C). Deliberately NOT a
content-addressed lockfile — the team rejected that as gold-plating for a single
workstation. Hashes appear only where a non-canonical mirror needs approval.

**SCOPE HONESTY (Oracle blocker 10, verified 2026-07-26).** A repo scan found the Steam
root string embedded in **66** files under `tools/`, `tools/scripts/`, `tools/convert/`
(`Program Files (x86)` → 66 files; `steamapps` → 64). Examples beyond the three named
above: `parse_int_tables.py:37`, `decode_level_streaming.py:16`,
`extract_mission_templates.ps1:17`. This plan therefore does NOT claim the registry is the
repo-wide single point of truth. It migrates only the three explicitly named callers, and
T1E gates ONLY those three. `tools/AGENTS.md` (T4B) must say "roots for migrated tools come
from the registry; a full migration of the remaining ~63 root consumers is deferred" — it
must NOT claim all generators use it. A follow-up milestone owns the bulk migration; record
the 66-file inventory in the notepad as the backlog baseline.

```json
{
  "schema_version": 1,
  "roots": {
    "retail_steam": {
      "path": "C:\\Program Files (x86)\\Steam\\steamapps\\common\\APB Reloaded",
      "canonical": true, "authoritative": true,
      "expected_package_version": { "file_version": 564, "licensee": 33 },
      "packages_subpath": "APBGame\\Content\\Release\\Packages",
      "apb_private_server_subpath": "ApbPrivateServer",
      "note": "Policy-canonical retail root (C1)."
    },
    "retail_mirror": {
      "path": "D:\\APBReloaded\\APB Reloaded",
      "canonical": false, "authoritative": false,
      "inventory_approved": false,
      "note": "Repo-root copy, distinct NTFS file IDs, bit-identity NEVER established. Warn-only; fails closed if explicitly selected while inventory_approved is false."
    },
    "ref_2011": {
      "path": "D:\\APBReloaded\\2011 apb\\APB All Points Bulletin\\APB North America",
      "canonical": true, "authoritative": true,
      "expected_package_version": { "file_version": 547, "licensee": 31 }
    },
    "ref_2011_archive": {
      "path": "D:\\APBReloaded\\2011 apb\\APB All Points Bulletin\\Client\\Client1.1.0.534979.7z",
      "canonical": false, "status": "quarantined_candidate",
      "note": "6.45 GB, never extracted. Inventory/overlap report (T6A) must precede any extraction. Never merge into the canonical extracted tree unproven."
    }
  },
  "readers": {
    "umodel": {
      "path": "D:\\APBReloaded\\tools\\UEViewer\\umodel_64.exe",
      "patched_fork": true,
      "supports": ["564/33", "547/31"],
      "required_patches": ["UnPackage3.cpp DependsOffset 4x int32 pad (GAME_APB licensee>=33)", "GameDatabase.cpp ArVer 564 / Licensee 33 detection", "UnObject.cpp ByteProperty EnumName + BoolProperty byte"],
      "note": "Stock build 1590 FAILS on APB (ASSET_PIPELINE.md:36-37). Do NOT substitute a stock rebuild. Sibling umodel.exe is a STALE copy (verified 2026-07-26) and must not be used."
    }
  }
}
```

**Verify:** `pwsh -NoProfile -Command "Get-Content D:\APBReloaded\tools\source_registry.json -Raw | ConvertFrom-Json | Out-Null; 'JSON_OK'"` → prints `JSON_OK`.

## T1B — create `tools/scripts/resolve_source_root.ps1`

`param([Parameter(Mandatory)][string]$Alias, [switch]$Preflight)`.
Reads the registry, resolves the alias, ECHOES the selected absolute root (satisfying
`tools/AGENTS.md:24-28`), and applies C1.

**NO `-AllowMirror` BYPASS (Oracle blocker 1, accepted).** The earlier draft added an
`-AllowMirror` switch, which let a caller select an unapproved mirror and thereby override
C1's fail-closed requirement. That switch is REMOVED. Mirror approval is a property of the
registry DATA, never of the command line:

| Situation | Behavior |
|---|---|
| canonical root missing | FAIL, exit 1 |
| mirror alias selected, its `inventory_approved` is `false` or absent | FAIL closed, exit 1, message names the alias and that approval is unrecorded |
| mirror alias selected, `inventory_approved:true` | resolve normally, exit 0 |
| canonical alias selected, mirror present but divergent | WARN only, exit 0 (C1: warn-only on non-selection) |

There is no command-line flag, environment variable, or parameter that can turn the second
row into a pass. Flipping `inventory_approved` to `true` is a reviewable registry edit.

**Verify:** three commands, all exact.
1. `pwsh -File tools\scripts\resolve_source_root.ps1 -Alias retail_steam -Preflight` → exit 0, echoes the Steam path.
2. `pwsh -File tools\scripts\resolve_source_root.ps1 -Alias retail_mirror -Preflight` → exit 1, message contains `inventory_approved`.
3. Bypass-proof test: `pwsh -File tools\scripts\resolve_source_root.ps1 -Alias retail_mirror -Preflight -AllowMirror` → MUST fail as an unknown-parameter error, NOT resolve the root. Capture the terminal text (S2).

## T1C — fix `tools/scripts/extract_with_umodel.ps1` (TOP-TIER, live defect)

Three defects in one script:

1. **Line 16 points at the STALE binary.** It sets `$Umodel` to
   `...\UEViewer\umodel.exe`, but that file is 1,218,048 bytes @ 14:13:40 while the
   patched `umodel_64.exe` is 1,218,560 bytes @ 16:10:34 — a copy made BEFORE the patched
   build. Repoint at `umodel_64.exe` via the T1A registry.
2. **Lines 20-39 check only existence, never capability.** Add a probe that actually
   opens a known retail package and fails with the patch requirement if it cannot.
3. **Line 39 recovery text misleads.** It reads
   `"Try updating umodel or building UEViewer from source with APB fixes"` — the
   unqualified "Try updating umodel" LEADS, and following it installs a stock build that
   silently loses APB support. Reorder so the APB-patched-rebuild requirement comes first.
   (The team's stronger "recommends an arbitrary build" phrasing was conceded as overstated;
   this is the defensible version.)

Also replace the hardcoded root at lines 16-18 with a `resolve_source_root.ps1` call.

**Verify (S3/S4/S5) — exact, no worker decisions (Oracle blocker 11).**

**Probe package — CORRECTED against the real retail tree (re-review #1 blocker 11).** The
earlier draft named `<retail_steam>\APBGame\CookedPC\Characters.upk` with a fallback globbing
the same directory. Verified on disk 2026-07-26: **`APBGame\CookedPC` DOES NOT EXIST** in the
retail install, so both the probe and its only fallback were unresolvable and S3/S4/S5 could
never have run. The real package tree is the registry's `packages_subpath`,
`APBGame\Content\Release\Packages`, holding **6,620** `.upk` files.

Probe resolution, deterministic, in this order:

1. Root: `resolve_source_root.ps1 -Alias retail_steam`; package root = that root joined with
   the registry's `packages_subpath` value (never a hardcoded subpath).
2. Preferred probe (verified present): `Character\Contact\Contact_LaRocha.upk`.
3. If absent, the ONLY permitted substitute is the deterministic recursive first:
   `Get-ChildItem -LiteralPath <packageRoot> -Filter *.upk -Recurse -File | Sort-Object FullName | Select-Object -First 1`
   (verified to resolve `Anim\Contact\Ambient\Male\Anim_Contact_Ambient_Male.upk`). Note the
   sort is on `FullName`, not `Name` — sorting by bare name is non-deterministic across
   directories holding same-named packages.
4. The chosen package MUST be echoed by the wrapper and recorded in the notepad.
5. `-Package` takes the path RELATIVE TO the package root (the form the wrapper joins onto
   the resolved root); confirm the wrapper's existing argument convention before use and do
   not pass a bare filename if the wrapper expects a relative path.

1. S3: `pwsh -File tools\scripts\extract_with_umodel.ps1 -ListOnly -Package "Character\Contact\Contact_LaRocha.upk"`
   → exit 0; stdout contains `umodel_64.exe`; stdout does NOT contain `\umodel.exe`.
2. S4: same command → stdout object-list marker is a non-empty line matching
   `^\s*\d+\s+\S+` (umodel's `-list` object rows); assert match count ≥ 1.
3. S5 mutation: `Rename-Item tools\UEViewer\umodel_64.exe umodel_64.exe.bak` →
   rerun command 1 → exit 1; stdout FIRST non-empty error line matches
   `APB-patched|rebuild|patched fork` and does NOT match `^Try updating umodel`.
4. S5 restore (MANDATORY teardown): `Rename-Item tools\UEViewer\umodel_64.exe.bak umodel_64.exe`
   then rerun command 1 → exit 0. Capture the restore receipt; a left-renamed reader is a
   broken worktree.

## T1D — parameterize `tools/convert/parse_privateserver.py`

Lines 14-22 embed the Steam path and workstation project root. Add
`--steam-root` / `--project-root` args defaulting from `tools/source_registry.json`, and
print the resolved roots. Preserve the existing honesty markers at lines 83-87 verbatim.
`APBPrivateServerOpcodes.h` is GENERATED — regenerate, never hand-edit.

**MUST NOT overwrite `Source/` during verification (Oracle blocker 8, accepted).** A normal
run of this script writes `Source/APBReloaded/Domain/APBPrivateServerOpcodes.h` directly
(`parse_privateserver.py:176`), and that header is `#include`d by
`APBGameInstanceSubsystem.cpp`. Verifying by "just running it" therefore CAN mutate C++ and
silently invalidate this plan's "zero Source impact, no UE build needed" claim. So:

- Add an `--out` argument (defaulting to the current in-tree header path, preserving
  existing behavior for real regeneration).
- Verification runs to a TEMP path only and byte-compares against the checked-in header.

**Verify — exact:**
1. `python tools\convert\parse_privateserver.py --help` → exit 0, stdout lists
   `--steam-root`, `--project-root`, `--out`.
2. `python tools\convert\parse_privateserver.py --out "$env:TEMP\opcodes_probe.h"` → exit 0,
   stdout prints both resolved roots.
3. `(Get-FileHash "$env:TEMP\opcodes_probe.h").Hash -eq (Get-FileHash Source\APBReloaded\Domain\APBPrivateServerOpcodes.h).Hash`
   → `True`. **If this prints `False`, the generator's output has drifted from the committed
   header: STOP, do not overwrite, and escalate — resolving the drift requires an
   `APBReloadedEditor` build and is OUT OF SCOPE for this milestone.**
4. Teardown: `Remove-Item "$env:TEMP\opcodes_probe.h"`.
5. Targeted header assertion:
   `git diff --quiet -- Source\APBReloaded\Domain\APBPrivateServerOpcodes.h` → exit 0
   (verified 2026-07-26: this header is currently CLEAN, so the assertion is not vacuous).
   The repo-wide no-C++-touch proof is the BASELINE-DELTA procedure in "End-of-work gates" —
   NOT an "empty `git status`" check, which is unsatisfiable because `Source/` already carries
   102 pre-existing unrelated worktree entries.

## T1E — new gate `tools/check_source_registry.ps1`

Follows the `check_port_contract.ps1` convention: `SOURCE_REGISTRY_PASS` /
`SOURCE_REGISTRY_FAIL`, non-zero exit on failure. Asserts registry parses; every
`canonical:true` root exists; the umodel reader path exists AND resolves to
`umodel_64.exe`; the reader passes the capability probe.

**Migrated-caller assertion is a CLOSED LIST of exactly three (per T1A scope honesty).**
The gate asserts these three files contain NO literal Steam/reference root and DO reference
the registry or resolver:

```
tools/ASSET_PIPELINE.md
tools/convert/parse_privateserver.py
tools/scripts/extract_with_umodel.ps1
```

It MUST NOT scan the other ~63 root consumers — they are knowingly un-migrated and gating
them would make the gate red on arrival. The gate hardcodes this three-entry list and fails
if the list is silently shortened.

**Verify — exact (S1/S2):**
1. `pwsh -File tools\check_source_registry.ps1` → exit 0, stdout contains `SOURCE_REGISTRY_PASS`.
2. Mutation: `Copy-Item tools\source_registry.json "$env:TEMP\reg.bak"`, then edit the
   `retail_steam` root value to `D:\__nonexistent_probe__`, rerun → exit 1, stdout contains
   `SOURCE_REGISTRY_FAIL` and the bogus path.
3. Teardown: `Copy-Item "$env:TEMP\reg.bak" tools\source_registry.json -Force; Remove-Item "$env:TEMP\reg.bak"`,
   rerun → `SOURCE_REGISTRY_PASS`. Capture the restore receipt.

## T2A — update `tools/scripts/build_import_status.py` (CONSUMER FIRST)

**This must land BEFORE T2B. It is the C4 hard edge.** Line 87 renders
`e.get('source')` straight into the Markdown table and lines 69-88 build the rows; if the
ledger migrates first, all 581 source cells render blank/None.

Change: emit a `source_build` column AND a `source_locator` column.

**The fallback must CLASSIFY, not passthrough (Oracle blocker 3, accepted).** The earlier
draft said "fall back to `source`", which would render absolute Windows paths into a column
literally named `source_build` — reproducing the exact schema lie C4 exists to kill, for the
duration of the consumer-first commit. Corrected rule:

- Extract the derivation logic into ONE shared module, `tools/scripts/ledger_source.py`,
  exposing **`classify(raw, asset_key) -> (source_build, source_locator)`** implementing the
  T2B table.

  **`asset_key` is a REQUIRED parameter (re-review #1 blocker 3).** An earlier draft declared
  `classify(raw)`, which cannot implement the T2B table at all: two of its rows key off
  `Content\Extracted\` paths whose origin build is recoverable only from `asset_key`. A
  single-argument signature would have forced those rows to collapse to `unknown` and quietly
  lost real provenance. Both callers (T2A report, T2B migration) MUST pass the entry's
  `asset_key`. The function is pure and total: it never raises, and returns `unknown` only
  when the table's last two rows apply.
- T2A imports it; T2B imports the SAME function. One implementation, two callers — the
  migration can never disagree with the report.
- **Per-entry resolution order in T2A (re-review #2 residual 3).** An earlier draft said T2A
  simply "calls `classify`", which breaks AFTER the migration: migrated entries no longer HAVE
  a legacy `source` key, so there is nothing to classify and a blind call would read `None`.
  Exact rule, applied per entry:

  ```python
  if "source_build" in entry:                 # post-migration
      build   = entry["source_build"]         # validate against the 5-member enum; fail loudly if not
      locator = entry.get("source_locator")   # may be absent for the 12 originally-tagged rows
  else:                                       # pre-migration
      build, locator = classify(entry["source"], entry["asset_key"])
  ```

  This is what makes "renders identically before and after" true rather than aspirational: the
  post-migration branch READS the stored enum, the pre-migration branch DERIVES the same enum
  from the same table. Neither branch can emit a path in the build column.
- Rendering: `source_build` column shows the resolved enum member (never a path).
  `source_locator` column shows the raw value, truncated to its last 2 path segments for
  readability. Un-migrated and migrated ledgers render identically.

T2B therefore depends on `ledger_source.py` existing, which T2A creates — reinforcing the C4
edge rather than weakening it.

**Verify (S7) — exact:**
1. `python tools\scripts\build_import_status.py` against the *un-migrated* ledger → exit 0.
2. Header assertion: `Select-String work\IMPORT_STATUS.md -Pattern 'source_build'` → ≥1 hit.
3. Classification assertion (this is the blocker-3 proof): NO value in the rendered
   `source_build` column matches `^[A-Za-z]:\\`, verified by the S8 structural parser below,
   run BEFORE the migration as well as after.

## T2B — migrate `tools/import_ledger.json`

Add a migration script `tools/scripts/migrate_ledger_source.py` with `--dry-run` and
`--apply`. Introduce `source_build` (closed enum) and preserve the raw string as
`source_locator`. Derivation rules, applied in order:

| Current `source` value | `source_build` | `source_locator` |
|---|---|---|
| already `retail` / `2011` / `2011+retail` (12 entries) | unchanged | omitted |
| begins `C:\Program Files (x86)\Steam\...\APB Reloaded` | `retail` | original path |
| begins `D:\APBReloaded\APB Reloaded` (mirror) | `retail` | original path |
| begins `D:\APBReloaded\2011 apb\` | `2011` | original path |
| begins `http://apbdb.com` or `https://apbdb.com` | `apbdb` | original URL |
| begins `D:\APBReloaded\Content\Extracted\` AND `asset_key` identifies the origin build | derived from `asset_key` | original path |
| begins `D:\APBReloaded\Content\Extracted\` and origin NOT recoverable | `unknown` | original path |
| anything else | `unknown` | original value |

`unknown` is an HONEST sentinel, deliberately not a guess, and must be documented in
ARCHITECTURE.md (T2C).

**Exact schema key and closed enum (Oracle blocker 2, accepted — both errors confirmed on
disk 2026-07-26).** The earlier draft said "bump `schema_version` to 2" and gave the enum as
`retail|2011|apbdb|unknown`. Two factual defects:

1. The ledger's top-level keys are `version`, `updated`, `statuses`, `note`, `entries`.
   There is NO `schema_version` key. The migration therefore sets the EXISTING key
   **`version: 2`** (in-place value change). It MUST NOT add a second parallel
   `schema_version` key — two version keys is a worse contract than one wrong one.
2. The ledger genuinely contains `2011+retail` (1 entry, alongside 8 `retail` and 3 `2011`),
   and the derivation table's first row preserves it — so an enum omitting it made the
   migration's own output invalid against its own schema.

**Closed enum (final):** `retail | 2011 | 2011+retail | apbdb | unknown`. This exact
five-member list is what `ledger_source.py` validates, what T2C documents, and what the S8
parser asserts against.

**Verify (S6) — exact:**
1. `python tools\scripts\migrate_ledger_source.py --dry-run` → exit 0; per-bucket counts
   printed; the counts SUM TO EXACTLY 581; `(Get-Item tools\import_ledger.json).LastWriteTime`
   unchanged across the run.
2. `python tools\scripts\migrate_ledger_source.py --apply` → exit 0.
3. Post-assert: `version` == 2; every entry has `source_build` in the five-member enum; no
   entry retains a legacy `source` key; entry count still exactly 581.
4. Re-run `--dry-run` → reports 0 remaining un-migrated entries (idempotence proof).
5. Record all bucket counts, including `unknown`, in the notepad.

## T2C — fix `work/ARCHITECTURE.md` lines 132-135

Replace the false `"source": "retail|2011"` schema line with the v2 shape:
`source_build` with the full five-member closed enum
`retail | 2011 | 2011+retail | apbdb | unknown` (matching T2B exactly — including
`2011+retail`, which the earlier draft dropped), plus optional `source_locator`. State
explicitly what the `unknown` sentinel means and why it is preferred over a guess. Also
document that the ledger's top-level key is `version` (now `2`), not `schema_version`.

**Verify — exact:**
1. `Select-String work\ARCHITECTURE.md -Pattern '"source": "retail\|2011"'` → NO hits.
2. `Select-String work\ARCHITECTURE.md -Pattern 'source_build','source_locator','2011\+retail','unknown'` → all four present.
3. `Select-String work\ARCHITECTURE.md -Pattern 'schema_version'` → NO hits (the key does not exist).

## T2D — regenerate `work/IMPORT_STATUS.md` (GENERATED — do not hand-edit)

```powershell
python D:\APBReloaded\tools\scripts\build_import_status.py
```

**Verify (S8) — STRUCTURAL parse, not a line-anchored grep (Oracle blocker 4, accepted).**
The earlier draft used `Select-String -Pattern '^\|\s*[CD]:\\'`, which only ever inspects the
FIRST Markdown cell — and the first cell is `asset_key`. A raw absolute path sitting in the
`source_build` or `source_locator` cell would sail straight through. Replaced with a real
column-aware parser, `tools/scripts/check_import_status_table.py`:

1. Locate the ledger table, read its header row, and resolve the ZERO-BASED INDEX of the
   `source_build` column by name (fail loudly if that header is absent — a renamed column
   must not silently disable the check).
2. Split every body row on `|`, strip cells, and assert for the `source_build` index:
   - NO cell matches `^[A-Za-z]:\\` (any drive letter, not just C/D), and
   - EVERY cell is a member of the five-member closed enum.
3. Assert the exact row count: the parsed body-row count for the ledger table MUST equal
   **581**, replacing the earlier vague "row count reflects all 581 entries" (the current
   file is stale at 11 rows).
4. Emit a single pass/fail marker and a non-zero exit on failure.

**Verify — exact:**
1. `python tools\scripts\build_import_status.py` → exit 0.
2. `python tools\scripts\check_import_status_table.py` → exit 0, asserts 581 rows + clean enum.
3. Negative control (proves the parser actually bites): hand-insert one temp row whose
   `source_build` cell is `C:\Program Files (x86)\Steam\x` → rerun → exit 1 naming that row.
   Then `python tools\scripts\build_import_status.py` to regenerate clean, rerun → exit 0.
   This negative control is run BEFORE the T2B migration as well, satisfying S7 step 3.

## T3A — bootstrap `Content/Data/catalog_provenance_manifest.json`

Keyed by catalog OUTPUT PATH, with records of
`{source_build, source_locator, generator, source_hash}`. Deliberately does NOT force the
65 heterogeneous runtime catalogs to share one `source` shape — the shapes (apbdb URLs,
INT locators, `source_install`, `source_package(s)`, build enums) are intentional.

**Exact field semantics (Oracle blocker 5, accepted).** The earlier draft named the four
fields but defined none of them, so a worker would have had to invent the schema. Binding
definitions:

| Field | Definition | Validation |
|---|---|---|
| `source_build` | which reference build the data came from | member of the SAME five-member enum as T2B (`retail\|2011\|2011+retail\|apbdb\|unknown`) |
| `source_locator` | the specific input: absolute path, package name, or apbdb URL | non-empty string; absolute paths stored with the reference ROOT REPLACED by its registry alias in `${alias}` form, so the manifest is workstation-independent |
| `generator` | repo-relative path of the script that produces this catalog | file MUST exist on disk |
| `source_hash` | SHA-256 of the CATALOG OUTPUT FILE itself (`Content/Data/<name>.json`), NOT of the reference input | lowercase 64-char hex, `^[0-9a-f]{64}$` |

`source_hash` hashes the OUTPUT deliberately: reference inputs include multi-gigabyte
packages and remote URLs that cannot be cheaply or reproducibly hashed, while the output is
small, local, and in-repo. It is a drift tripwire (catalog changed without provenance being
updated), NOT an input-integrity proof — the plan says so plainly rather than implying
stronger provenance than it delivers.

Path normalization: repo-relative, forward slashes, case as on disk. Keys MUST be unique.

Bootstrap algorithm (deterministic, in this order):
1. Enumerate `Content/Data/*.json` at top level only — currently exactly **65** files.
2. For each of the seven provenance-emitting generators below, read the provenance block it
   already writes and emit a `registrations` record.
3. Every remaining catalog goes to `allowlist`.
4. Assert `count(registrations) + count(allowlist) == 65` with NO key appearing in both.

Bootstrap from generators that already emit provenance: `extract_contact_levels.ps1:46-56`,
`extract_player_roles.ps1:56-63`, `extract_mission_templates.ps1:45-51`,
`extract_task_objectives.ps1:55-64`, `extract_subtitles.ps1:42-47`,
`parse_int_tables.py:86-98`, `sync_apbdb.py:283-295`.

The catalogs whose provenance cannot be resolved from an existing generator go on an
`allowlist` array, each with `owner`, `reason`, `added` date. The allowlist is explicitly
TEMPORARY and the gate forbids it growing.

**Verify — exact counts, not approximations (Oracle blocker 6, second half).** The earlier
draft asserted a "~20/~45 split", which is unverifiable by construction. Confirmed on disk
2026-07-26: **65** top-level `Content/Data/*.json` files BEFORE this task adds the manifest
(so 66 files exist afterwards, of which the manifest itself is excluded from enumeration).
1. Manifest parses as JSON.
2. `count(registrations) + count(allowlist) == 65` exactly; assert no key in both arrays.
3. Record the ACTUAL split produced by the bootstrap in the notepad as the baseline. Do not
   pre-commit to a predicted split — whatever the seven generators yield is the number, and
   that measured value becomes the immutable baseline consumed by T3B.
4. Every `source_hash` matches `^[0-9a-f]{64}$`; every `generator` path exists on disk.

## T3B — new gate `tools/check_catalog_provenance.ps1`

Emits `CATALOG_PROVENANCE_PASS` / `CATALOG_PROVENANCE_FAIL`, non-zero exit on failure.
Enumerates `Content/Data/*.json` (top level only, excluding the manifest itself and
`Content/Data/fidelity/`). Standalone, matching `check_port_contract.ps1`; wiring it into
`run_verification_gates.ps1` is deferred to a later pass.

**Full-field validation, not membership-only (Oracle blocker 5, second half, accepted).** The
earlier draft checked only "is the catalog present in one of the two arrays", so an empty or
malformed registration `{}` would have passed and the gate would have certified nothing. The
gate FAILS on ANY of:

1. A catalog on disk is in neither `registrations` nor `allowlist` (coverage gap).
2. A key appears in BOTH arrays (double-counting).
3. A manifest key names a file that no longer exists on disk (stale registration).
4. `count(registrations) + count(allowlist) != ` the on-disk catalog count (exact coverage).
5. Any registration is missing `source_build`, `source_locator`, `generator`, or `source_hash`.
6. `source_build` is outside the five-member closed enum.
7. `source_locator` is empty, or contains a literal drive-letter path instead of `${alias}` form.
8. `source_hash` fails `^[0-9a-f]{64}$` (SYNTAX).
9. `generator` names a path that does not exist on disk.
10. Duplicate keys within either array.
11. An allowlist row lacks `owner`, `reason`, or `added`.
12. The allowlist count exceeds the immutable baseline (see below).
13. **`source_hash` does not EQUAL the current SHA-256 of its keyed output file** (VALUE).

**Condition 13 is what makes the drift tripwire actually trip (re-review #1 blocker 5).** The
earlier draft validated only condition 8 — the hash's *shape*. A 64-hex string bearing no
relation to the file would have passed, so the "drift tripwire" advertised in T3A tripped on
nothing and the gate certified a format, not a fact. Condition 13 compares
`(Get-FileHash -Algorithm SHA256 <keyed output>).Hash.ToLower()` against the recorded
`source_hash` and FAILS on inequality, naming both values. This is the ONLY condition that
gives `source_hash` any meaning; without it the field is decoration.

Consequence to state plainly in the gate's own help text: editing a catalog now REQUIRES
updating its `source_hash` in the same commit. That is the intended coupling — it forces
provenance to be revisited whenever the data changes — but it also means the gate will fail
on any catalog regeneration that skips the manifest update, which is a feature, not a bug.

**Immutable baseline (Oracle blocker 6, accepted).** The "allowlist must not grow" guarantee
was ineffective as drafted: the baseline count lived inside the same manifest a worker is
editing, so adding a row and incrementing the baseline in the same commit would keep the gate
green — the check would guard nothing. Fix: the expected allowlist count is a CONSTANT
hardcoded in `check_catalog_provenance.ps1` itself (the gate script, not the data it
validates), set from T3A's measured bootstrap value. Raising it requires editing the gate,
which is a visible, reviewable, intentional act. The gate fails if the manifest's own
self-reported count disagrees with the script constant, catching tampering from either side.

**Teardown uses a FILE BACKUP, never `git checkout` (re-review #1, new defect 1).** The
earlier draft restored the manifest with
`git checkout -- Content/Data/catalog_provenance_manifest.json`. That CANNOT work: T3A creates
this file, so during T3B it is still UNTRACKED until commit 10 lands — `git checkout` on an
untracked path fails with `error: pathspec ... did not match any file(s) known to git`, and the
worker would be left with a mutated manifest and a red gate. Correct procedure, used for every
mutation below:

```powershell
# once, before the first mutation
Copy-Item Content\Data\catalog_provenance_manifest.json "$env:TEMP\m16_manifest.bak"
# after each mutation
Copy-Item "$env:TEMP\m16_manifest.bak" Content\Data\catalog_provenance_manifest.json -Force
```

**Verify (S9/S10/S11) — exact, with mandatory teardown:**
1. S9: `pwsh -File tools\check_catalog_provenance.ps1` → exit 0, stdout `CATALOG_PROVENANCE_PASS`.
2. S10: `'{}' | Set-Content Content\Data\__gate_probe.json` → rerun → exit 1,
   `CATALOG_PROVENANCE_FAIL`, stdout names `__gate_probe.json`. Teardown:
   `Remove-Item Content\Data\__gate_probe.json` → rerun → PASS.
3. S11a (growth): append a bogus allowlist row with valid `owner`/`reason`/`added` → rerun →
   exit 1 citing the baseline. Teardown: restore from `$env:TEMP\m16_manifest.bak` → rerun → PASS.
4. S11b (tamper-proof, proves blocker 6 is closed): append the bogus row AND increment the
   manifest's self-reported allowlist count → rerun → MUST STILL exit 1, because the script
   constant disagrees. Teardown: restore from backup → rerun → PASS.
5. Malformed-field control: blank one registration's `source_hash` → rerun → exit 1 citing
   field validation (condition 5). Teardown: restore the manifest from
   `$env:TEMP\m16_manifest.bak` → rerun → PASS.
6. **Condition-13 negative control (proves the drift tripwire bites).** Pick any registered
   catalog, append a single space to it, and rerun WITHOUT touching the manifest → MUST exit 1
   citing condition 13 and printing both the recorded and computed hashes.
7. Final teardown: `Remove-Item "$env:TEMP\m16_manifest.bak","$env:TEMP\m16_catalog.bak"`.
8. Every teardown above is its own tracked todo; a leftover `__gate_probe.json` or dirty
   manifest is NOT done.

**NEVER `git checkout` a file you did not first back up (re-review #2, new defect 1).** Two
problems with checkout as a teardown, both now closed:

1. An earlier draft left a bare `git checkout` with no path in the malformed-field step —
   pathless checkout is ambiguous at best and destructive at worst.
2. Even the *tracked* catalog in step 6 may already carry a PRE-EXISTING dirty edit from the
   102-entry in-flight worktree. `git checkout -- <catalog>` would silently destroy that
   someone-else's work, violating root contract rule 3.

Binding rule for every mutation in T3B: back up to a temp file FIRST, restore from that temp
file, and never use `git checkout` as a teardown at all.

```powershell
# before mutating the manifest (once)
Copy-Item Content\Data\catalog_provenance_manifest.json "$env:TEMP\m16_manifest.bak"
# before mutating the chosen catalog in step 6 (once)
Copy-Item Content\Data\<chosen>.json "$env:TEMP\m16_catalog.bak"
# after each mutation
Copy-Item "$env:TEMP\m16_manifest.bak" Content\Data\catalog_provenance_manifest.json -Force
Copy-Item "$env:TEMP\m16_catalog.bak"  Content\Data\<chosen>.json -Force
```

Restore receipt for step 6: the catalog's SHA-256 after restore equals its SHA-256 captured
before mutation. A hash match is the proof, not the absence of a `git status` line — the file
may legitimately have been dirty both before and after.

## T4A — root `AGENTS.md` discoverability edits

Three surgical changes. Explicitly NOT a broad authorization rewrite — the null hypothesis
held (zero instances across `work/_active.md`'s 1356 lines of an agent ever blocked on
reference access), and lines 67-69 already permit directed reads by negative implication.

1. **Decompose the double-duty sentence at lines 67-69** so build-output context-budget
   protection (`Binaries/`, `Intermediate/`, `Saved/`, `DerivedDataCache/`) is stated
   separately from reference-tree policy (`2011 apb/`, `APB Reloaded/`, `Content/Extracted/`).
   Keep it TIGHT — no 12-line three-section rewrite, and do not invent a "derived extracts"
   governance category that has no current problem.
2. **Add ONE canonical-copy sentence:** the Steam install is the canonical retail source;
   `D:\APBReloaded\APB Reloaded` is a non-authoritative mirror not bit-verified against it.
3. **Add a reference/extraction row to the "Where To Look" table (lines 71-84)** pointing at
   `tools/ASSET_PIPELINE.md` and `tools/source_registry.json` — this is the actual fix for
   the discoverability gap.

Do NOT add undefined verb grants ("read/unpack/reverse-engineer"): those three verbs are
materially distinct and a blanket phrase creates fresh ambiguity in place of the gap.

**Verify — mechanical part (exact):**
1. `Select-String AGENTS.md -Pattern 'ASSET_PIPELINE'` → ≥1 hit (it was in ZERO AGENTS.md before).
2. `Select-String AGENTS.md -Pattern 'source_registry'` → ≥1 hit.
3. `Select-String AGENTS.md -Pattern 'canonical'` → ≥1 hit in the retail-copy sentence.
4. `Select-String AGENTS.md -Pattern 'derived extracts'` → NO hits (rejected category stayed rejected).
5. `Select-String AGENTS.md -Pattern 'reverse-engineer'` → NO hits (rejected undefined verb grant).
6. Net line delta of the 67-69 decomposition is ≤ +6 lines (`git diff --stat AGENTS.md`),
   enforcing "TIGHT" rather than the rejected 12-line rewrite.

**Human acceptance checklist (Oracle blocker 11 / non-blocking note — grep presence does NOT
establish useful prose).** T4A's entire value is wording quality, so it is NOT done until a
human answers YES to all five, recorded in the notepad:

- [ ] Does the decomposed 67-69 text still forbid broad crawling of `Binaries/`,
      `Intermediate/`, `Saved/`, `DerivedDataCache/` as strongly as before?
- [ ] Does it now make clear that a DIRECTED read of a reference tree for a specific task is
      expected and normal, without granting a blanket crawl?
- [ ] Does the canonical-copy sentence make it unambiguous which retail path to prefer, and
      state that the mirror is NOT bit-verified?
- [ ] Can a new agent reach `ASSET_PIPELINE.md` from the root file in ONE hop via the
      "Where To Look" table?
- [ ] Is any sentence open to a reading that contradicts `tools/AGENTS.md` after T4B?

If any answer is NO, revise and re-ask. This is the single largest residual risk in the plan.

## T4B — `tools/AGENTS.md` additions

- **ninjaripper anti-pattern:** `ASSET_PIPELINE.md:97` records a BattlEye ban risk against
  the live client, invisible to any agent following only the hierarchy. Add it as an
  explicit anti-pattern.
- **ApbPrivateServer entry:** document it as a protocol reference with its stub-opcode
  limits. The existing line-34 anti-pattern covers `apb_sdk_ref` BINARY OFFSETS and does
  NOT cover C#-source parsing at a different path.
- **umodel patched-fork contract:** `umodel_64.exe` is the only working APB reader; a stock
  rebuild silently loses support; the sibling `umodel.exe` is stale.
- **Registry pointer, HONESTLY SCOPED (per T1A scope honesty).** Write: roots for the
  MIGRATED tools (`extract_with_umodel.ps1`, `parse_privateserver.py`, `ASSET_PIPELINE.md`)
  come from `tools/source_registry.json`; a full migration of the remaining ~63 root
  consumers is DEFERRED, so most generators still embed the Steam path and lines 24-28 are
  not yet satisfied repo-wide. It MUST NOT claim all tooling reads the registry — that would
  be a fresh documentation lie of exactly the kind T2C exists to remove.

**Verify — exact:**
1. `Select-String tools\AGENTS.md -Pattern 'ninjaripper','ApbPrivateServer','umodel_64','source_registry'` → all four present.
2. `Select-String tools\AGENTS.md -Pattern 'deferred|not yet satisfied'` → ≥1 hit, proving the
   deferral is stated rather than glossed.

## T4C — link `tools/ASSET_PIPELINE.md` to the registry

Replace the hardcoded Steam path at line 13 with a pointer to `tools/source_registry.json`
plus `resolve_source_root.ps1`, keeping the patched-fork detail at lines 49-55 intact.

**Verify:** grep shows the registry referenced and no bare hardcoded Steam path remains
as the authoritative source.

## T5A — delete `tools/scripts/build_fidelity_manifests.py`

**On-disk check performed 2026-07-26: `tools/fidelity_oracle_manifest.json` and
`tools/fidelity_source_precedence.json` are both ABSENT.**

Justification for DELETE over repair: the generator writes to the wrong directory
(`OUT = ROOT/"tools"`, lines 14-15) while `validate_fidelity_oracle.ps1:1-4` validates only
`Content/Data/fidelity/`; and more fundamentally the schemas are INCOMPATIBLE — the
validator requires `status`/`staged_path`/`comparison_type`/`ledger_key`/`ledger_status`
(lines 12-16) while the generator emits
`source_path`/`source_sha256`/`destination_sha256`/`comparison` (lines 45-57), with zero
overlap on discriminating keys. It has therefore NEVER produced validator-consumable
output, and the absent output files confirm it. The hand-maintained
`Content/Data/fidelity/` manifests are authoritative and current. Repair would mean a full
schema rewrite plus a path fix for no benefit, while leaving it in place invites a future
agent to run it and trust dead output.

Additionally note that lines 39-53 hash only repo-staged files and never the external
install input — so it could not have satisfied source-fingerprinting either.

**Verify (S12):** file absent via `Test-Path` → `False`; then
`pwsh -File tools\validate_fidelity_oracle.ps1` still prints `FIDELITY_ORACLE_PASS`,
proving the deletion touched nothing the gate depends on.

## T6A — non-extracting 2011 archive inventory

Create `tools/scripts/inventory_2011_archive.ps1`. LIST ONLY — no extraction. The archive
stays a QUARANTINED CANDIDATE and enters the roadmap ONLY if unique or divergent content is
proven. Never merge into the canonical extracted tree unproven.

**Exact executable, source, and rules (Oracle blocker 9, accepted).** The earlier draft said
"`7z l`" and "a recommendation", neither of which is executable without invention:

- **Aliases, corrected (re-review #1 blocker 1).** An earlier draft used
  `-Alias apb_2011`, which is NOT a registry alias — T1A defines `ref_2011` and
  `ref_2011_archive`. A worker would have hit an unknown-alias failure on the first command.
  Correct usage:
  - Archive: `resolve_source_root.ps1 -Alias ref_2011_archive` — this alias resolves the
    ARCHIVE FILE directly (`...\Client\Client1.1.0.534979.7z`), so do NOT join a subpath onto it.
  - Comparison tree: `resolve_source_root.ps1 -Alias ref_2011` (the extracted
    `APB North America` root).
  - `ref_2011_archive` is `canonical:false, status:"quarantined_candidate"`, so the resolver
    must permit resolving a quarantined FILE alias for listing purposes while still refusing
    any extraction — listing is not selection of a mirror root and C1's fail-closed mirror
    rule does not apply to it.
- Executable resolution, in this order, first hit wins, FAIL if none:
  `tools\7z\7za.exe` → `C:\Program Files\7-Zip\7z.exe` → `(Get-Command 7z).Source`.
  Echo the resolved executable into the report.
- List command, exact: `& $sevenZip l -ba -slt -- $archive` (`-slt` for parseable records,
  `-ba` to drop the banner). Parse `Path = ` / `Size = ` / `Attributes = ` records; skip
  directory records (`Attributes` contains `D`).
- Comparison target: the extracted sibling tree resolved from the registry, enumerated with
  `Get-ChildItem -Recurse -File`.
- Path normalization (binding): backslashes → forward slashes, strip any leading `./`, strip
  the common root prefix from both sides, and compare **case-insensitively** (NTFS semantics).
  Record the normalization rule in the report so the diff is reproducible.
- Report schema `work/m16_2011_archive_inventory.md`, fixed sections: `Resolved inputs`
  (executable, archive path, compare root), `Totals` (entry count, uncompressed byte total),
  `Archive-unique paths`, `Common paths (count only)`, `Verdict`.
- `Verdict` is CONSTRAINED to one of exactly three strings, chosen by rule — not prose
  judgement: `NO-UNIQUE-CONTENT / KEEP QUARANTINED` when the unique-path count is 0;
  `UNIQUE-CONTENT PRESENT / ROADMAP CANDIDATE` when > 0; `INCONCLUSIVE / TOOL OR PATH FAILURE`
  when listing or resolution failed.

**Verify (S15) — proves non-extraction properly.** `LastWriteTime` on the archive is NOT
sufficient evidence: normal extraction leaves the source archive's mtime untouched, so the
earlier check proved nothing. Replaced with a filesystem-snapshot diff:
1. Pre-snapshot: `Get-ChildItem -LiteralPath <2011 root> -Recurse -File | Select-Object FullName,Length,LastWriteTime | Export-Csv $env:TEMP\pre.csv`.
2. Run `pwsh -File tools\scripts\inventory_2011_archive.ps1` → exit 0.
3. Post-snapshot to `$env:TEMP\post.csv`.
4. `Compare-Object (Import-Csv $env:TEMP\pre.csv) (Import-Csv $env:TEMP\post.csv) -Property FullName,Length` → MUST be EMPTY. Any new file under the 2011 root = extraction occurred = FAIL.
5. Report assertions: file exists; all five fixed sections present; `Verdict` is one of the
   three permitted strings; free disk space did not drop by ≳6 GB.
6. Teardown: `Remove-Item $env:TEMP\pre.csv,$env:TEMP\post.csv`.

## T7 — missions.json dead-data note (LOWEST)

No code change. `APBWorldService.cpp:54-63` loads `mission_templates.json` and calls
`ApplyTo`, implemented at `APBMission.cpp:123-129`, which overlays canonical retail titles;
`APBCatalog.cpp:71-77` never reads `description`. So the 40 `description:null` values are
dead data, NOT a runtime defect — the earlier "raw IDs surface at runtime" claim was refuted.

Add a short note recording that these nulls are a known completeness gap with no runtime
effect, so a future agent does not "fix" them by inventing plausible descriptions — which
`Content/Data/AGENTS.md`'s own anti-pattern forbids.

**Destination is `Content/Data/AGENTS.md` — ONE file, decided (Oracle blocker 11).** The
earlier draft offered "`Content/Data/AGENTS.md` or the ARCHITECTURE catalog section", leaving
the worker a coin-flip. Chosen: `Content/Data/AGENTS.md`, because that file already owns the
"do not invent catalog values" anti-pattern (lines 24-27) the note reinforces, so the warning
sits where an agent editing catalogs will actually read it. Do NOT also add it to
`ARCHITECTURE.md` — duplicated guidance drifts.

**Verify:** `Select-String Content\Data\AGENTS.md -Pattern 'description.*null|dead data'` → ≥1
hit, AND `Select-String work\ARCHITECTURE.md -Pattern 'dead data'` → NO hits (single home).
Usefulness of the wording is not mechanically verifiable.

## Gate inventory

| Gate | State | Used by |
|---|---|---|
| `tools/check_source_registry.ps1` | NEW (T1E) | S1, S2 |
| `tools/check_catalog_provenance.ps1` | NEW (T3B) | S9, S10, S11 |
| `tools/validate_fidelity_oracle.ps1` | EXISTING, unchanged | S12 regression |
| `tools/check_port_contract.ps1` | EXISTING, unchanged | S13 regression |
| `tests/build_and_run.ps1` | EXISTING, unchanged | S14 regression |

## Commit strategy (one concern each — NO commit unless the user asks)

1. `tools/source_registry.json` (T1A)
2. `resolve_source_root.ps1` (T1B)
3. `extract_with_umodel.ps1` stale-binary + capability + message fix (T1C)
4. `parse_privateserver.py` parameterization (T1D)
5. `check_source_registry.ps1` (T1E)
6. `ledger_source.py` shared classifier + `build_import_status.py` consumer update (T2A)
   — the classifier ships WITH its first consumer, never as an orphan commit
7. `import_ledger.json` migration + `migrate_ledger_source.py` (T2B)
8. `ARCHITECTURE.md` schema correction (T2C)
9. `check_import_status_table.py` structural checker + regenerated `IMPORT_STATUS.md` (T2D)
10. catalog provenance manifest (T3A)
11. `check_catalog_provenance.ps1` (T3B)
12. root `AGENTS.md` + `tools/AGENTS.md` + `ASSET_PIPELINE.md` discoverability (T4A/T4B/T4C)
13. delete shadow generator (T5A)
14. archive inventory script + report (T6A) and the missions.json note (T7)

Commit ORDER must respect the wave graph: commit 6 strictly before 7 (C4 hard edge), and
commits 1-2 before 12 (T4A/T4B/T4C document files that must already exist).

## End-of-work gates

```powershell
pwsh -File D:\APBReloaded\tools\check_source_registry.ps1        # SOURCE_REGISTRY_PASS
pwsh -File D:\APBReloaded\tools\check_catalog_provenance.ps1     # CATALOG_PROVENANCE_PASS
pwsh -File D:\APBReloaded\tools\validate_fidelity_oracle.ps1     # FIDELITY_ORACLE_PASS
pwsh -File D:\APBReloaded\tools\check_port_contract.ps1          # APB_PORT_CONSISTENCY_PASS
powershell -File D:\APBReloaded\tests\build_and_run.ps1          # 0 failures
```

No UE target build is required — this scope changes no Domain or UE C++ source. Run the
domain suite anyway as a regression check. Per `work/AGENTS.md`, record for every completed
task: exact command, date, terminal marker, result, and log path. A milestone may NOT be
called proven from code inspection alone.

**Standing proof of the no-C++-touch claim — BASELINE DELTA, not "empty" (self-review fix,
verified 2026-07-26).** An earlier draft of this section required
`git status --porcelain Source/` to be EMPTY at end of work. That gate is UNSATISFIABLE ON
ARRIVAL: `Source/` already carries **102** pre-existing worktree entries (32 ` M` modified,
70 `??` untracked) from unrelated in-flight work, which the root contract line 3 explicitly
requires be preserved. A gate that is red before any task runs proves nothing and would
pressure a worker into reverting someone else's work.

**Status lines are NOT sufficient — the proof is a HASH INVENTORY (re-review #2, residual 8).**
A porcelain-line comparison has a hidden-modification loophole: a file already listed as ` M`
or `??` keeps the SAME status line no matter how much further it is edited. With 102 `Source/`
entries already dirty, a status-only delta would silently permit editing any of them. The
proof is therefore a path + SHA-256 inventory, not a status listing.

Corrected procedure:

1. BEFORE the first edit, capture a Source inventory (tracked-modified AND untracked, content
   hashed):

   ```powershell
   Get-ChildItem -LiteralPath Source -Recurse -File |
     Sort-Object FullName |
     ForEach-Object { "{0}`t{1}" -f $_.FullName, (Get-FileHash -Algorithm SHA256 $_.FullName).Hash } |
     Set-Content "$env:TEMP\m16_source_baseline.tsv"
   ```

   Record the file count and the 102-entry porcelain count in the notepad.
2. At end of work, capture the same inventory to `$env:TEMP\m16_source_final.tsv`.
3. `Compare-Object (Get-Content "$env:TEMP\m16_source_baseline.tsv") (Get-Content "$env:TEMP\m16_source_final.tsv")`
   → MUST be EMPTY. Because each line carries the CONTENT HASH, this now catches: a further
   edit to an already-dirty file, a new file, a deleted file, and a same-length in-place
   mutation — none of which a status-line diff would see.
4. Targeted assertion for the one file actually at risk (T1D): the opcode header must be
   BYTE-IDENTICAL to its committed state —
   `git diff --quiet -- Source/APBReloaded/Domain/APBPrivateServerOpcodes.h` → exit 0.
   Verified 2026-07-26: this header is currently CLEAN and is NOT among the 102 entries, so
   this assertion is meaningful rather than vacuous. Retained ALONGSIDE the inventory: the
   inventory proves "nothing under Source changed", this proves "and the one generated header
   still matches HEAD".
5. Teardown: `Remove-Item "$env:TEMP\m16_source_baseline.tsv","$env:TEMP\m16_source_final.tsv"`.

**Whole-repo final sweep — ALLOWLIST of expected deltas (re-review #1, new defect 2).** An
earlier draft said the final sweep should show "ONLY intended deliverables" while ALSO
requiring `Compare-Object` against the baseline to be empty. Those two demands contradict
each other: every intended deliverable IS a delta, so an empty comparison would mean the
milestone produced nothing. Corrected success definition:

1. Capture a whole-repo baseline BEFORE the first edit:
   `git status --porcelain | Sort-Object | Set-Content "$env:TEMP\m16_repo_baseline.txt"`.
2. At end of work capture `$env:TEMP\m16_repo_final.txt` the same way.
3. `Compare-Object` the two. For EVERY line present in final-but-not-baseline, the path MUST
   appear in this allowlist of planned outputs:

```
tools/source_registry.json                          (new)
tools/scripts/resolve_source_root.ps1               (new)
tools/scripts/extract_with_umodel.ps1               (modified)
tools/convert/parse_privateserver.py                (modified)
tools/check_source_registry.ps1                     (new)
tools/scripts/ledger_source.py                      (new)
tools/scripts/build_import_status.py                (modified)
tools/scripts/migrate_ledger_source.py              (new)
tools/import_ledger.json                            (modified)
tools/scripts/check_import_status_table.py          (new)
work/IMPORT_STATUS.md                               (modified, GENERATED)
work/ARCHITECTURE.md                                (modified)
Content/Data/catalog_provenance_manifest.json       (new)
tools/check_catalog_provenance.ps1                  (new)
AGENTS.md                                           (modified)
tools/AGENTS.md                                     (modified)
tools/ASSET_PIPELINE.md                             (modified)
Content/Data/AGENTS.md                              (modified)
tools/scripts/build_fidelity_manifests.py           (DELETED)
tools/scripts/inventory_2011_archive.ps1            (new)
work/m16_2011_archive_inventory.md                  (new)
work/m16_reference_infra_plan.md                    (this plan)
```

4. Any delta path NOT on that list is an unintended side effect and must be explained or
   reverted. Any line present in baseline-but-not-final means unrelated in-flight work was
   clobbered — a contract violation under root rule 3.
5. The `Source/`-scoped check from the procedure above must pass (no `Source/` path appears on
   the allowlist at all — that is the point).

**The status allowlist alone has the SAME hidden-modification loophole (re-review #2, new
defect 2).** A baseline-dirty file outside the allowlist keeps its ` M` / `??` line however
much it is further edited, so step 3 would not notice. Add a content check over exactly those
files:

6. From the baseline porcelain listing, take every path that is NOT on the 22-path allowlist —
   these are the in-flight files this milestone must not touch. Hash them BEFORE the first
   edit and again at the end:

   ```powershell
   # before (and again after, to m16_untouched_final.tsv)
   Get-Content "$env:TEMP\m16_repo_baseline.txt" |
     ForEach-Object { $_.Substring(3).Trim('"') } |
     Where-Object { $_ -notin $allowlistPaths } |
     Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
     Sort-Object |
     ForEach-Object { "{0}`t{1}" -f $_, (Get-FileHash -Algorithm SHA256 $_).Hash } |
     Set-Content "$env:TEMP\m16_untouched_baseline.tsv"
   ```

7. `Compare-Object` those two inventories → MUST be EMPTY. Any changed hash means we edited
   someone else's in-flight work; any missing path means we deleted it. Both are root-rule-3
   violations and block completion.
8. Teardown: `Remove-Item "$env:TEMP\m16_repo_baseline.txt","$env:TEMP\m16_repo_final.txt","$env:TEMP\m16_untouched_baseline.tsv","$env:TEMP\m16_untouched_final.tsv"`.

QA teardown todos — each is its OWN tracked todo and needs a captured receipt:

| # | Teardown | Receipt |
|---|---|---|
| 1 | `Remove-Item Content\Data\__gate_probe.json` (S10) | gate rerun → `CATALOG_PROVENANCE_PASS` |
| 2 | Restore manifest from `$env:TEMP\m16_manifest.bak` (S11a/S11b/field control) — NOT `git checkout`, the file is untracked until commit 10 | gate rerun → `CATALOG_PROVENANCE_PASS` |
| 2b | `Remove-Item "$env:TEMP\m16_manifest.bak"` | `Test-Path` → False |
| 2c | Restore the condition-13 catalog from `$env:TEMP\m16_catalog.bak` (NOT `git checkout` — it may have been dirty before we touched it) | post-restore SHA-256 equals pre-mutation SHA-256 |
| 2d | `Remove-Item "$env:TEMP\m16_catalog.bak"` | `Test-Path` → False |
| 3 | `Rename-Item tools\UEViewer\umodel_64.exe.bak umodel_64.exe` (S5) | wrapper rerun → exit 0 |
| 4 | Restore `tools\source_registry.json` from `$env:TEMP\reg.bak`, delete the backup (T1E) | gate rerun → `SOURCE_REGISTRY_PASS` |
| 5 | `Remove-Item "$env:TEMP\opcodes_probe.h"` (T1D) | `Test-Path` → False |
| 6 | `Remove-Item $env:TEMP\pre.csv,$env:TEMP\post.csv` (S15) | `Test-Path` → False |
| 7 | Regenerate `IMPORT_STATUS.md` after the S8 negative-control row | checker rerun → exit 0 |
| 8 | `Remove-Item "$env:TEMP\m16_source_baseline.tsv","$env:TEMP\m16_source_final.tsv"` | `Test-Path` → False |
| 9 | Final sweep: whole-repo status delta (every new line on the 22-path allowlist, no baseline line lost) AND the untouched-file HASH inventory comparison EMPTY AND the `Source/` hash inventory comparison EMPTY | both `Compare-Object` outputs + terminal capture |
| 10 | `Remove-Item "$env:TEMP\m16_repo_baseline.txt","$env:TEMP\m16_repo_final.txt","$env:TEMP\m16_untouched_baseline.tsv","$env:TEMP\m16_untouched_final.tsv"` | `Test-Path` → False |

A leftover probe file, renamed binary, dirty manifest, or temp artifact means NOT done.

## NOT mechanically verifiable — flagged honestly

- **T4A / T4B / T4C prose quality.** A gate can confirm a string is present; it cannot
  confirm the wording is unambiguous or that the 67-69 decomposition preserves intent.
  Requires human review. This is the single largest residual risk, since the original
  proposal was almost entirely prose.
- **T7 note.** Presence is checkable; usefulness is not.
- **`unknown` sentinel accuracy (T2B).** Bucket counts are checkable; whether a given
  entry's true origin build was genuinely unrecoverable versus insufficiently investigated
  is a judgement call. Record the per-bucket counts so a later pass can revisit.
- **C1 mirror equivalence.** The plan deliberately does NOT assert the two retail copies
  are identical; bit-identity remains unestablished and is out of scope. The warn-only
  posture is a policy choice, not a proof.
- **Archive uniqueness (T6A).** The report proves path-level overlap. Content-level
  divergence for same-named files would need hashing, deferred unless the report shows cause.

## Review resolution record (Oracle hostile review, 2026-07-26)

The first draft of this plan was REJECTED by an adversarial reviewer with 11 blocking issues.
All 11 are resolved above. Recorded here so a later agent does not re-litigate settled points
or silently regress a fix.

| # | Blocking issue | Resolution | Independently re-verified? |
|---|---|---|---|
| 1 | `-AllowMirror` switch let a caller bypass C1's fail-closed mirror rule | Switch REMOVED; approval is registry DATA (`inventory_approved`) only; added a bypass-proof test asserting the flag errors as unknown | design fix |
| 2 | v2 schema could not represent its own output: `2011+retail` missing from enum, and key is `version` not `schema_version` | Enum is now the five-member `retail\|2011\|2011+retail\|apbdb\|unknown`; migration sets the EXISTING `version: 2` | YES — on disk: keys are `version/updated/statuses/note/entries`; values 8 `retail`, 3 `2011`, 1 `2011+retail` |
| 3 | T2A "fall back to `source`" would render absolute paths into a column named `source_build`, preserving the C4 lie | Shared `tools/scripts/ledger_source.py` `classify(raw, asset_key)` used by BOTH T2A and T2B; build column is always an enum member. Signature corrected in re-review #1 — a single-arg `classify(raw)` could not implement the two `Content\Extracted\` rows, which need `asset_key` | design fix |
| 4 | S8's `^\|\s*[CD]:\\` grep only inspects the FIRST cell (`asset_key`) | Replaced with column-aware structural parser `check_import_status_table.py`; exact 581-row assertion; negative control | design fix |
| 5 | T3A/T3B defined no field semantics; membership-only gate would pass empty registrations | Full field definitions table (incl. what `source_hash` hashes and why) + **13** explicit gate failure conditions. Re-review #1 added condition 13: the gate must compare `source_hash` to the ACTUAL SHA-256 of the keyed output, not merely its 64-hex shape — without it the advertised drift tripwire tripped on nothing | design fix |
| 6 | Allowlist baseline stored in the file being edited → "must not grow" guarded nothing; `~20/~45` unverifiable | Baseline is a CONSTANT in the gate script; cross-checked against the manifest's self-report; exact on-disk count used | YES — 65 top-level `Content/Data/*.json` |
| 7 | Contract demanded RED→GREEN for S12-S14, which are already-green regression gates | RED→GREEN restricted to S1-S11/S15; regressions capture before-GREEN and after-GREEN; faking RED forbidden | design fix |
| 8 | T1D "normal run" writes `Source/.../APBPrivateServerOpcodes.h`, contradicting the no-C++-touch claim | Added `--out`; verification writes to TEMP and hash-compares; drift → STOP and escalate. The end-of-work proof is a BASELINE DELTA plus `git diff --quiet` on that one header — the earlier "empty `git status --porcelain Source/`" wording was itself unsatisfiable and was removed from T1D in re-review #1 | YES — generator writes that header, which `APBGameInstanceSubsystem.cpp` includes; header currently CLEAN, not among the 102 pre-existing entries |
| 9 | T6A: unchanged archive mtime does not prove non-extraction; "recommendation" unconstrained | Pre/post filesystem snapshot diff must be EMPTY; exact 7z resolution + `l -ba -slt`; normalization rules; verdict constrained to 3 fixed strings | design fix |
| 10 | Registry claimed repo-wide "single point of truth" while most tools hardcode the Steam root | Scope narrowed to 3 migrated callers; T1E gates only those 3; T4B must state the deferral explicitly | YES — 66 files under `tools/` embed the root (`Program Files (x86)` → 66, `steamapps` → 64) |
| 11 | T1C/T1E/T7 left worker decisions (probe package, "break one alias", two candidate files) | Exact mutation/restore commands; T7 destination decided as `Content/Data/AGENTS.md` only. Probe package CORRECTED in re-review #1: the first fix named `APBGame\CookedPC\Characters.upk`, a path that DOES NOT EXIST — real packages live under the registry's `packages_subpath` | YES — `APBGame\CookedPC` absent; `APBGame\Content\Release\Packages` holds 6,620 `.upk`; `Character\Contact\Contact_LaRocha.upk` present |

Reviewer findings ACCEPTED as correct and left unchanged in the plan: the C4 consumer-first
edge is real (`build_import_status.py` is the SOLE `.py`/`.ps1` reader of a ledger entry's
`source`); `validate_fidelity_oracle.ps1` reads only `asset_key`/`status`; the shadow
generator's schema is genuinely incompatible with the validator; `extract_with_umodel.ps1:16`
genuinely points at the stale `umodel.exe`; T2B is the highest-risk task (blast radius: all
581 rows + generated report); and adding `catalog_provenance_manifest.json` is runtime-safe
because `APBCatalog.cpp` / `WorldService::InitFromDataDir` load explicit filenames — verified
independently: no `*.json` glob enumeration exists anywhere in `Source/APBReloaded/Domain/`.

All four originally-rejected scope expansions remain rejected: no content-addressed lockfile,
no invented "derived extracts" category, no broad undefined-verb authorization prose, no
fail-closed exact-inventory mirror gate.

### Re-review #1 (same reviewer, APPROVE WITH CHANGES) — all items closed

Six blockers were confirmed CLOSED on first pass (2, 4, 6, 7, 10, and 9-pending-alias-fix).
Five were STILL OPEN and are now fixed, plus two newly-introduced defects:

| Item | Defect found in my FIX | Resolution | Independently re-verified? |
|---|---|---|---|
| 1 (residual) | T6A used `-Alias apb_2011`, which is not a registry alias — T1A defines `ref_2011` / `ref_2011_archive` | Both aliases corrected; noted that `ref_2011_archive` resolves the archive FILE so no subpath is joined, and that listing a quarantined file alias is not mirror selection | YES — alias names read from the T1A registry block |
| 3 (residual) | `classify(raw)` could not implement the two `Content\Extracted\` derivation rows | Signature is `classify(raw, asset_key)`; both callers must pass it; documented as pure and total | design fix |
| 5 (residual) | Gate checked `source_hash` SYNTAX only, so the drift tripwire never tripped | Added failure condition 13 (hash VALUE equality) + a catalog-mutation negative control; documented the intended coupling that editing a catalog requires updating its hash | design fix |
| 8 (residual) | T1D still carried the stale "`git status --porcelain Source/` → EMPTY" line, contradicting the corrected baseline-delta procedure | Stale line replaced with the targeted `git diff --quiet` header assertion + an explicit pointer to the baseline-delta gate | YES — 102 pre-existing entries (32 ` M`, 70 `??`) |
| 11 (residual) | Probe package and its only fallback pointed at a nonexistent `APBGame\CookedPC` | Probe resolves via the registry's `packages_subpath`; preferred `Character\Contact\Contact_LaRocha.upk`; fallback sorts on `FullName` (not `Name`, which is non-deterministic across dirs) | YES — 6,620 `.upk` under `APBGame\Content\Release\Packages`; LaRocha present |
| NEW 1 | S11 teardown used `git checkout` on the manifest, which is UNTRACKED until commit 10 — checkout would fail and leave a mutated manifest | Teardown switched to a `$env:TEMP\m16_manifest.bak` file backup; `git checkout` retained only for the genuinely-tracked catalog in the condition-13 control | design fix |
| NEW 2 | Final sweep demanded both "only intended deliverables" AND an empty `Compare-Object` — mutually contradictory, since deliverables ARE deltas | Success redefined as: every new delta path must appear on an explicit 22-path allowlist, no baseline line may be lost, and the `Source/`-scoped delta must be empty | design fix |

### Re-review #2 (same reviewer, APPROVE WITH CHANGES) — final round, all items closed

Round 2 confirmed items 1, 5 and 11 CLOSED. Four remained open; all are now fixed. Reviewer
rounds are exhausted at 2 per protocol, and each remaining item came with an exact prescribed
edit, so these were applied directly rather than looping a third time.

| Item | Defect still present after re-review #1 | Resolution | Independently re-verified? |
|---|---|---|---|
| 3 (2nd residual) | After migration entries have NO legacy `source`, so T2A could not "always call `classify`" and render both ledgers identically — a blind call would read `None` | Added an explicit per-entry resolution order: if `source_build` present, READ and validate it against the enum; else DERIVE via `classify(entry["source"], entry["asset_key"])`. Both branches yield the same enum, neither can emit a path | design fix |
| 8 (2nd residual) | **Porcelain status lines cannot detect further edits to an already-dirty file** — a file already ` M` or `??` keeps the same status line no matter how much more it is edited, so with 102 dirty `Source/` entries a status-only delta permitted editing any of them | Replaced the status-line delta with a path + SHA-256 **hash inventory** over `Source/`, compared pre/post; catches further edits to dirty files, additions, deletions, and same-length in-place mutations. Targeted `git diff --quiet` on the opcode header retained alongside it | YES — the loophole is real: status lines for the 32 ` M` / 70 `??` entries are invariant under further edits |
| NEW 1 (2nd) | A bare pathless `git checkout` survived in the malformed-field teardown, and even the *tracked* condition-13 catalog may carry a PRE-EXISTING dirty edit that checkout would destroy (root rule 3 violation) | Binding rule added: never use `git checkout` as a teardown at all. Back up manifest AND catalog to `$env:TEMP\*.bak` first, restore via `Copy-Item -Force`; restore receipt is a SHA-256 match, not the absence of a `git status` line. Also fixed duplicate step numbering (two `5.`/`6.`) in that verify list | design fix |
| NEW 2 (2nd) | The 22-path status allowlist carried the SAME hidden-modification loophole for baseline-dirty files outside the allowlist | Added a hash inventory over exactly those in-flight files (baseline-dirty ∖ allowlist), compared pre/post and required EMPTY; any changed hash = we edited someone else's work, any missing path = we deleted it, both blocking | design fix |

Reviewer statement accepted on the baseline-delta design: the targeted clean-header diff closes
the actual T1D risk. Its caveat — that a status-only comparison could still hide a change to an
already-dirty file — is what drove the hash-inventory replacement above, so the residual hole
the reviewer identified is now closed rather than merely acknowledged.

**Net effect of three review rounds:** 11 original blockers + 5 residuals + 4 fix-introduced
defects = 20 defects found and closed before a single line of implementation. Nine were
independently re-verified against the actual repo and retail install rather than accepted on
assertion; two reviewer claims were themselves corrected in the process (the plan's own
`git status` gate was unsatisfiable on arrival, and one of my greps had returned a false
negative that understated the hardcoded-root problem by a factor of 22).



