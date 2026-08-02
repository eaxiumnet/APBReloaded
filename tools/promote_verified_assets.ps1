[CmdletBinding()]
param(
    [string]$ProjectRoot = "",
    [string]$Output = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
if ([string]::IsNullOrWhiteSpace($Output)) {
    $Output = Join-Path $ProjectRoot "Content/Data/verified_asset_allowlist.json"
}
$ledgerPath = Join-Path $ProjectRoot "tools/import_ledger.json"
$ledger = Get-Content -LiteralPath $ledgerPath -Raw | ConvertFrom-Json
$entries = @($ledger.entries)
$allowedSourceBuilds = @("retail", "2011", "2011+retail", "apbdb")
$promoted = [System.Collections.Generic.List[object]]::new()

function Test-SourceLocator {
    param([string]$SourceBuild, [string]$SourceLocator)
    $normalized = $SourceLocator.Replace('\', '/').ToLowerInvariant()
    switch ($SourceBuild) {
        "retail" {
            return $normalized.StartsWith('${retail_steam}/') -or $normalized.StartsWith('retail ') -or $normalized.StartsWith('d:/apbreloaded/content/extracted/')
        }
        "2011" { return $normalized.Contains('2011') }
        "2011+retail" { return $normalized.Contains('2011') -or $normalized.Contains('retail') }
        "apbdb" { return $normalized.Contains('apbdb') }
    }
    return $false
}

foreach ($entry in $entries) {
    if ([string]$entry.status -ne "verified") { continue }
    $assetKey = [string]$entry.asset_key
    $dest = [string]$entry.dest
    $assetClass = [string]$entry.asset_class
    $sourceBuild = [string]$entry.source_build
    if ([string]::IsNullOrWhiteSpace($dest) -or -not $dest.StartsWith("/Game/")) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL asset=$assetKey reason=invalid_destination"
    }
    if ([string]::IsNullOrWhiteSpace($assetClass)) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL asset=$assetKey reason=missing_asset_class"
    }
    $sourceLocator = [string]$entry.source_locator
    if ([string]::IsNullOrWhiteSpace($sourceLocator)) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL asset=$assetKey reason=missing_source_locator"
    }
    if (-not (Test-SourceLocator $sourceBuild $sourceLocator)) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL asset=$assetKey reason=invalid_source_locator"
    }
    if ($allowedSourceBuilds -notcontains $sourceBuild) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL asset=$assetKey reason=invalid_source_build value=$sourceBuild"
    }
    $objectPath = $dest
    $leaf = ($dest -split "/")[-1]
    if ($leaf -notmatch "\.") {
        $objectPath = "$dest.$leaf"
    }
    $promoted.Add([ordered]@{
        asset_key = $assetKey
        object_path = $objectPath
        class = $assetClass
        source_build = $sourceBuild
        source_locator = $sourceLocator
    })
}

$ordered = @($promoted | Sort-Object object_path, asset_key)
$document = [ordered]@{
    schema_version = 1
    generated_by = "tools/promote_verified_assets.ps1"
    generated_at = ([DateTime]$ledger.updated).ToUniversalTime().ToString("yyyy-MM-dd")
    ledger_version = [int]$ledger.version
    entries = $ordered
}
$parent = Split-Path -Parent $Output
if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
$document | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $Output -Encoding utf8

$canonicalOutput = Join-Path $ProjectRoot "Content/Data/verified_asset_allowlist.json"
if ((Resolve-Path -LiteralPath $Output).Path -eq (Resolve-Path -LiteralPath $canonicalOutput -ErrorAction SilentlyContinue).Path) {
    $manifestPath = Join-Path $ProjectRoot "Content/Data/catalog_provenance_manifest.json"
    if (-not (Test-Path -LiteralPath $manifestPath -PathType Leaf)) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL reason=missing_catalog_provenance_manifest"
    }
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $registration = @($manifest.registrations) | Where-Object { $_.catalog -eq "Content/Data/verified_asset_allowlist.json" }
    if (@($registration).Count -ne 1) {
        throw "VERIFIED_ASSET_ALLOWLIST_FAIL reason=missing_or_duplicate_catalog_registration"
    }
    $allowlistHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $canonicalOutput).Hash.ToLowerInvariant()
    $registration[0].source_hash = $allowlistHash
    $manifest | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $manifestPath -Encoding utf8
    Write-Output "CATALOG_ALLOWLIST_HASH_SYNC hash=$allowlistHash manifest=$manifestPath"
}

Write-Output "VERIFIED_ASSET_ALLOWLIST_PASS entries=$($ordered.Count) path=$Output"
