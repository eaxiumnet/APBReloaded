[CmdletBinding()]
param(
    [string]$ProjectRoot = "",
    [string]$Scratch = "",
    [string]$AllowlistOverride = "",
    [string]$Editor = "",
    [switch]$RequireCookAuditPass,
    [int]$MaxLaunches = 2,
    [int]$TimeoutSec = 120
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($Scratch)) {
    $Scratch = Join-Path $ProjectRoot ".omo/evidence/m3r_asset_qa"
}
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
if ([string]::IsNullOrWhiteSpace($Editor)) {
    $Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
}
$Project = Join-Path $ProjectRoot "APBReloaded.uproject"
$RunId = [Guid]::NewGuid().ToString("N").Substring(0, 12)
$CanonicalAllowlist = Join-Path $ProjectRoot "Content/Data/verified_asset_allowlist.json"

function Write-Step([string]$Message) {
    Write-Host "[m3r_asset_qa] $Message"
}

function Stop-EditorProcs {
    try {
        Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue |
            Stop-Process -Force -ErrorAction SilentlyContinue
    } catch {}
}

function Test-EditorAlive($proc) {
    if ($null -eq $proc) { return $false }
    if (-not $proc.HasExited) { return $true }
    return $null -ne (Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue)
}

function Invoke-EditorProbe {
    param(
        [string[]]$LegArgs,
        [string]$MarkerFile,
        [scriptblock]$IsDone
    )
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $proc = $null
    $ok = $false
    for ($att = 1; $att -le $MaxLaunches -and -not $ok -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec; $att++) {
        $proc = Start-Process -FilePath $Editor -ArgumentList $LegArgs -PassThru `
            -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
        while ((Test-EditorAlive $proc) -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
            Start-Sleep 2
            if (Test-Path $MarkerFile) {
                $c = Get-Content $MarkerFile -Raw -ErrorAction SilentlyContinue
                if (& $IsDone $c) { $ok = $true; Start-Sleep 2; break }
            }
        }
        if (-not $ok -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) { Stop-EditorProcs }
    }
    return [pscustomobject]@{ Ok = $ok; Proc = $proc }
}

function Get-FileSha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return "" }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

Write-Step "run_id=$RunId scratch=$Scratch"

# 1. Canonical allowlist integrity anchor (captured before any probe).
$CanonicalHashBefore = Get-FileSha256 $CanonicalAllowlist
if ([string]::IsNullOrWhiteSpace($CanonicalHashBefore)) {
    throw "M3R_ASSET_QA_FAIL reason=missing_canonical_allowlist path=$CanonicalAllowlist"
}
Write-Step "canonical_allowlist_sha256=$CanonicalHashBefore"

# 2. Static load-token audit (task-17/19 wiring must keep this clean).
$StaticAuditLog = Join-Path $Scratch "static_audit_$RunId.log"
$StaticAuditJson = Join-Path $Scratch "static_audit_$RunId.json"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot "tools/check_verified_asset_static_audit.ps1") -Output $StaticAuditJson 2>&1 |
    Tee-Object -FilePath $StaticAuditLog
$StaticAuditText = Get-Content -LiteralPath $StaticAuditLog -Raw
if ($StaticAuditText -notmatch "VERIFIED_ASSET_STATIC_AUDIT_PASS") {
    throw "M3R_ASSET_QA_FAIL reason=static_audit_failed log=$StaticAuditLog"
}
Write-Step "static_audit PASS"

# 3. Static/serialized cook reference audit (task-20 closure; informative unless required).
$CookAuditLog = Join-Path $Scratch "cook_audit_$RunId.log"
$CookAuditJson = Join-Path $Scratch "cook_audit_$RunId.json"
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot "tools/check_static_cook_assets.ps1") -Output $CookAuditJson 2>&1 |
    Tee-Object -FilePath $CookAuditLog
$CookAuditText = Get-Content -LiteralPath $CookAuditLog -Raw
$CookAuditPass = $CookAuditText -match "STATIC_COOK_ASSET_AUDIT_PASS"
if (-not $CookAuditPass) {
    Write-Step "cook_audit FAIL (unverified references remain; task 18/19 routing closure pending)"
    if ($RequireCookAuditPass) {
        throw "M3R_ASSET_QA_FAIL reason=cook_audit_required log=$CookAuditLog"
    }
} else {
    Write-Step "cook_audit PASS"
}

# 4. Positive probe against the canonical allowlist.
# The probe writes its verdict to <scratch>/asset_allowlist.log; use a dedicated
# scratch subdirectory per run so positive/negative logs never collide.
$PositiveScratch = Join-Path $Scratch "positive_$RunId"
New-Item -ItemType Directory -Force -Path $PositiveScratch | Out-Null
$PositiveLog = Join-Path $PositiveScratch "asset_allowlist.log"
$PositiveArgs = @(
    $Project, "/Game/Maps/Lvl_APB_Frontend", "-game",
    "-APBProbe=asset_allowlist", "-APBStrictAssetAllowlist", "-APBScratch=$PositiveScratch",
    "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$PositiveRes = Invoke-EditorProbe -LegArgs $PositiveArgs -MarkerFile $PositiveLog -IsDone {
    param($c)
    $c -match "RUNTIME_ALLOWLIST_ALLOW_OK" -and
        $c -match "RUNTIME_ALLOWLIST_REJECT_OK" -and
        $c -match "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
}
Stop-EditorProcs
if (-not $PositiveRes.Ok) {
    throw "M3R_ASSET_QA_FAIL reason=positive_probe_no_verdict log=$PositiveLog"
}
$PositiveText = Get-Content -LiteralPath $PositiveLog -Raw
if ($PositiveText -notmatch "RUNTIME_ALLOWLIST_ALLOW_OK" -or
    $PositiveText -notmatch "RUNTIME_ALLOWLIST_REJECT_OK" -or
    $PositiveText -notmatch "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK") {
    throw "M3R_ASSET_QA_FAIL reason=positive_probe_markers_missing log=$PositiveLog"
}
Write-Step "positive_probe PASS (ALLOW_OK + REJECT_OK + NO_SUBSTITUTE_OK)"

# 5. Negative probe(s) against an override copy. Never touches the canonical file.
$NegativePath = $AllowlistOverride
if ([string]::IsNullOrWhiteSpace($NegativePath)) {
    $NegativePath = Join-Path $Scratch "allowlist_negative_empty_$RunId.json"
    $Canonical = Get-Content -LiteralPath $CanonicalAllowlist -Raw | ConvertFrom-Json
    $NegativeDoc = [ordered]@{
        schema_version = $Canonical.schema_version
        generated_by = "tools/run_m3r_asset_qa.ps1 (negative fixture)"
        generated_at = (Get-Date).ToUniversalTime().ToString("yyyy-MM-dd")
        ledger_version = $Canonical.ledger_version
        entries = @()
    }
    $NegativeDoc | ConvertTo-Json -Depth 10 |
        Set-Content -LiteralPath $NegativePath -Encoding utf8
    Write-Step "negative fixture generated: $NegativePath (empty entries)"
}
$NegativeScratch = Join-Path $Scratch "negative_$RunId"
New-Item -ItemType Directory -Force -Path $NegativeScratch | Out-Null
$NegativeLog = Join-Path $NegativeScratch "asset_allowlist.log"
$NegativeArgs = @(
    $Project, "/Game/Maps/Lvl_APB_Frontend", "-game",
    "-APBProbe=asset_allowlist", "-APBStrictAssetAllowlist", "-APBScratch=$NegativeScratch",
    "-APBAllowlistOverride=$NegativePath",
    "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$NegativeRes = Invoke-EditorProbe -LegArgs $NegativeArgs -MarkerFile $NegativeLog -IsDone {
    param($c)
    $c -match "RUNTIME_ALLOWLIST_ALLOW_BLOCKED" -and
        $c -match "RUNTIME_ALLOWLIST_REJECT_OK" -and
        $c -match "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
}
Stop-EditorProcs
if (-not $NegativeRes.Ok) {
    throw "M3R_ASSET_QA_FAIL reason=negative_probe_no_verdict log=$NegativeLog"
}
$NegativeText = Get-Content -LiteralPath $NegativeLog -Raw
if ($NegativeText -notmatch "RUNTIME_ALLOWLIST_ALLOW_BLOCKED" -or
    $NegativeText -notmatch "RUNTIME_ALLOWLIST_REJECT_OK" -or
    $NegativeText -notmatch "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK") {
    throw "M3R_ASSET_QA_FAIL reason=negative_probe_markers_missing log=$NegativeLog"
}
Write-Step "negative_probe PASS (ALLOW_BLOCKED + REJECT_OK + NO_SUBSTITUTE_OK) override=$NegativePath"

# 6. Canonical integrity: probes must never modify the canonical allowlist.
$CanonicalHashAfter = Get-FileSha256 $CanonicalAllowlist
if ($CanonicalHashAfter -ne $CanonicalHashBefore) {
    throw "M3R_ASSET_QA_FAIL reason=canonical_allowlist_modified before=$CanonicalHashBefore after=$CanonicalHashAfter"
}
Write-Step "canonical_allowlist_integrity PASS"

$Evidence = [ordered]@{
    schema_version = 1
    generated_by = "tools/run_m3r_asset_qa.ps1"
    run_id = $RunId
    time = (Get-Date).ToUniversalTime().ToString("o")
    static_audit_pass = $true
    cook_audit_pass = $CookAuditPass
    positive_probe = @{
        allow_ok = $true
        reject_ok = $true
        no_substitute_ok = $true
        log = (Split-Path $PositiveLog -Leaf)
    }
    negative_probe = @{
        allow_blocked = $true
        reject_ok = $true
        no_substitute_ok = $true
        override = $NegativePath.Replace('\', '/')
        log = (Split-Path $NegativeLog -Leaf)
    }
    canonical_allowlist = @{
        path = "Content/Data/verified_asset_allowlist.json"
        sha256_before = $CanonicalHashBefore
        sha256_after = $CanonicalHashAfter
    }
}
$EvidenceJson = Join-Path $Scratch "m3r_asset_qa_$RunId.json"
$Json = $Evidence | ConvertTo-Json -Depth 6
[System.IO.File]::WriteAllText($EvidenceJson, $Json, [System.Text.UTF8Encoding]::new($false))

Write-Output "M3R_ASSET_QA_PASS run_id=$RunId static_audit=1 cook_audit=$([int]$CookAuditPass) positive=1 negative=1 canonical_integrity=1 evidence=$EvidenceJson"
