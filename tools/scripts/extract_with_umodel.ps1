# APB package extraction via Gildor UE Viewer (umodel)
# Usage:
#   .\extract_with_umodel.ps1 -Package Contact_LaRocha
#   .\extract_with_umodel.ps1 -Package V_A_2DrCoupe_Wheels_7 -MeshesOnly
#   .\extract_with_umodel.ps1 -ListOnly -Package EngineMaterials -PathOverride "C:\...\Engine"

param(
  [Parameter(Mandatory=$true)][string]$Package,
  [switch]$ListOnly,
  [switch]$MeshesOnly,
  [string]$GameTag = "apb",
  [string]$PathOverride = "",
  [string]$OutDir = "D:\APBReloaded\Content\Extracted\UmodelExport"
)

$Umodel = "D:\APBReloaded\Tools\UEViewer\umodel.exe"
$DefaultPkgRoot = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages"
$Path = if ($PathOverride) { $PathOverride } else { $DefaultPkgRoot }

if (-not (Test-Path $Umodel)) { throw "umodel not found at $Umodel" }
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$args = @("-path=$Path", "-game=$GameTag")
if ($ListOnly) {
  $args += @("-list", $Package)
} else {
  $args += @("-export", "-out=$OutDir")
  if ($MeshesOnly) { $args += "-meshes" }
  $args += $Package
}

Write-Host "RUN: $Umodel $($args -join ' ')"
& $Umodel @args
$code = $LASTEXITCODE
Write-Host "EXIT=$code"
if ($code -ne 0) {
  Write-Host "NOTE: APB FileVersion 564/33 cooked packages often fail stock umodel decompress."
  Write-Host "Use package_inventory.csv name_hints + model_reference_catalog.json for referencing."
  Write-Host "Try updating umodel or building UEViewer from source with APB fixes."
}
exit $code
