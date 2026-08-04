param(
  [string]$Scratch = "$env:TEMP\apb_m7_travel_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 90,
  [switch]$NoEncryption
)

$encArgs = if ($NoEncryption) { @("-DisableEncryption") } else { @() }

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
. (Join-Path $PSScriptRoot "scripts\APBGateCleanup.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
# Phase A/B each get a fresh world+district (see the exit-on-disconnect note below).
$worldA = Join-Path $Scratch "worldA.log"
$districtA = Join-Path $Scratch "districtA.log"
$worldB = Join-Path $Scratch "worldB.log"
$districtB = Join-Path $Scratch "districtB.log"
$live = [System.Collections.Generic.List[object]]::new()

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

function Stop-GateProcesses([switch]$BestEffort) {
  Stop-APBGateProcesses -Tracked @($live) -Project $Project -EngineBin (Split-Path $Editor) -BestEffort:$BestEffort
  $live.Clear()
}

function Start-Editor([string[]]$Arguments) {
  Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
}

function Wait-Log([string]$Path, [string]$Pattern, [string]$FailureName, [Diagnostics.Process]$Process = $null) {
  $watch = [Diagnostics.Stopwatch]::StartNew()
  while ($true) {
    if ($TimeoutSec -gt 0 -and $watch.Elapsed.TotalSeconds -ge $TimeoutSec) {
      if ($null -eq $Process) { break }
    }

    $content = Read-LogText $Path
    if ($content -and $content -match $Pattern) { return $content }

    if ($null -ne $Process -and $Process.HasExited) {
      $content = Read-LogText $Path
      if ($content -and $content -match $Pattern) { return $content }
      break
    }

    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

# Non-throwing wait used for the retryable Phase B admission/travel checks: returns the
# matching content on success, or $null once BudgetSec elapses (so the caller can retry
# the whole phase on a fresh world+district instead of aborting the leg).
function Wait-LogSoft([string]$Path, [string]$Pattern, [int]$BudgetSec) {
  $watch = [Diagnostics.Stopwatch]::StartNew()
  while ($watch.Elapsed.TotalSeconds -lt $BudgetSec) {
    $content = Read-LogText $Path
    if ($content -and $content -match $Pattern) { return $content }
    Start-Sleep -Milliseconds 250
  }
  return $null
}

function Read-LogText([string]$Path) {
  if (-not (Test-Path $Path)) { return "" }
  try {
    $stream = [System.IO.File]::Open($Path, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read, [System.IO.FileShare]::ReadWrite)
    $reader = New-Object System.IO.StreamReader($stream)
    $text = $reader.ReadToEnd(); $reader.Close(); $stream.Close(); return $text
  } catch { return "" }
}

function Start-World([string]$LogPath) {
  Start-Editor (@(
    $Project, "/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    "-nullrhi", "-nosound", "-unattended", "-AbsLog=$LogPath"
  ) + $encArgs)
}

function Start-District([pscustomobject]$District, [int]$DistrictPort, [string]$LogPath) {
  $map = "/Game/Maps/$($District.map)"
  Start-Editor (@(
    $Project, "$map`?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    "-game", "-Port=$DistrictPort", "-RelayHost=127.0.0.1", "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$([int]$District.numeric_id)", "-RequireTicket",
    "-nullrhi", "-nosound", "-unattended", "-AbsLog=$LogPath"
  ) + $encArgs)
}

function Start-TravelClient([string]$Id, [string]$DistrictId, [int]$DelayMs = 0) {
  $probeLog = Join-Path $Scratch "world_travel_client_$Id.log"
  $engineLog = Join-Path $Scratch "travel_$Id.log"
  $client = Start-Editor (@(
    $Project, "127.0.0.1:$($ports.World)", "-game", "-WorldServerHost=127.0.0.1",
    "-APBProbe=world_travel_client", "-WSClientId=$Id", "-WSTravelDistrict=$DistrictId", "-WSTravelDelayMs=$DelayMs",
    "-nullrhi", "-nosound", "-unattended", "-AbsLog=$engineLog", "-APBScratch=$Scratch"
  ) + $encArgs)
  $live.Add($client)
  return @{ Process = $client; ProbeLog = $probeLog; EngineLog = $engineLog }
}

# Boot the world server, tolerating the transient engine startup ensure
# 'TaskSystemPtr.IsValid() [InterchangeTaskSystem.cpp:86]' that -unattended promotes to a
# fatal boot-abort under concurrent-build/editor DDC contention (shared multi-agent host).
# On that specific boot flake, kill and relaunch; a clean boot reaches RELAY_LISTEN quickly.
function Start-WorldWithRetry([string]$LogPath, [int]$MaxAttempts = 4, [int]$PerAttemptSec = 60) {
  for ($attempt = 1; $attempt -le $MaxAttempts; $attempt++) {
    Remove-Item $LogPath -Force -ErrorAction SilentlyContinue
    $w = Start-World $LogPath
    $sw = [Diagnostics.Stopwatch]::StartNew()
    while ($sw.Elapsed.TotalSeconds -lt $PerAttemptSec) {
      Start-Sleep -Milliseconds 400
      $txt = Read-LogText $LogPath
      if ($txt -match "RELAY_LISTEN port=$($ports.Relay)") { $live.Add($w); return $w }
      if ($txt -match "TaskSystemPtr\.IsValid" -or ($w.HasExited -and $txt -match "LogExit: Exiting")) { break }
      if ($w.HasExited) { break }
    }
    Write-Host "world_boot_retry attempt=$attempt (transient engine-init ensure or exit)"
    Stop-ProcessTree $w
    Start-Sleep -Seconds 2
  }
  Fail "world_listener_timeout"
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
  # M16 zero-trust: world/district processes preflight APB_DEPLOYMENT_SECRET and halt when it
  # is missing. The spine exports it for child gates; standalone leg runs must set it too.
  [Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
  # M7 teardown hardening: kill any leftover editors from a prior run BEFORE Phase A
  # launches, so this leg (first in run_m7_gate.ps1) never starts with a live ghost.
  # Sweep-and-verify throws if any persist.
  Stop-GateProcesses

  # ---------------------------------------------------------------------------
  # PHASE A - failure modes (no client completes travel, so no world-connection
  # ever closes; the Game-target listen-server world stays alive throughout).
  #
  # The world server here is a -game ?listen process: the binary-engine stopgap
  # for the dedicated Server target that cannot be built with the installed
  # engine (see work/m6_server_target_limit.md, work/m7_world_server_exit_on_disconnect.md).
  # When a remote client's world connection CLOSES - which a successful travel
  # inherently does, since the client leaves the world to join the district -
  # the standalone game loop exits ~4s later via the Launch.cpp cleanup guard.
  # A real dedicated build has no client game loop and is unaffected. So the
  # no-travel failure checks share one world, and the successful-travel checks
  # (Phase B) get their own fresh world.
  # ---------------------------------------------------------------------------
  $world = Start-WorldWithRetry $worldA
  $district = Start-District $financial $financialPort $districtA
  $live.Add($district)
  [void](Wait-Log $worldA "RELAY_REGISTER district=Financial numeric_id=$([int]$financial.numeric_id) ok=1" "phaseA_district_register_timeout")

  # no_node: a district with no live node rejects the reservation up front.
  $noNode = Start-TravelClient "no_node" $waterfront.id
  [void](Wait-Log $noNode.ProbeLog "TRAVEL_FAIL reason=no_live_node" "no_node_failure_timeout" $noNode.Process)

  # killed: reservation is granted, then the district dies before the client
  # dispatches, so the world evicts it and the reservation expires/releases.
  $killed = Start-TravelClient "killed" $financial.id 5000
  [void](Wait-Log $killed.ProbeLog "TRAVEL_RESERVATION district=Financial" "killed_reservation_timeout" $killed.Process)
  Stop-ProcessTree $district
  [void](Wait-Log $worldA "RELAY_EVICT stale=1" "stale_eviction_timeout")
  [void](Wait-Log $killed.EngineLog "TRAVEL_FAIL reason=(timeout|travel_error)" "killed_failure_timeout" $killed.Process)
  [void](Wait-Log $worldA "TRAVEL_RESERVATION_(RELEASED|EXPIRED)" "reservation_release_timeout")

  Stop-GateProcesses

  # ---------------------------------------------------------------------------
  # PHASE B - multi-client travel (the DoD requirement: a SECOND client also
  # travels into the same dedicated district). Both clients boot in parallel and
  # obtain their world-minted tickets while both are connected; each then travels
  # into the district. The district validates the signed ticket independently of
  # the world, so both are admitted even though the world's own game loop exits
  # shortly after the first client's travel closes its world connection. This is
  # the behaviour proven by work/_verify_overlap.ps1.
  # ---------------------------------------------------------------------------
  # Each client mints its signed reservation ~1s after ITS OWN login (fast, right
  # after connect); WSTravelDelayMs only postpones the later DISPATCH
  # (StartDistrictTravel), which closes the client's world connection. The world
  # tears itself down ~2.5s after the FIRST dispatch-close (Game-target listen-server
  # stopgap; see work/m7_world_server_exit_on_disconnect.md), and the dispatch step
  # re-reads the replicated reservation each tick, so BOTH clients must dispatch
  # within that window - i.e. their dispatch times (login + ~1s + delay) must land
  # within ~2.5s of each other. Equal short delays make the dispatch gap equal the
  # login skew, which build contention on this shared multi-agent host can stretch
  # past the window. Retry the whole of Phase B on a fresh world+district (targeted
  # teardown only - never the broad editor kill, to preserve concurrent agents) so a
  # transient contention spike does not fail the leg; a quieter retry lands both
  # dispatches inside the window.
  $MaxPhaseB = 3
  $phaseBOk = $false
  $lastPhaseBFail = "phaseB_not_attempted"
  $happyBudget = if ($TimeoutSec -gt 0) { $TimeoutSec } else { 90 }
  for ($attempt = 1; $attempt -le $MaxPhaseB -and -not $phaseBOk; $attempt++) {
    if ($attempt -gt 1) {
      Write-Host "phaseB_retry attempt=$attempt reason=$lastPhaseBFail"
      foreach ($p in $live) { Stop-ProcessTree $p }
      $live.Clear()
      Remove-Item $worldB, $districtB -Force -ErrorAction SilentlyContinue
      Get-ChildItem $Scratch -Filter "travel_*.log" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
      Get-ChildItem $Scratch -Filter "world_travel_client_*.log" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
      Start-Sleep -Seconds 2
    }
    $world = Start-WorldWithRetry $worldB
    $district = Start-District $financial $financialPort $districtB
    $live.Add($district)
    [void](Wait-Log $worldB "RELAY_REGISTER district=Financial numeric_id=$([int]$financial.numeric_id) ok=1" "phaseB_district_register_timeout")

    $happy = Start-TravelClient "happy" $financial.id 1500
    Start-Sleep -Milliseconds 300
    $second = Start-TravelClient "second" $financial.id 1500

    # Both must be admitted to the district (independent of world lifetime). Soft waits
    # so a contention-stretched miss retries the phase instead of aborting the leg.
    if (-not (Wait-LogSoft $districtB "DISTRICT_TICKET_ADMITTED account=ACC-travel_happy char=travel_happy" $happyBudget)) { $lastPhaseBFail = "happy_admission_timeout"; continue }
    if (-not (Wait-LogSoft $districtB "DISTRICT_TICKET_ADMITTED account=ACC-travel_second char=travel_second" 30)) { $lastPhaseBFail = "second_admission_timeout"; continue }
    if (-not (Wait-LogSoft $happy.EngineLog "TRAVEL_OK district=Financial host=127.0.0.1 port=$financialPort" 30)) { $lastPhaseBFail = "happy_travel_timeout"; continue }
    if (-not (Wait-LogSoft $second.EngineLog "TRAVEL_OK district=Financial host=127.0.0.1 port=$financialPort" 30)) { $lastPhaseBFail = "second_travel_timeout"; continue }
    $phaseBOk = $true
  }
  if (-not $phaseBOk) { Fail $lastPhaseBFail }
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-GateProcesses -BestEffort
  foreach ($pair in @(@("worldA", $worldA), @("districtA", $districtA), @("worldB", $worldB), @("districtB", $districtB))) {
    Write-Host "===== $($pair[0]).log ====="
    $t = Read-LogText $pair[1]
    if ($t) { $t -split "`n" | Where-Object { $_ -match "RELAY_|TRAVEL_|DISTRICT_TICKET_" } } else { Write-Host "(no log written)" }
  }
  Get-ChildItem $Scratch -Filter "travel_*.log" -ErrorAction SilentlyContinue | Sort-Object Name | ForEach-Object {
    Write-Host "===== $($_.Name) ====="
    (Read-LogText $_.FullName) -split "`n" | Where-Object { $_ -match "TRAVEL_" }
  }
}

if ($failure) {
  Write-Host "TRAVEL_GATE_FAIL $failure"
  exit 1
}

Write-Host "TRAVEL_GATE_OK"
exit 0
