param(
  [string]$Scratch = "$env:TEMP\apb_m7_travel_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 75
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$worldLog = Join-Path $Scratch "world.log"
$districtLog = Join-Path $Scratch "district.log"
$failure = $null
$world = $null
$district = $null

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
  Stop-ProcessTree $district
  Stop-ProcessTree $world
  Get-Process -Name "UnrealEditor","CrashReportClientEditor" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "D:\UE58\UE_5.8\Engine\Binaries\Win64\*" } |
    Stop-Process -Force -ErrorAction SilentlyContinue
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

function Start-World {
  Start-Editor @(
    $Project, "/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$worldLog"
  )
}

function Start-District([pscustomobject]$District, [int]$DistrictPort) {
  $map = "/Game/Maps/$($District.map)"
  Start-Editor @(
    $Project, "$map`?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    "-game", "-Port=$DistrictPort", "-RelayHost=127.0.0.1", "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$([int]$District.numeric_id)", "-RequireTicket",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$districtLog"
  )
}

function Start-TravelClient([string]$Id, [string]$DistrictId, [int]$DelayMs = 0) {
  $probeLog = Join-Path $Scratch "world_travel_client_$Id.log"
  $engineLog = Join-Path $Scratch "travel_$Id.log"
  $client = Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", "-game", "-WorldServerHost=127.0.0.1",
    "-APBProbe=world_travel_client", "-WSClientId=$Id", "-WSTravelDistrict=$DistrictId", "-WSTravelDelayMs=$DelayMs",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$engineLog", "-APBScratch=$Scratch"
  )
  return @{ Process = $client; ProbeLog = $probeLog; EngineLog = $engineLog }
}

try {
  if (-not (Test-Path $Project)) { Fail "project_missing" }
  if (-not (Test-Path $Editor)) { Fail "editor_missing" }
  $catalog = Get-Content (Join-Path $projectRoot "Content\Data\districts.json") -Raw | ConvertFrom-Json
  $financial = $catalog | Where-Object { $_.id -eq "Financial" } | Select-Object -First 1
  $waterfront = $catalog | Where-Object { $_.id -eq "Waterfront" } | Select-Object -First 1
  if ($null -eq $financial -or $null -eq $waterfront) { Fail "catalog_missing_required_district" }
  $financialPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financial.numeric_id)

  Remove-Item $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  $world = Start-World
  [void](Wait-Log $worldLog "RELAY_LISTEN port=$($ports.Relay)" "world_listener_timeout")
  $district = Start-District $financial $financialPort
  [void](Wait-Log $worldLog "RELAY_REGISTER district=Financial numeric_id=$([int]$financial.numeric_id) ok=1" "district_register_timeout")

  $happy = Start-TravelClient "happy" $financial.id
  [void](Wait-Log $happy.ProbeLog "TRAVEL_DISPATCH district=Financial" "happy_dispatch_timeout")
  [void](Wait-Log $districtLog "DISTRICT_TICKET_ADMITTED account=ACC-travel_happy char=Operative" "happy_admission_timeout")
  [void](Wait-Log $happy.EngineLog "TRAVEL_OK district=Financial host=127.0.0.1 port=$financialPort" "happy_travel_timeout")
  Stop-ProcessTree $happy.Process

  $second = Start-TravelClient "second" $financial.id
  [void](Wait-Log $districtLog "DISTRICT_TICKET_ADMITTED account=ACC-travel_second char=Operative" "second_admission_timeout")
  [void](Wait-Log $second.EngineLog "TRAVEL_OK district=Financial host=127.0.0.1 port=$financialPort" "second_travel_timeout")
  Stop-ProcessTree $second.Process

  $noNode = Start-TravelClient "no_node" $waterfront.id
  [void](Wait-Log $noNode.ProbeLog "TRAVEL_FAIL reason=no_live_node" "no_node_failure_timeout")
  Stop-ProcessTree $noNode.Process

  $killed = Start-TravelClient "killed" $financial.id 5000
  [void](Wait-Log $killed.ProbeLog "TRAVEL_RESERVATION district=Financial" "killed_reservation_timeout")
  Stop-ProcessTree $district
  $district = $null
  [void](Wait-Log $worldLog "RELAY_EVICT stale=1" "stale_eviction_timeout")
  [void](Wait-Log $killed.EngineLog "TRAVEL_FAIL reason=(timeout|travel_error)" "killed_failure_timeout")
  [void](Wait-Log $worldLog "TRAVEL_RESERVATION_(RELEASED|EXPIRED)" "reservation_release_timeout")
  Stop-ProcessTree $killed.Process
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-GateProcesses
  Write-Host "===== world.log ====="
  if (Test-Path $worldLog) { Get-Content $worldLog | Where-Object { $_ -match "RELAY_|TRAVEL_" } } else { Write-Host "(no world log written)" }
  Write-Host "===== district.log ====="
  if (Test-Path $districtLog) { Get-Content $districtLog | Where-Object { $_ -match "DISTRICT_TICKET_|RELAY_CLIENT_" } } else { Write-Host "(no district log written)" }
  Get-ChildItem $Scratch -Filter "travel_*.log" -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
    Write-Host "===== $($_.Name) ====="
    Get-Content $_.FullName | Where-Object { $_ -match "TRAVEL_" }
  }
}

if ($failure) {
  Write-Host "TRAVEL_GATE_FAIL $failure"
  exit 1
}

Write-Host "TRAVEL_GATE_OK"
exit 0
