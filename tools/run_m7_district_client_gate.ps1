param(
  [string]$Scratch = "$env:TEMP\apb_m7_district_client_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 75
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$worldLog = Join-Path $Scratch "world_relay.log"
$worldRestartLog = Join-Path $Scratch "world_relay_restart.log"
$districtLog = Join-Path $Scratch "district_relay.log"
$world = $null
$district = $null
$failure = $null

function Fail([string]$Reason) {
  throw [System.InvalidOperationException]::new($Reason)
}

function Stop-ProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" |
      ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if (-not $Process.HasExited) {
      Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
      $Process.WaitForExit(10000) | Out-Null
    }
  } catch {}
}

function Stop-AllGateProcesses {
  Stop-ProcessTree $district
  Stop-ProcessTree $world
  Get-Process -Name "UnrealEditor","CrashReportClientEditor" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "D:\UE58\UE_5.8\Engine\Binaries\Win64\*" } |
    Stop-Process -Force -ErrorAction SilentlyContinue
}

function Launch-World([string]$LogPath) {
  $frontendMap = "/Game/Maps/Lvl_APB_Frontend"
  $args = @(
    $Project, "$frontendMap`?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$LogPath"
  )
  return Start-Process -FilePath $Editor -ArgumentList $args -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

function Launch-District([pscustomobject]$District, [int]$DistrictPort) {
  $map = "/Game/Maps/$($District.map)"
  $args = @(
    $Project, "$map`?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    "-game", "-Port=$DistrictPort", "-RelayHost=127.0.0.1", "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$([int]$District.numeric_id)",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$districtLog"
  )
  return Start-Process -FilePath $Editor -ArgumentList $args -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

function Wait-Log([string]$Path, [string]$Pattern, [string]$FailureName) {
  $watch = [Diagnostics.Stopwatch]::StartNew()
  while ($watch.Elapsed.TotalSeconds -lt $TimeoutSec) {
    if (Test-Path $Path) {
      $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
      if ($content -match $Pattern) { return $content }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

try {
  if (-not (Test-Path -LiteralPath $Project)) { Fail "project_missing" }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail "editor_binary_missing" }

  $catalog = Get-Content -LiteralPath (Join-Path $projectRoot "Content\Data\districts.json") -Raw | ConvertFrom-Json
  $financial = $catalog | Where-Object { $_.id -eq "Financial" } | Select-Object -First 1
  if ($null -eq $financial) { Fail "financial_catalog_missing" }
  $districtPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financial.numeric_id)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

  $world = Launch-World $worldLog
  [void](Wait-Log $worldLog "RELAY_LISTEN port=$($ports.Relay)" "world_listen_timeout")

  $district = Launch-District $financial $districtPort
  [void](Wait-Log $districtLog "RELAY_CLIENT_REGISTERED district=$($financial.id) numeric_id=$([int]$financial.numeric_id)" "district_register_timeout")
  [void](Wait-Log $worldLog "RELAY_REGISTER district=$($financial.id) numeric_id=$([int]$financial.numeric_id) ok=1" "world_register_timeout")

  $heartbeatContent = Wait-Log $districtLog "RELAY_CLIENT_HEARTBEAT seq=2" "district_heartbeat_timeout"
  if (($heartbeatContent | Select-String -Pattern "RELAY_CLIENT_HEARTBEAT seq=" -AllMatches).Matches.Count -lt 2) {
    Fail "district_heartbeat_count"
  }

  Stop-ProcessTree $world
  $world = $null
  [void](Wait-Log $districtLog "RELAY_CLIENT_RECONNECT attempt=" "district_reconnect_timeout")

  $world = Launch-World $worldRestartLog
  [void](Wait-Log $worldRestartLog "RELAY_LISTEN port=$($ports.Relay)" "world_restart_listen_timeout")
  [void](Wait-Log $districtLog "RELAY_CLIENT_REGISTERED district=$($financial.id) numeric_id=$([int]$financial.numeric_id)" "district_reregister_timeout")
  [void](Wait-Log $worldRestartLog "RELAY_REGISTER district=$($financial.id) numeric_id=$([int]$financial.numeric_id) ok=1" "world_reregister_timeout")
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-AllGateProcesses
  Write-Host "===== world_relay.log ====="
  if (Test-Path $worldLog) { Get-Content $worldLog | Where-Object { $_ -match "RELAY_" } } else { Write-Host "(no world log written)" }
  Write-Host "===== district_relay.log ====="
  if (Test-Path $districtLog) { Get-Content $districtLog | Where-Object { $_ -match "RELAY_CLIENT_" } } else { Write-Host "(no district log written)" }
  Write-Host "===== world_relay_restart.log ====="
  if (Test-Path $worldRestartLog) { Get-Content $worldRestartLog | Where-Object { $_ -match "RELAY_" } } else { Write-Host "(no restart log written)" }
}

if (-not [string]::IsNullOrWhiteSpace($failure)) {
  Write-Host "RELAY_DISTRICT_CLIENT_GATE_FAIL $failure"
  exit 1
}

Write-Host "RELAY_DISTRICT_CLIENT_GATE_OK"
exit 0
