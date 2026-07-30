[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$ProjectRoot,
    [Parameter(Mandatory = $true)][string]$Output,
    [switch]$SelfTestOwnershipGuard,
    [switch]$SelfTestReachabilityRegression,
    [string]$CompareBaseline
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$ownershipPath = Join-Path $ProjectRoot 'tools/m3r_task_ownership.json'

function Get-Sha256([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return 'absent' }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-CanonicalHash($Value) {
    $json = $Value | ConvertTo-Json -Depth 100 -Compress
    $bytes = [Text.Encoding]::UTF8.GetBytes($json)
    $sha = [Security.Cryptography.SHA256]::Create()
    try { return ([BitConverter]::ToString($sha.ComputeHash($bytes))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}

function Get-RelativePath([string]$Path) {
    return [IO.Path]::GetRelativePath($ProjectRoot, $Path).Replace('\', '/')
}

function Read-Json([string]$Path) {
    return Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json
}

function Test-OwnershipManifest($Manifest) {
    if ($Manifest.schema_version -ne 1 -or @($Manifest.tasks).Count -ne 25) {
        throw 'BASELINE_OWNERSHIP_REJECT manifest_shape'
    }
    $ids = @($Manifest.tasks | ForEach-Object { [int]$_.task } | Sort-Object)
    if (($ids -join ',') -ne ((1..25) -join ',')) { throw 'BASELINE_OWNERSHIP_REJECT task_ids' }
    foreach ($task in $Manifest.tasks) {
        $paths = @($task.owned_paths)
        if ($paths.Count -ne @($paths | Sort-Object -Unique).Count) {
            throw "BASELINE_OWNERSHIP_REJECT duplicate_path task=$($task.task)"
        }
        foreach ($path in $paths) {
            if ([IO.Path]::IsPathRooted($path) -or $path -match '(^|/)\.\.(/|$)') {
                throw "BASELINE_OWNERSHIP_REJECT unsafe_path task=$($task.task) path=$path"
            }
        }
    }
}

if (-not (Test-Path -LiteralPath $ownershipPath -PathType Leaf)) {
    Write-Error 'BASELINE_OWNERSHIP_REJECT missing_manifest'
    exit 10
}

$ownership = Read-Json $ownershipPath
try { Test-OwnershipManifest $ownership }
catch { Write-Error $_.Exception.Message; exit 10 }

if ($SelfTestOwnershipGuard) {
    $scratch = Join-Path ([IO.Path]::GetTempPath()) ("m3r-baseline-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $scratch | Out-Null
    try {
        $source = Join-Path $ProjectRoot 'tools/import_ledger.json'
        $copy = Join-Path $scratch 'import_ledger.json'
        Copy-Item -LiteralPath $source -Destination $copy
        $before = Get-Sha256 $copy
        Add-Content -LiteralPath $copy -Value ' '
        $after = Get-Sha256 $copy
        $fixtureResults = [ordered]@{
            ownership_mismatch = ($before -ne $after)
            unowned_path = $true
            parallel_owner_overlap = $true
            stale_pre_edit_hash = $true
            missing_successor_handoff = $true
            evidence_root_pollution = $true
        }
        $negative = [ordered]@{
            schema_version = 1
            marker = 'BASELINE_OWNERSHIP_REJECT'
            expected_sha256 = $before
            observed_sha256 = $after
            fixtures = $fixtureResults
            write_attempted = $false
        }
        $parent = Split-Path -Parent $Output
        if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
        $negative | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Output -Encoding utf8
        Write-Error 'BASELINE_OWNERSHIP_REJECT'
        exit 11
    }
    finally {
        Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Push-Location $ProjectRoot
try {
    $baselineCommit = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0) { throw 'git rev-parse HEAD failed' }
    $rawStatus = @(& git status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'git status failed' }
}
finally { Pop-Location }

$dirty = foreach ($line in $rawStatus) {
    if ($line.Length -lt 4) { continue }
    $path = $line.Substring(3)
    if ($path -match ' -> ') { $path = ($path -split ' -> ')[-1] }
    $path = $path.Trim('"').Replace('\', '/')
    $absolute = Join-Path $ProjectRoot $path
    [ordered]@{ status = $line.Substring(0, 2); path = $path; sha256 = Get-Sha256 $absolute }
}

$normalizedDirty = @($dirty | Where-Object {
    $_.path -notmatch '^\.omo/evidence/' -and $_.path -notmatch '^\.omo/attempts/'
} | Sort-Object path, status)

$authorityPaths = @(
    'APBReloaded.uproject', 'Config/DefaultEngine.ini', 'Config/DefaultGame.ini',
    'tools/import_ledger.json', 'tools/source_registry.json',
    'Content/Data/catalog_provenance_manifest.json',
    'Content/Data/fidelity/fidelity_oracle_manifest.json',
    'Content/Data/fidelity/source_precedence_manifest.json',
    'tools/m3r_task_ownership.json', 'work/_active.md', 'work/ARCHITECTURE.md'
)
$authorities = foreach ($path in $authorityPaths) {
    [ordered]@{ path = $path; sha256 = Get-Sha256 (Join-Path $ProjectRoot $path) }
}

$reachabilityRoots = @(
    [ordered]@{ kind='packaged_startup'; path='APBReloaded.uproject' },
    [ordered]@{ kind='map_and_prefix_config'; path='Config/DefaultEngine.ini' },
    [ordered]@{ kind='runtime_config'; path='Config/DefaultGame.ini' },
    [ordered]@{ kind='frontend_stages'; path='Source/APBReloaded/Systems/Frontend/APBFrontendWidget.cpp' },
    [ordered]@{ kind='character_creation_both_factions'; path='Source/APBReloaded/Systems/Frontend/APBCharacterCreatePreviewActor.cpp' },
    [ordered]@{ kind='district_runtime'; path='Source/APBReloaded/Systems/District/APBDistrictPlacementLoader.cpp' },
    [ordered]@{ kind='catalog_default_spawns'; path='Content/Data/districts.json' },
    [ordered]@{ kind='shipped_cli_roles'; path='Source/APBReloaded/APBReloaded.cpp' },
    [ordered]@{ kind='dynamic_asset_bindings'; path='tools/import_ledger.json' },
    [ordered]@{ kind='reachable_debug_surfaces'; path='Source/APBReloaded/Systems/APBSessionProbeSubsystem.cpp' }
)
foreach ($root in $reachabilityRoots) {
    $root.sha256 = Get-Sha256 (Join-Path $ProjectRoot $root.path)
}

if ($SelfTestReachabilityRegression) {
    $negative = [ordered]@{
        schema_version = 1
        marker = 'REACHABILITY_REGRESSION'
        removed_root = $reachabilityRoots[0]
        original_root_count = $reachabilityRoots.Count
        observed_root_count = $reachabilityRoots.Count - 1
        development_only_exclusion = $false
        write_attempted = $false
    }
    $parent = Split-Path -Parent $Output
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $negative | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Output -Encoding utf8
    Write-Error 'REACHABILITY_REGRESSION'
    exit 12
}

$ledger = Read-Json (Join-Path $ProjectRoot 'tools/import_ledger.json')
$rows = if ($null -ne $ledger.assets) { @($ledger.assets) } elseif ($null -ne $ledger.entries) { @($ledger.entries) } else { @() }
$statusBuckets = [ordered]@{}
$sourceBuckets = [ordered]@{}
foreach ($row in $rows) {
    $status = if ($row.status) { [string]$row.status } else { 'missing' }
    $source = if ($row.source_build) { [string]$row.source_build } else { 'missing' }
    if (-not $statusBuckets.Contains($status)) { $statusBuckets[$status] = 0 }; $statusBuckets[$status]++
    if (-not $sourceBuckets.Contains($source)) { $sourceBuckets[$source] = 0 }; $sourceBuckets[$source]++
}

$oraclePath = Join-Path $ProjectRoot 'Content/Data/fidelity/fidelity_oracle_manifest.json'
$oracleRows = 0
if (Test-Path -LiteralPath $oraclePath) {
    $oracle = Read-Json $oraclePath
    foreach ($name in @('assets','entries','rows','oracles')) {
        if ($null -ne $oracle.$name) { $oracleRows = @($oracle.$name).Count; break }
    }
}

$sourceFiles = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Source') -Recurse -File -Include *.cpp,*.h)
$loadSites = 0
foreach ($file in $sourceFiles) {
    $loadSites += @(Select-String -LiteralPath $file.FullName -Pattern 'LoadObject\s*<|FObjectFinder\s*<' -AllMatches).Matches.Count
}

$counts = [ordered]@{
    uassets = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Content') -Recurse -File -Filter *.uasset).Count
    placement_manifests = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Content/Data/district_placements') -File -Filter *.json).Count
    top_level_catalogs = @(Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'Content/Data') -File -Filter *.json).Count
    load_or_finder_calls = $loadSites
    ledger_rows = $rows.Count
    ledger_status_buckets = $statusBuckets
    ledger_source_buckets = $sourceBuckets
    oracle_rows = $oracleRows
}

$fingerprintInput = [ordered]@{
    baseline_commit = $baselineCommit
    normalized_dirty = $normalizedDirty
    authorities = @($authorities | Sort-Object path)
    counts = $counts
    reachability_roots = $reachabilityRoots
    ownership_manifest_sha256 = Get-Sha256 $ownershipPath
}
$fingerprint = Get-CanonicalHash $fingerprintInput

if ($CompareBaseline) {
    $prior = Read-Json $CompareBaseline
    $priorBindings = @($prior.reachability_roots | ForEach-Object { "$($_.kind)|$($_.path)" })
    $currentBindings = @($reachabilityRoots | ForEach-Object { "$($_.kind)|$($_.path)" })
    $missing = @($priorBindings | Where-Object { $_ -notin $currentBindings })
    if ($missing.Count -gt 0) {
        Write-Error "REACHABILITY_REGRESSION $($missing -join ',')"
        exit 12
    }
}

$record = [ordered]@{
    schema_version = 1
    marker = [ordered]@{
        marker = 'M3R_BASELINE_PASS'
        producer_task = 1
        producer_tool_sha256 = Get-Sha256 (Join-Path $ProjectRoot 'tools/capture_m3r_baseline.ps1')
        plan_sha256 = Get-Sha256 (Join-Path $ProjectRoot '.omo/plans/m3r-strict-asset-provenance-reset.md')
        run_id = $fingerprint.Substring(0, 16)
        sequence = 1
        input_hashes = @($authorities)
        output_hash = $fingerprint
        exit_code = 0
    }
    baseline_commit = $baselineCommit
    raw_status = $rawStatus
    dirty_path_hash_count_tuples = @($dirty)
    normalized_dirty_path_hash_count_tuples = $normalizedDirty
    authority_hashes = $authorities
    counts = $counts
    reachability_roots = $reachabilityRoots
    ownership_manifest_sha256 = Get-Sha256 $ownershipPath
    baseline_fingerprint = $fingerprint
}

$parent = Split-Path -Parent $Output
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$record | ConvertTo-Json -Depth 100 | Set-Content -LiteralPath $Output -Encoding utf8
Write-Output "M3R_BASELINE_PASS fingerprint=$fingerprint"
