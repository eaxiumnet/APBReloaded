param(
  [Parameter(Mandatory=$true)][string]$ListFile,   # text file: one .upk full path per line
  [Parameter(Mandatory=$true)][string]$OutRoot,    # staging dir root
  [Parameter(Mandatory=$true)][string]$PathRoot,   # umodel -path (packages root so imports resolve)
  [Parameter(Mandatory=$true)][string]$LogFile
)
$ErrorActionPreference = 'Continue'
$umodel = 'D:\APBReloaded\tools\UEViewer\umodel_64.exe'
$pkgs = Get-Content -LiteralPath $ListFile | Where-Object { $_.Trim() -ne '' }
New-Item -ItemType Directory -Force -Path $OutRoot | Out-Null
$total = $pkgs.Count; $i = 0; $ok = 0; $fail = 0
"START $(Get-Date -Format o)  total=$total  outRoot=$OutRoot" | Set-Content -LiteralPath $LogFile
foreach($p in $pkgs){
  $i++
  $name = Split-Path -Leaf $p
  $dest = Join-Path $OutRoot ([IO.Path]::GetFileNameWithoutExtension($name))
  New-Item -ItemType Directory -Force -Path $dest | Out-Null
  try {
    & $umodel "-path=$PathRoot" -game=apb -export "-out=$dest" $name *>&1 |
      Where-Object { $_ -match 'Exported|ERROR|was not found' } | Out-Null
    $tga = (Get-ChildItem $dest -Recurse -File -ErrorAction SilentlyContinue | Measure-Object).Count
    if($tga -gt 0){ $ok++ } else { $fail++ }
    "[$i/$total] $name  files=$tga" | Add-Content -LiteralPath $LogFile
  } catch {
    $fail++
    "[$i/$total] $name  EXCEPTION: $($_.Exception.Message)" | Add-Content -LiteralPath $LogFile
  }
}
"DONE $(Get-Date -Format o)  ok=$ok  empty/fail=$fail  total=$total" | Add-Content -LiteralPath $LogFile
