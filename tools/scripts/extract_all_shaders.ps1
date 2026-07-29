# Extract all APB shader/world material packages for UE5.8 recreation analysis.
# Uses UEViewer (umodel_64.exe) to dump .mat material graphs and textures.

param(
    [string]$SourceRoot = 'D:\APBReloaded\2011 apb\APB All Points Bulletin\APB North America',
    [string]$OutRoot = 'D:\APBReloaded\work\extracted-shaders\2011-full'
)

$umodel = 'D:\APBReloaded\tools\UEViewer\umodel_64.exe'
$ErrorActionPreference = 'Continue'

# Shader-related package patterns (file names)
$patterns = @(
    'Engine_MI_Shaders.upk',
    'RefShaderCache-PC-D3D-SM3.upkf',
    'RoadEditorMaterials*.upk',
    'TerrainMaterials*.upk',
    'PosterDecals*.upk',
    'RoadDecals*.upk',
    'StatueMaterials.upk',
    'HUDMaterials.upk',
    'ReflectionMap-HeightmapShader.upk',
    'VFX_*.upk',
    'vfx_*.upk'
)

New-Item -ItemType Directory -Path $OutRoot -Force | Out-Null

# Find matching packages
$packages = @()
foreach ($pat in $patterns) {
    $packages += Get-ChildItem -Path $SourceRoot -Recurse -Filter $pat -File | Select-Object -ExpandProperty FullName
}
$packages = $packages | Sort-Object -Unique

Write-Host "Found $($packages.Count) shader/material packages to extract." -ForegroundColor Green

foreach ($pkg in $packages) {
    $rel = $pkg.Substring($SourceRoot.Length).TrimStart('\')
    $out = Join-Path $OutRoot ($rel -replace '\.upk$', '')
    New-Item -ItemType Directory -Path $out -Force | Out-Null
    Write-Host "Extracting: $rel"
    & $umodel -path=$SourceRoot -export "$pkg" -out=$out 2>&1 | Select-Object -Last 5
}

Write-Host "Extraction complete. Output: $OutRoot" -ForegroundColor Green
