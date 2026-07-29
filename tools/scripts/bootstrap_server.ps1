<#
  bootstrap_server.ps1 — M16 (brief #15): prepare / validate a clean APB server working set
  before start_world.ps1 / start_district.ps1 launch the UE5.8 recreation.

  This satisfies the M16 "fresh-server bootstrap from clean Saved\" gate on the ops side:
     1. Ensure the writable persist dir structure exists at Saved\DomainDB (the runtime-
        authoritative path: FPaths::ProjectSavedDir()/"DomainDB", used by APBWorldGameMode
        and APBGameInstanceSubsystem) (JsonDomainStore layout, see
       Source/APBReloaded/Domain/APBPersistence.h §Layout: accounts.json, characters\
       (incl. per-character <account>_<slot>.json + M15 <account>_<slot>_progress.json
       sidecars), auction.json, mail.json). Loads tolerate missing files (fresh = empty),
       so a clean
       bootstrap provisions the *directory structure* and does NOT fabricate state files with
       a hand-guessed schema — the server writes them on first save.
    2. Verify the read-only catalog the world needs to route is present (Content/Data).
    3. Report whether the persist dir is already CLEAN (no prior state) or CARRIES state.

  Switches:
    -Clean   remove existing persist STATE (accounts.json/auction.json/mail.json/characters\)
             — requires -Force to actually delete; otherwise it only reports what it WOULD
             remove. Scoped strictly to the resolved persist dir; never touches Content/Data.
    -DryRun  resolve + validate + report only; create nothing, delete nothing. Exit 0 unless
             a REQUIRED catalog file is missing.

  Exit codes: 0 = server working set ready; 1 = a required catalog file is missing.
#>
[CmdletBinding()]
param(
    [string]$PersistDir = "D:\APBReloaded\Saved\DomainDB",
    [string]$DataDir    = "D:\APBReloaded\Content\Data",
    [switch]$Clean,
    [switch]$Force,
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"

function Write-Log([string]$m) {
    Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m)
}

# --- 1. validate the read-only catalog -------------------------------------------------
# districts.json is REQUIRED (world routing / district port resolution). The rest are
# recommended domain catalogs — missing ones only warn (services fall back to empty).
$required    = @("districts.json")
$recommended = @("contacts_lore.json", "roles.json", "threat_table.json", "vehicles.json")

$missingRequired = @()
foreach ($f in $required) {
    if (Test-Path (Join-Path $DataDir $f)) { Write-Log "catalog OK (required): $f" }
    else { Write-Log "catalog MISSING (required): $f"; $missingRequired += $f }
}
foreach ($f in $recommended) {
    if (Test-Path (Join-Path $DataDir $f)) { Write-Log "catalog OK: $f" }
    else { Write-Log "WARN: recommended catalog missing: $f (service will start empty)" }
}

# --- 2. inspect current persist state --------------------------------------------------
$charDir     = Join-Path $PersistDir "characters"
$stateFiles  = @("accounts.json", "auction.json", "mail.json") | ForEach-Object { Join-Path $PersistDir $_ }
$existingState = @()
$existingState += ($stateFiles | Where-Object { Test-Path $_ })
if (Test-Path $charDir) {
    $existingState += (Get-ChildItem -Path $charDir -Filter *.json -ErrorAction SilentlyContinue | ForEach-Object { $_.FullName })
}
$isClean = ($existingState.Count -eq 0)
Write-Log ("persist dir: {0} -> {1}" -f $PersistDir, ($(if ($isClean) { "CLEAN (no prior state)" } else { "CARRIES {0} state file(s)" -f $existingState.Count })))

# --- 3. optional clean -----------------------------------------------------------------
if ($Clean -and -not $isClean) {
    # Safety: only ever operate inside a path that looks like a Saved persist dir.
    if ($PersistDir -notmatch 'Saved') { throw "Refusing -Clean: '$PersistDir' is not under a Saved\ path" }
    foreach ($p in $existingState) {
        if ($DryRun -or -not $Force) { Write-Log "would remove: $p" }
        else { Remove-Item -LiteralPath $p -Force; Write-Log "removed: $p" }
    }
    if (-not $DryRun -and -not $Force) { Write-Log "NOTE: -Clean is a report only; pass -Force to actually delete"; Write-Host "CLEAN_REFUSED" }
}

# --- 4. provision the persist directory structure -------------------------------------
if ($DryRun) {
    Write-Log "DryRun: would ensure $PersistDir and $charDir exist (no changes made)"
} else {
    New-Item -ItemType Directory -Force -Path $PersistDir | Out-Null
    New-Item -ItemType Directory -Force -Path $charDir    | Out-Null
    Write-Log "ensured persist structure: $PersistDir (+ characters\)"
    Write-Log "server will create accounts.json/auction.json/mail.json on first save (fresh = empty)"
}

if ($missingRequired.Count -gt 0) {
    Write-Log ("PREFLIGHT FAIL: missing required catalog: {0}" -f ($missingRequired -join ', '))
    exit 1
}
Write-Log "PREFLIGHT OK: server working set ready."
Write-Host "BOOTSTRAP_CLEAN_OK"
exit 0
