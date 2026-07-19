# Export a 2011 .apb level's mesh packages and open the model viewer.
# Usage:
#   .\view_apb_level.ps1 FinancialDistrict_Block09
#   .\view_apb_level.ps1 -List
#   .\view_apb_level.ps1 -UmodelGui FinancialDistrict_Block09   # umodel native 3D viewer

param(
  [Parameter(Position=0)][string]$Level = "FinancialDistrict_Block09",
  [switch]$List,
  [switch]$UmodelGui,
  [switch]$SkipExport
)

$Root = "D:\APBReloaded"
$Umodel = Join-Path $Root "tools\UEViewer\umodel_64.exe"
$Content = Join-Path $Root "2011 apb\APB All Points Bulletin\APB North America\APBGame\Content"
$OutRoot = Join-Path $Root "Content\Extracted\2011\Levels"

if ($List) {
  python (Join-Path $Root "tools\scripts\export_apb_level.py") --list
  exit $LASTEXITCODE
}

if ($UmodelGui) {
  # Native umodel mesh viewer (best for one package). Resolve related building UPK.
  Write-Host "Opening umodel GUI against Content path (pick StaticMesh in browser)..."
  Write-Host "Tip: for district blocks, open the *_Package.UPK under District\Packages\Buildings"
  Start-Process -FilePath $Umodel -ArgumentList @(
    "-path=$Content",
    "-game=apb",
    $Level
  )
  exit 0
}

if (-not $SkipExport) {
  python (Join-Path $Root "tools\scripts\export_apb_level.py") $Level
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$out = Join-Path $OutRoot $Level
if (-not (Test-Path $out)) {
  # try stem without path
  $cand = Get-ChildItem $OutRoot -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "*$Level*" } | Select-Object -First 1
  if ($cand) { $out = $cand.FullName }
}

Write-Host "Launching model viewer on $out"
Set-Location (Join-Path $Root "tools\model_viewer")
python view_models.py --root $out
