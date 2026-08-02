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
$MediaPattern = '\b(?:OpenFile|SetFilePath|OpenSource|OpenUrl|APB_LoadPcmWavProcedural)\s*\('
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
$MediaRows = [System.Collections.Generic.List[object]]::new()
foreach ($File in $Files) {
    $Lines = Get-Content -LiteralPath $File.FullName
    for ($Index = 0; $Index -lt $Lines.Count; $Index++) {
        $Line = [string]$Lines[$Index]
        if ($Line -match '^\s*#include') { continue }
        foreach ($Match in [regex]::Matches($Line, $MediaPattern)) {
            $Relative = $File.FullName.Substring($ProjectRoot.Length).TrimStart('\', '/')
            $MediaRows.Add([ordered]@{
                path = $Relative.Replace('\', '/')
                line = $Index + 1
                token = $Match.Value
                text = $Line.Trim()
            })
        }
    }
}
$AllowedMediaPaths = @(
    'Source/APBReloaded/Systems/Frontend/APBFrontendWidget.cpp'
)
$UnroutedMediaRows = @($MediaRows | Where-Object { $AllowedMediaPaths -notcontains $_.path })
$UnroutedRows = @($Rows | Where-Object { $AllowedPaths -notcontains $_.path })
$PlacementText = Get-Content -LiteralPath $PlacementPath -Raw
$RegistryText = Get-Content -LiteralPath $RegistryPath -Raw
$RegistryOwnerOk = $RegistryText -match '\bLoadObject<[^>]+>\s*\(' -and $RegistryText -match 'UAPBVerifiedAssetRegistry::LoadStaticMesh'
$PlacementRouteOk = $PlacementText -match 'UAPBVerifiedDistrictAssetRouting::RoutePlacementMesh' -and $PlacementText -match 'APBVerifiedDistrictAssetRouting.h'
$WidgetPath = Join-Path $SourceRoot "Systems/Frontend/APBFrontendWidget.cpp"
$WidgetText = Get-Content -LiteralPath $WidgetPath -Raw
$FrontendMediaRouteOk = $WidgetText -match 'UAPBVerifiedAssetRegistry' -and $WidgetText -match 'IsMediaAllowed' -and $WidgetText -match 'VerifyMediaFile'
if (-not $RegistryOwnerOk) { throw 'VERIFIED_ASSET_STATIC_AUDIT_FAIL reason=registry_owner_missing' }
if (-not $PlacementRouteOk) { throw 'VERIFIED_ASSET_STATIC_AUDIT_FAIL reason=placement_registry_route_missing' }
if (-not $FrontendMediaRouteOk) { throw 'VERIFIED_ASSET_STATIC_AUDIT_FAIL reason=frontend_media_route_missing' }

$Document = [ordered]@{
    schema_version = 1
    generated_by = 'tools/check_verified_asset_static_audit.ps1'
    source_root = 'Source/APBReloaded'
    load_tokens = @('LoadObject', 'StaticLoadObject', 'FObjectFinder', 'SoftObjectPath', 'TSoftObjectPtr', 'ConstructorHelpers')
    allowed_paths = $AllowedPaths
    registry_owner = $true
    placement_registry_route = $true
    frontend_media_route = $true
    routing_layer = 'UAPBVerifiedDistrictAssetRouting'
    unrouted_load_count = $UnroutedRows.Count
    media_unrouted_count = $UnroutedMediaRows.Count
    loads = @($Rows)
    media_loads = @($MediaRows)
}
if (-not [string]::IsNullOrWhiteSpace($Output)) {
    $Parent = Split-Path -Parent $Output
    if ($Parent) { New-Item -ItemType Directory -Force -Path $Parent | Out-Null }
    $Json = $Document | ConvertTo-Json -Depth 10
    [System.IO.File]::WriteAllText($Output, $Json, [System.Text.UTF8Encoding]::new($false))
}
if ($UnroutedRows.Count -gt 0 -or $UnroutedMediaRows.Count -gt 0) {
    $Paths = @($UnroutedRows | ForEach-Object { $_.path } | Sort-Object -Unique) -join ','
    $MediaPaths = @($UnroutedMediaRows | ForEach-Object { $_.path } | Sort-Object -Unique) -join ','
    Write-Output "VERIFIED_ASSET_STATIC_AUDIT_FAIL loads=$($Rows.Count) unrouted=$($UnroutedRows.Count) media_unrouted=$($UnroutedMediaRows.Count) paths=$Paths media_paths=$MediaPaths"
    exit 1
}
Write-Output "VERIFIED_ASSET_STATIC_AUDIT_PASS loads=$($Rows.Count) media_loads=$($MediaRows.Count) files=$($Files.Count) registry_owner=1 placement_registry_route=1 frontend_media_route=1"
