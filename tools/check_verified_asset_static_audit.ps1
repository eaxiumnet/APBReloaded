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
$SourceRoot = Join-Path $ProjectRoot "Source/APBReloaded"
$RegistryPath = Join-Path $SourceRoot "Systems/APBVerifiedAssetRegistry.cpp"
$PlacementPath = Join-Path $SourceRoot "Systems/District/APBDistrictPlacementLoader.cpp"
$Pattern = '\b(?:LoadObject|StaticLoadObject|FObjectFinder|SoftObjectPath|TSoftObjectPtr|ConstructorHelpers)\b'
$Files = Get-ChildItem -LiteralPath $SourceRoot -Recurse -File |
    Where-Object { $_.Extension -in @('.cpp', '.h') } |
    Sort-Object FullName
$Rows = [System.Collections.Generic.List[object]]::new()

foreach ($File in $Files) {
    $Lines = Get-Content -LiteralPath $File.FullName
    for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
        $Line = [string]$Lines[$Index]
        if ($Line -match '^\s*#include') { continue }
        foreach ($Match in [regex]::Matches($Line, $Pattern)) {
            $Relative = $File.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/')
            $Rows.Add([ordered]@{
                path = $Relative.Replace('\', '/')
                line = $Index + 1
                token = $Match.Value
                text = $Line.Trim()
            })
        }
    }
}

$AllowedPaths = @(
    'Source/APBReloaded/Systems/APBVerifiedAssetRegistry.cpp'
)
$UnroutedRows = @($Rows | Where-Object { $AllowedPaths -notcontains $_.path })
$PlacementText = Get-Content -LiteralPath $PlacementPath -Raw
$RegistryText = Get-Content -LiteralPath $RegistryPath -Raw
$RegistryOwnerOk = $RegistryText -match '\bLoadObject<[^>]+>\s*\(' -and $RegistryText -match 'UAPBVerifiedAssetRegistry::LoadStaticMesh'
$PlacementRouteOk = $PlacementText -match 'GetSubsystem<UAPBVerifiedAssetRegistry>' -and $PlacementText -match 'Registry->LoadStaticMesh'
if (-not $RegistryOwnerOk) { throw 'VERIFIED_ASSET_STATIC_AUDIT_FAIL reason=registry_owner_missing' }
if (-not $PlacementRouteOk) { throw 'VERIFIED_ASSET_STATIC_AUDIT_FAIL reason=placement_registry_route_missing' }

$Document = [ordered]@{
    schema_version = 1
    generated_by = 'tools/check_verified_asset_static_audit.ps1'
    source_root = 'Source/APBReloaded'
    load_tokens = @('LoadObject', 'StaticLoadObject', 'FObjectFinder', 'SoftObjectPath', 'TSoftObjectPtr', 'ConstructorHelpers')
    allowed_paths = $AllowedPaths
    registry_owner = $true
    placement_registry_route = $true
    unrouted_load_count = $UnroutedRows.Count
    loads = @($Rows)
}
if (-not [string]::IsNullOrWhiteSpace($Output)) {
    $Parent = Split-Path -Parent $Output
    if ($Parent) { New-Item -ItemType Directory -Force -Path $Parent | Out-Null }
    $Json = $Document | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($Output, $Json, [System.Text.UTF8Encoding]::new($false))
}
if ($UnroutedRows.Count -gt 0) {
    $Paths = @($UnroutedRows | ForEach-Object { $_.path } | Sort-Object -Unique) -join ','
    Write-Output "VERIFIED_ASSET_STATIC_AUDIT_FAIL loads=$($Rows.Count) unrouted=$($UnroutedRows.Count) paths=$Paths"
    exit 1
}
Write-Output "VERIFIED_ASSET_STATIC_AUDIT_PASS loads=$($Rows.Count) files=$($Files.Count) registry_owner=1 placement_registry_route=1"
