# Bulk umodel LOD0 static mesh export for secondary San Paro districts
param(
  [string]$Umodel = "D:\APBReloaded\tools\UEViewer\umodel_64.exe",
  [string]$PkgRoot = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Content\Release\Packages",
  [string]$OutRoot = "D:\APBReloaded\Content\Extracted\UmodelExport\SecondaryBulk",
  [string]$Log = "C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\umodel_secondary.log"
)

$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
"" | Set-Content $Log

$districts = @(
  @{ Name = "Asylum"; Dir = "AsylumDistrict"; Out = "AsylumBulk" },
  @{ Name = "Beacon"; Dir = "PGBeaconDistrict"; Out = "BeaconBulk" },
  @{ Name = "Crate"; Dir = "PGCrateDistrict"; Out = "CrateBulk" },
  @{ Name = "Social"; Dir = "RWorldSocialDistrict"; Out = "SocialBulk" }
)

foreach ($d in $districts) {
  $src = Join-Path $PkgRoot $d.Dir
  $dest = Join-Path $OutRoot $d.Out
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  $upks = Get-ChildItem $src -Recurse -Filter "*_Package.UPK" -ErrorAction SilentlyContinue
  "=== $($d.Name) packages=$($upks.Count) ===" | Tee-Object -FilePath $Log -Append
  foreach ($u in $upks) {
    $line = "EXPORT $($u.FullName)"
    Add-Content $Log $line
    Write-Host $line
    # -export -meshes exports static meshes; game=apb; path for package deps
    & $Umodel -game=apb -path="$PkgRoot" -path="$src" -export -meshes -out="$dest" "$($u.FullName)" 2>&1 |
      Tee-Object -FilePath $Log -Append | Out-Null
  }
  $pskx = (Get-ChildItem $dest -Recurse -Filter "*.pskx" -ErrorAction SilentlyContinue).Count
  "=== $($d.Name) done pskx=$pskx ===" | Tee-Object -FilePath $Log -Append
}

"ALL_SECONDARY_DONE" | Tee-Object -FilePath $Log -Append
