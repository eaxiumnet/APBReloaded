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
  [string]$OutDir = "",
  [string]$RegistryPath = ""
)

$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$RegistryPath = if ([string]::IsNullOrWhiteSpace($RegistryPath)) {
  Join-Path $ProjectRoot "tools\source_registry.json"
} else {
  [System.IO.Path]::GetFullPath($RegistryPath)
}
$Resolver = Join-Path $PSScriptRoot "resolve_source_root.ps1"
$Registry = Get-Content -LiteralPath $RegistryPath -Raw | ConvertFrom-Json -Depth 32
$RetailRoot = & $Resolver -Alias retail_steam -Preflight -RegistryPath $RegistryPath
if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) {
  exit $LASTEXITCODE
}
$Umodel = [string]$Registry.readers.umodel.path
$Identity = $Registry.readers.umodel.identity
$Capability = $Registry.readers.umodel.capability_probe
$DefaultPkgRoot = Join-Path $RetailRoot ([string]$Registry.roots.retail_steam.packages_subpath)
$Path = if ($PathOverride) { $PathOverride } else { $DefaultPkgRoot }
$OutDir = if ([string]::IsNullOrWhiteSpace($OutDir)) {
  Join-Path $ProjectRoot "Content\Extracted\UmodelExport"
} else {
  [System.IO.Path]::GetFullPath($OutDir)
}

if (-not (Test-Path -LiteralPath $Umodel -PathType Leaf)) {
  Write-Output "UMODEL_READER_FAIL reason=reader_missing path=$Umodel"
  exit 1
}
if ([System.IO.Path]::GetFileName($Umodel) -cne [string]$Identity.file_name) {
  Write-Output "UMODEL_READER_FAIL reason=reader_file_name expected=$($Identity.file_name) actual=$Umodel"
  exit 1
}
if ((Get-FileHash -LiteralPath $Umodel -Algorithm SHA256).Hash.ToLowerInvariant() -ne [string]$Identity.sha256) {
  Write-Output "UMODEL_READER_FAIL reason=reader_sha256_mismatch"
  exit 1
}
if ((Get-Item -LiteralPath $Umodel).Length -ne [long]$Identity.bytes) {
  Write-Output "UMODEL_READER_FAIL reason=reader_size_mismatch"
  exit 1
}
Write-Host "UMODEL_READER_IDENTITY_PASS path=$Umodel"

$ProbeRelative = [string]$Capability.package
$ProbePath = Join-Path $DefaultPkgRoot $ProbeRelative
if (-not (Test-Path -LiteralPath $ProbePath -PathType Leaf)) {
  Write-Output "UMODEL_READER_FAIL reason=probe_package_missing package=$ProbeRelative"
  exit 1
}

Write-Host "UMODEL_READER=$Umodel"
Write-Host "UMODEL_PROBE_PACKAGE=$ProbeRelative"
$ProbeOutput = & $Umodel "-path=$DefaultPkgRoot" "-game=$($Capability.game_tag)" "-list" $ProbeRelative 2>&1
$ProbeCode = $LASTEXITCODE
if ($ProbeCode -ne 0 -or -not ($ProbeOutput -match [string]$Capability.list_pattern)) {
  Write-Output "UMODEL_READER_FAIL reason=capability_probe_failed exit_code=$ProbeCode"
  $ProbeOutput | Write-Output
  exit 1
}
Write-Host "UMODEL_CAPABILITY_PASS package=$ProbeRelative"

if (-not $ListOnly) {
  New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
}

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
  Write-Host "APB-patched UEViewer is required; rebuild the patched fork before retrying."
  Write-Host "NOTE: APB FileVersion 564/33 cooked packages often fail stock umodel decompress."
  Write-Host "Use package_inventory.csv name_hints + model_reference_catalog.json for referencing."
}
exit $code
