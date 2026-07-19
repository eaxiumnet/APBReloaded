# Live dump of running APB Reloaded (owned Steam install) for private offline recreation.
# Captures: process metadata, modules, window titles, open package paths from cmd line,
# optional minidump (if procdump/cdb available), and memory-string samples for known keys.
#
# Usage (admin recommended for full module list):
#   powershell -ExecutionPolicy Bypass -File tools\scripts\live_dump_apb.ps1
#   powershell -File tools\scripts\live_dump_apb.ps1 -OutDir C:\temp\apb_live

param(
  [string]$OutDir = "C:\Users\Support\AppData\Local\Temp\grok-goal-9ca60165ac93\implementer\live_dump",
  [switch]$Minidump,
  [switch]$WaitForProcess,
  [int]$WaitSeconds = 120
)

$ErrorActionPreference = "Continue"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$ts = Get-Date -Format "yyyyMMdd_HHmmss"
$log = Join-Path $OutDir "live_dump_$ts.log"

function L($m) {
  $line = "[{0}] {1}" -f (Get-Date -Format "o"), $m
  Add-Content -Path $log -Value $line
  Write-Host $line
}

function Find-ApbProcess {
  $names = @("APB", "APB_EAC", "APB_Catcher", "APBLauncher")
  foreach ($n in $names) {
    $p = Get-Process -Name $n -ErrorAction SilentlyContinue
    if ($p) { return $p | Select-Object -First 1 }
  }
  # Path-based
  $cim = Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object { $_.ExecutablePath -and ($_.ExecutablePath -match 'APB Reloaded\\Binaries\\APB\.exe|\\APB\.exe$') }
  if ($cim) {
    return Get-Process -Id $cim[0].ProcessId -ErrorAction SilentlyContinue
  }
  return $null
}

L "LIVE_DUMP_START out=$OutDir"

$proc = Find-ApbProcess
if (-not $proc -and $WaitForProcess) {
  L "Waiting up to ${WaitSeconds}s for APB.exe..."
  $sw = [Diagnostics.Stopwatch]::StartNew()
  while (-not $proc -and $sw.Elapsed.TotalSeconds -lt $WaitSeconds) {
    Start-Sleep 2
    $proc = Find-ApbProcess
  }
}

if (-not $proc) {
  L "FAIL: APB.exe not running in this session"
  L "Checked: APB, APB_EAC, APB_Catcher, APBLauncher, path match APB Reloaded\Binaries\APB.exe"
  Get-Process | Where-Object { $_.MainWindowHandle -ne 0 } |
    Select-Object Id, ProcessName, MainWindowTitle |
    Export-Csv (Join-Path $OutDir "other_windows_$ts.csv") -NoTypeInformation
  L "Wrote other_windows_$ts.csv for diagnostics"
  exit 2
}

$procId = $proc.Id
L "FOUND process Id=$procId Name=$($proc.ProcessName) WS=$([int]($proc.WorkingSet64/1MB))MB Title=$($proc.MainWindowTitle)"

# WMI details
$wmi = Get-CimInstance Win32_Process -Filter "ProcessId=$procId" -ErrorAction SilentlyContinue
$meta = [ordered]@{
  time = (Get-Date).ToString("o")
  process_id = $procId
  process_name = $proc.ProcessName
  path = $proc.Path
  start_time = $proc.StartTime
  working_set_mb = [math]::Round($proc.WorkingSet64 / 1MB, 1)
  peak_ws_mb = [math]::Round($proc.PeakWorkingSet64 / 1MB, 1)
  main_window = $proc.MainWindowTitle
  command_line = $wmi.CommandLine
  parent_pid = $wmi.ParentProcessId
  executable_path = $wmi.ExecutablePath
}
$meta | ConvertTo-Json | Set-Content (Join-Path $OutDir "process_meta_$ts.json") -Encoding UTF8
L "Wrote process_meta_$ts.json"

# Modules
try {
  $mods = $proc.Modules | Select-Object ModuleName, FileName, Size, FileVersionInfo
  $mods | Export-Csv (Join-Path $OutDir "modules_$ts.csv") -NoTypeInformation
  L "Modules count=$($mods.Count)"
  $mods | Select-Object -First 40 ModuleName, FileName | Format-Table | Out-String | ForEach-Object { L $_.TrimEnd() }
} catch {
  L "Modules list failed (need elevation?): $_"
}

# Related APB processes
Get-CimInstance Win32_Process | Where-Object {
  $_.ExecutablePath -and $_.ExecutablePath -match 'APB Reloaded'
} | Select-Object ProcessId, Name, ExecutablePath, CommandLine |
  ConvertTo-Json | Set-Content (Join-Path $OutDir "related_processes_$ts.json") -Encoding UTF8

# Binaries folder (WMI path often null under EAC — fall back to Steam install)
$binDir = $null
if ($wmi.ExecutablePath) { $binDir = Split-Path $wmi.ExecutablePath }
if (-not $binDir -and $proc.Path) { $binDir = Split-Path $proc.Path }
if (-not $binDir) {
  $fallback = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries"
  if (Test-Path $fallback) { $binDir = $fallback }
}
if ($binDir -and (Test-Path $binDir)) {
  Get-ChildItem $binDir -File | Select-Object Name, Length, LastWriteTime |
    Export-Csv (Join-Path $OutDir "binaries_dir_$ts.csv") -NoTypeInformation
  L "binDir=$binDir"
}

# Network connections (district server endpoints)
try {
  Get-NetTCPConnection -OwningProcess $procId -ErrorAction SilentlyContinue |
    Select-Object LocalAddress, LocalPort, RemoteAddress, RemotePort, State |
    Export-Csv (Join-Path $OutDir "tcp_$ts.csv") -NoTypeInformation
  L "TCP connections captured"
} catch {
  L "TCP capture failed: $_"
}

# Optional minidump
if ($Minidump) {
  $dumpPath = Join-Path $OutDir ("APB_{0}_{1}.dmp" -f $procId, $ts)
  $procdump = Get-Command procdump.exe -ErrorAction SilentlyContinue
  $cdb = @(
    "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe",
    "${env:ProgramFiles}\Windows Kits\10\Debuggers\x64\cdb.exe"
  ) | Where-Object { Test-Path $_ } | Select-Object -First 1

  if ($procdump) {
    L "Minidump via procdump -> $dumpPath"
    & procdump.exe -accepteula -ma $procId $dumpPath 2>&1 | Tee-Object -FilePath (Join-Path $OutDir "procdump_$ts.log")
  } elseif ($cdb) {
    L "Minidump via cdb -> $dumpPath"
    $cmd = ".dump /ma `"$dumpPath`"; q"
    & $cdb -p $procId -c $cmd 2>&1 | Tee-Object -FilePath (Join-Path $OutDir "cdb_$ts.log")
  } else {
    L "No procdump/cdb found; skip minidump (install Sysinternals procdump or WinDbg SDK)"
  }
}

# String scan of loaded APB.exe file on disk (not full process memory) for district/weapon tokens
$exePath = $wmi.ExecutablePath
if ($exePath -and (Test-Path $exePath)) {
  L "Scanning on-disk APB.exe for tokens (not full RAM dump)"
  $bytes = [IO.File]::ReadAllBytes($exePath)
  $text = [Text.Encoding]::ASCII.GetString($bytes)
  $keys = @(
    "Financial", "Waterfront", "Notoriety", "Prestige", "Armas", "Auction",
    "Weapon_", "Vehicle_", "District", "Opposition", "G1C", "APBDB",
    "cStreamedBuildingActor", "cStreamedComponentSet"
  )
  $hits = @{}
  foreach ($k in $keys) {
    $c = ([regex]::Matches($text, [regex]::Escape($k))).Count
    $hits[$k] = $c
  }
  $hits | ConvertTo-Json | Set-Content (Join-Path $OutDir "exe_string_hits_$ts.json") -Encoding UTF8
  L "Wrote exe_string_hits_$ts.json"
}

L "LIVE_DUMP_COMPLETE"
Write-Host "Artifacts under $OutDir"
exit 0
