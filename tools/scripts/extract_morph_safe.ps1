param(
  [Parameter(Mandatory=$true)][string]$ListFile,   # text file: one .upk full path (or bare package name) per line
  [Parameter(Mandatory=$true)][string]$OutRoot,    # staging dir root
  [Parameter(Mandatory=$true)][string]$PathRoot,   # umodel -path (packages root so imports resolve)
  [Parameter(Mandatory=$true)][string]$LogFile
)
# Morph-safe object-targeted extractor for APB packages whose whole-package export
# crashes in UMorphTargetSet::PostLoad on the runtime "_ReChunkified" mesh variant
# (TArray index out of range). Instead of LoadWholePackage, we enumerate objects via
# -list, DROP any *_ReChunkified mesh, and lazy-export the base SkeletalMesh/StaticMesh
# + all Texture2D by explicit -obj= names. Verified: recovers .psk + all .tga, EXIT=0.
$ErrorActionPreference = 'Continue'
$umodel = 'D:\APBReloaded\tools\UEViewer\umodel_64.exe'
$pkgs = Get-Content -LiteralPath $ListFile | Where-Object { $_.Trim() -ne '' }
New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
$total = $pkgs.Count; $i = 0; $ok = 0; $empty = 0
"START $(Get-Date -Format o)  MORPH-SAFE  total=$total  outRoot=$OutRoot" | Set-Content -LiteralPath $LogFile
foreach($p in $pkgs){
  $i++
  $name = [IO.Path]::GetFileNameWithoutExtension((Split-Path -Leaf $p))
  $dest = Join-Path $OutRoot $name
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  try {
    $list = & $umodel "-path=$PathRoot" -game=apb -list $name 2>&1
    $objArgs = @()
    foreach($line in $list){
      $s = $line.ToString().Trim()
      if($s -match '\s(SkeletalMesh|StaticMesh)\s'){
        $o = ($s -split '\s+')[-1]
        if($o -notmatch '_ReChunkified'){ $objArgs += "-obj=$o" }   # skip crash object
      } elseif($s -match '\sTexture2D\s'){
        $objArgs += "-obj=$(($s -split '\s+')[-1])"
      }
    }
    if($objArgs.Count -eq 0){
      $empty++; "[$i/$total] $name  NO-TARGETS (no mesh/tex objects)" | Add-Content -LiteralPath $LogFile
      continue
    }
    & $umodel "-path=$PathRoot" -game=apb @objArgs -export "-out=$dest" $name *>&1 |
      Where-Object { $_ -match 'ERROR|was not found' } | Out-Null
    $files = (Get-ChildItem $dest -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
    if($files -gt 0){ $ok++ } else { $empty++ }
    "[$i/$total] $name  files=$files  targets=$($objArgs.Count)" | Add-Content -LiteralPath $LogFile
  } catch {
    $empty++
    "[$i/$total] $name  EXCEPTION: $($_.Exception.Message)" | Add-Content -LiteralPath $LogFile
  }
}
"DONE $(Get-Date -Format o)  ok=$ok  empty=$empty  total=$total" | Add-Content -LiteralPath $LogFile
