param(
  [string]$Scratch = "$env:TEMP\apb_m7_handoff_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 90
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
$world = $null
$district = $null
$client = $null
# Pre-initialize log-path vars so the StrictMode finally block can reference them
# even when the try body throws before they are assigned (avoids masking HANDOFF_GATE_FAIL).
$happyWorldLog = $null
$happyDistrictLog = $null
$happyProbeLog = $null
$tamperWorldLog = $null
$tamperDistrictLog = $null
$tamperProbeLog = $null

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

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

function Stop-GateProcesses {
  Stop-ProcessTree $client
  Stop-ProcessTree $district
  Stop-ProcessTree $world
  $client = $null
  $district = $null
  $world = $null
  Get-Process -Name "UnrealEditor","CrashReportClientEditor" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "D:\UE58\UE_5.8\Engine\Binaries\Win64\*" } |
    Stop-Process -Force -ErrorAction SilentlyContinue
}

function Assert-PortsFree([int]$DistrictPort) {
  $tcp = @(Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object {
    $_.LocalPort -in @($ports.Relay)
  })
  $udp = @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object {
    $_.LocalPort -in @($ports.World, $DistrictPort)
  })
  if ($tcp.Count -ne 0 -or $udp.Count -ne 0) { Fail "ports_not_free" }
}

function Start-Editor([string[]]$Arguments) {
  Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
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

function Start-World([string]$LogPath, [bool]$Tamper) {
  $arguments = @(
    $Project, "/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-APBProbe=world_handoff_server", "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$LogPath"
  )
  if ($Tamper) { $arguments += "-APBRelayTamperCharacter=Operative" }
  Start-Editor $arguments
}

function Start-District([pscustomobject]$DistrictInfo, [int]$DistrictPort, [string]$LogPath) {
  Start-Editor @(
    $Project, "/Game/Maps/$($DistrictInfo.map)`?listen?MaxPlayers=$([int]$DistrictInfo.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    "-game", "-APBProbe=world_handoff_district", "-Port=$DistrictPort", "-WorldPort=$($ports.World)",
    "-RelayHost=127.0.0.1", "-RelayPort=$($ports.Relay)", "-DistrictId=$($DistrictInfo.id)", "-NumericId=$([int]$DistrictInfo.numeric_id)", "-RequireTicket",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$LogPath"
  )
}

function Start-HandoffClient([string]$Id, [string]$EngineLog) {
  Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", "-game", "-WorldServerHost=127.0.0.1", "-APBProbe=world_handoff_client", "-WSClientId=$Id",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$EngineLog", "-APBScratch=$Scratch"
  )
}

function Show-Log([string]$Title, [string]$Path, [string]$Pattern) {
  Write-Host "===== $Title ====="
  if (Test-Path $Path) { Get-Content $Path | Where-Object { $_ -match $Pattern } }
  else { Write-Host "(no log written)" }
}

try {
  if (-not (Test-Path $Project)) { Fail "project_missing" }
  if (-not (Test-Path $Editor)) { Fail "editor_missing" }
  $catalog = Get-Content (Join-Path $projectRoot "Content\Data\districts.json") -Raw | ConvertFrom-Json
  $financial = $catalog | Where-Object { $_.id -eq "Financial" } | Select-Object -First 1
  if ($null -eq $financial) { Fail "catalog_missing_financial" }
  $financialPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financial.numeric_id)
  Stop-GateProcesses
  Assert-PortsFree $financialPort
  Remove-Item $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

  $happyWorldLog = Join-Path $Scratch "world_happy.log"
  $happyDistrictLog = Join-Path $Scratch "district_happy.log"
  $happyEngineLog = Join-Path $Scratch "handoff_happy_engine.log"
  $happyProbeLog = Join-Path $Scratch "world_handoff_client_happy.log"
  $world = Start-World $happyWorldLog $false
  [void](Wait-Log $happyWorldLog "RELAY_LISTEN port=$($ports.Relay)" "happy_world_listener_timeout")
  $district = Start-District $financial $financialPort $happyDistrictLog
  [void](Wait-Log $happyWorldLog "RELAY_REGISTER district=Financial numeric_id=$([int]$financial.numeric_id) ok=1" "happy_district_register_timeout")
  $client = Start-HandoffClient "happy" $happyEngineLog
  [void](Wait-Log $happyProbeLog "HANDOFF_PRE cash=12345 g1c=4321 threat=42.5 faction=Enforcer inv_slots=2 inv_qty=5" "happy_pre_timeout")
  [void](Wait-Log $happyDistrictLog "CHAR_HANDOFF_APPLIED account=ACC-handoff_happy faction=Enforcer cash=12345 threat=42.5" "happy_apply_timeout")
  [void](Wait-Log $happyProbeLog "HANDOFF_DISTRICT_PARITY ok=1" "happy_district_parity_timeout")
  [void](Wait-Log $happyDistrictLog "CHAR_RETURN_SENT account=ACC-handoff_happy" "happy_return_sent_timeout")
  [void](Wait-Log $happyWorldLog "CHAR_RETURN_APPLIED account=ACC-handoff_happy cash=12422 threat=47.5" "happy_return_apply_timeout")
  [void](Wait-Log $happyProbeLog "WORLD_HANDOFF_CLIENT_OK" "happy_client_verdict_timeout")
  Stop-GateProcesses
  Assert-PortsFree $financialPort

  $tamperWorldLog = Join-Path $Scratch "world_tamper.log"
  $tamperDistrictLog = Join-Path $Scratch "district_tamper.log"
  $tamperEngineLog = Join-Path $Scratch "handoff_tamper_engine.log"
  $tamperProbeLog = Join-Path $Scratch "world_handoff_client_tamper.log"
  $world = Start-World $tamperWorldLog $true
  [void](Wait-Log $tamperWorldLog "RELAY_LISTEN port=$($ports.Relay)" "tamper_world_listener_timeout")
  $district = Start-District $financial $financialPort $tamperDistrictLog
  [void](Wait-Log $tamperWorldLog "RELAY_REGISTER district=Financial numeric_id=$([int]$financial.numeric_id) ok=1" "tamper_district_register_timeout")
  $client = Start-HandoffClient "tamper" $tamperEngineLog
  [void](Wait-Log $tamperDistrictLog "CHAR_HANDOFF_REJECT reason=account_mismatch" "tamper_reject_timeout")
  Stop-GateProcesses
  Assert-PortsFree $financialPort
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-GateProcesses
  Write-Host "===== happy world ====="
  if ($happyWorldLog -and (Test-Path $happyWorldLog)) { Get-Content $happyWorldLog | Where-Object { $_ -match "CHAR_HANDOFF|CHAR_RETURN|RELAY_|TRAVEL_" } }
  Write-Host "===== happy district ====="
  if ($happyDistrictLog -and (Test-Path $happyDistrictLog)) { Get-Content $happyDistrictLog | Where-Object { $_ -match "CHAR_HANDOFF|CHAR_RETURN|DISTRICT_TICKET_|RELAY_CLIENT_" } }
  Write-Host "===== happy client ====="
  if ($happyProbeLog -and (Test-Path $happyProbeLog)) { Get-Content $happyProbeLog | Where-Object { $_ -match "HANDOFF_|WORLD_HANDOFF" } }
  Write-Host "===== tamper world ====="
  if ($tamperWorldLog -and (Test-Path $tamperWorldLog)) { Get-Content $tamperWorldLog | Where-Object { $_ -match "CHAR_HANDOFF|CHAR_RETURN|RELAY_|TRAVEL_" } }
  Write-Host "===== tamper district ====="
  if ($tamperDistrictLog -and (Test-Path $tamperDistrictLog)) { Get-Content $tamperDistrictLog | Where-Object { $_ -match "CHAR_HANDOFF|CHAR_RETURN|DISTRICT_TICKET_|RELAY_CLIENT_" } }
  Write-Host "===== tamper client ====="
  if ($tamperProbeLog -and (Test-Path $tamperProbeLog)) { Get-Content $tamperProbeLog | Where-Object { $_ -match "HANDOFF_|WORLD_HANDOFF" } }
}

if ($failure) {
  Write-Host "HANDOFF_GATE_FAIL $failure"
  exit 1
}

Write-Host "HANDOFF_GATE_OK"
exit 0
