[CmdletBinding()]
param(
    [string]$ProjectRoot = "",
    [string]$Output = "",
    [string]$AllowlistPath = ""
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent $PSScriptRoot
}
$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path

if ([string]::IsNullOrWhiteSpace($AllowlistPath)) {
    $AllowlistPath = Join-Path $ProjectRoot "Content/Data/verified_asset_allowlist.json"
}

# Allowed non-presentational engine internals (task-20 closure rule).
# BasicShapes is intentionally NOT allowed: district routing rejects it.
$EngineInternalPrefixes = @(
    "/Engine/EngineMaterials/DefaultMaterial"
)

function Get-AllowlistPaths([string]$AllowlistPath) {
    if (-not (Test-Path -LiteralPath $AllowlistPath -PathType Leaf)) {
        throw "STATIC_COOK_ASSET_AUDIT_FAIL reason=missing_allowlist path=$AllowlistPath"
    }
    $Allowlist = Get-Content -LiteralPath $AllowlistPath -Raw | ConvertFrom-Json
    $Paths = New-Object System.Collections.Generic.HashSet[string] ([System.StringComparer]::Ordinal)
    foreach ($Entry in @($Allowlist.entries)) {
        if ($Entry.object_path) {
            $P = [string]$Entry.object_path
            [void]$Paths.Add($P)
            # Catalogs may reference the object path with or without the trailing
            # .<leaf> suffix; index both forms so either reference form closes.
            $Leaf = ($P -split '/')[-1]
            if ($Leaf -match '\.' -and $P.EndsWith('.' + $Leaf)) {
                [void]$Paths.Add($P.Substring(0, $P.Length - $Leaf.Length - 1))
            }
        }
    }
    foreach ($Media in @($Allowlist.media_entries)) {
        if ($Media.object_path) { [void]$Paths.Add([string]$Media.object_path) }
    }
    return ,$Paths
}

function Test-IsEngineInternal([string]$Reference) {
    if (-not $Reference.StartsWith("/Engine/")) { return $false }
    foreach ($Prefix in $EngineInternalPrefixes) {
        if ($Reference.StartsWith($Prefix)) { return $true }
    }
    return $false
}

function Test-AllowlistMatch([string]$Reference, $AllowlistPaths) {
    if ($AllowlistPaths.Contains($Reference)) { return $true }
    $Dot = $Reference.LastIndexOf(".")
    if ($Dot -gt 0) {
        $Package = $Reference.Substring(0, $Dot)
        if ($AllowlistPaths.Contains($Package)) { return $true }
    }
    # Package-level reference (no .Object): match any allowlisted object under it.
    $Prefix = $Reference + "."
    foreach ($Entry in $AllowlistPaths) {
        if ($Entry.StartsWith($Prefix)) { return $true }
    }
    return $false
}

# Scan JSON catalogs for string references that look like /Game/ object paths.
$DataDir = Join-Path $ProjectRoot "Content/Data"
$Files = Get-ChildItem -LiteralPath $DataDir -Recurse -Filter *.json -File |
    Where-Object { $_.FullName -notmatch "\\Extracted\\" } |
    Sort-Object FullName
$AllowlistPaths = Get-AllowlistPaths $AllowlistPath

$Allowlisted = New-Object System.Collections.Generic.List[string]
$Unverified = New-Object System.Collections.Generic.List[string]
$EngineAllowed = New-Object System.Collections.Generic.List[string]
$EngineBlocked = New-Object System.Collections.Generic.List[string]
$ReferencePattern = '"/Game/[A-Za-z0-9_./-]+"'

foreach ($File in $Files) {
    $Text = Get-Content -LiteralPath $File.FullName -Raw
    foreach ($Match in [regex]::Matches($Text, $ReferencePattern)) {
        $Reference = $Match.Value.Trim('"')
        $Relative = $File.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/').Replace('\', '/')
        $Row = "$Relative|$Reference"
        if ($Reference.StartsWith("/Engine/")) {
            if (Test-IsEngineInternal $Reference) { $EngineAllowed.Add($Row) }
            else { $EngineBlocked.Add($Row) }
        }
        elseif (Test-AllowlistMatch $Reference $AllowlistPaths) { $Allowlisted.Add($Row) }
        else { $Unverified.Add($Row) }
    }
}

$Document = [ordered]@{
    schema_version = 1
    generated_by = "tools/check_static_cook_assets.ps1"
    allowlist_path = $AllowlistPath.Replace('\', '/')
    allowlist_entries = $AllowlistPaths.Count
    scanned_catalog_files = $Files.Count
    allowlisted_references = $Allowlisted.Count
    unverified_references = $Unverified.Count
    engine_allowed_references = $EngineAllowed.Count
    engine_blocked_references = $EngineBlocked.Count
    engine_internal_prefixes = $EngineInternalPrefixes
    unverified_sample = @($Unverified | Select-Object -First 15)
    engine_blocked_sample = @($EngineBlocked | Select-Object -First 15)
}

if (-not [string]::IsNullOrWhiteSpace($Output)) {
    $Parent = Split-Path -Parent $Output
    if ($Parent) { New-Item -ItemType Directory -Force -Path $Parent | Out-Null }
    $Json = $Document | ConvertTo-Json -Depth 6
    [System.IO.File]::WriteAllText($Output, $Json, [System.Text.UTF8Encoding]::new($false))
}

if ($Unverified.Count -gt 0 -or $EngineBlocked.Count -gt 0) {
    Write-Output ("STATIC_COOK_ASSET_AUDIT_FAIL refs={0} allowlisted={1} unverified={2} engine_allowed={3} engine_blocked={4} files={5}" -f
        ($Allowlisted.Count + $Unverified.Count + $EngineAllowed.Count + $EngineBlocked.Count),
        $Allowlisted.Count, $Unverified.Count, $EngineAllowed.Count, $EngineBlocked.Count, $Files.Count)
    exit 1
}
Write-Output ("STATIC_COOK_ASSET_AUDIT_PASS refs={0} allowlisted={1} engine_allowed={2} files={3}" -f
    ($Allowlisted.Count + $EngineAllowed.Count), $Allowlisted.Count, $EngineAllowed.Count, $Files.Count)
