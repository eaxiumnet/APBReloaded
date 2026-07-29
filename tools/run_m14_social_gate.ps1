# M14 social gate: exercises cross-player clan/friends/group/mail via the world
# authority + district relay. Requires the world server to emit SOCIAL_GATE_* markers
# and both social_probe clients to emit their role-specific OK markers.
param(
  [string]$Scratch = "$env:TEMP\apb_m14_social_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 180
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'scripts\APBPortContract.ps1')
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
$world = $null
$financial = $null
$alice = $null
$bob = $null
$worldLog = $null
$financialLog = $null
$aliceLog = $null
$bobLog = $null
$deadline = $null

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

function Stop-ProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
      try {
        $childProcess = Get-Process -Id $child.ProcessId -ErrorAction Stop
        Stop-ProcessTree $childProcess
      } catch {
        Write-Verbose "child_process_already_stopped id=$($child.ProcessId)"
      }
    }
    if (-not $Process.HasExited) {
      Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
      $Process.WaitForExit(10000) | Out-Null
    }
  } catch {
    Write-Verbose "process_tree_cleanup_failed id=$($Process.Id)"
  }
}

function Stop-GateProcesses {
  Stop-ProcessTree $bob
  Stop-ProcessTree $alice
  Stop-ProcessTree $financial
  Stop-ProcessTree $world
  $escapedProject = [regex]::Escape($Project)
  Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -in @('UnrealEditor.exe', 'CrashReportClientEditor.exe') -and
      $_.CommandLine -match $escapedProject
    } |
    ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
}

function Assert-PortsFree([int[]]$DistrictPorts) {
  $tcp = @(Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object {
    $_.LocalPort -eq $ports.Relay
  })
  $udp = @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue | Where-Object {
    $_.LocalPort -in @($ports.World) + $DistrictPorts
  })
  if ($tcp.Count -ne 0 -or $udp.Count -ne 0) { Fail 'ports_not_free' }
}

function Start-Editor([string[]]$Arguments) {
  Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

function Wait-Log([string]$Path, [string]$Pattern, [string]$FailureName) {
  while ([Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline) {
    if (Test-Path -LiteralPath $Path) {
      $content = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
      if ($content -match $Pattern) { return $content }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

function Start-District($District, [int]$Port, [string]$LogPath) {
  Start-Editor @(
    $Project,
    "/Game/Maps/$($District.map)?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    '-game', "-Port=$Port", '-RelayHost=127.0.0.1', "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$([int]$District.numeric_id)", '-RequireTicket',
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath"
  )
}

function Start-SocialClient([string]$Id, [string]$Role, [string]$LogPath) {
  Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)",
    '-game', "-WorldServerHost=127.0.0.1", "-WSClientId=$Id",
    '-APBProbe=social_probe', "-SocialRole=$Role", "-APBScratch=$Scratch",
    '-nullvolumetrics', '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath"
  )
}

function Show-Log([string]$Title, [string]$Path, [string]$Pattern) {
  Write-Host "===== $Title ====="
  if ($Path -and (Test-Path -LiteralPath $Path)) {
    Get-Content -LiteralPath $Path | Where-Object { $_ -match $Pattern }
  } else {
    Write-Host '(no log written)'
  }
}

try {
  if ($TimeoutSec -le 0) { Fail 'invalid_timeout' }
  if (-not (Test-Path -LiteralPath $Project)) { Fail 'project_missing' }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail 'editor_missing' }

  $catalog = Get-Content (Join-Path $projectRoot 'Content\Data\districts.json') -Raw | ConvertFrom-Json
  $financialEntry = $catalog | Where-Object { $_.id -eq 'Financial' } | Select-Object -First 1
  if ($null -eq $financialEntry) { Fail 'financial_district_catalog_missing' }
  $financialPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financialEntry.numeric_id)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Stop-GateProcesses
  Assert-PortsFree @($financialPort)

  $deadline = [Diagnostics.Stopwatch]::GetTimestamp() + ($TimeoutSec * [Diagnostics.Stopwatch]::Frequency)
  $worldLog = Join-Path $Scratch 'world.log'
  $financialLog = Join-Path $Scratch 'financial.log'
  $aliceLog = Join-Path $Scratch 'social_probe_alice.log'
  $bobLog = Join-Path $Scratch 'social_probe_bob.log'

  $world = Start-Editor @(
    $Project, '/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode',
    '-game', '-WorldServer', "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$worldLog"
  )

  $financial = Start-District $financialEntry $financialPort $financialLog

  Start-Sleep 4

  $alice = Start-SocialClient 'alice' 'alice' $aliceLog
  $bob = Start-SocialClient 'bob' 'bob' $bobLog

  # Wait for both clients to finish their social sequences.
  Wait-Log $aliceLog 'SOCIAL_PROBE_ALICE_OK' 'alice_social_probe_incomplete' | Out-Null
  Wait-Log $bobLog 'SOCIAL_PROBE_BOB_OK' 'bob_social_probe_incomplete' | Out-Null

  # World authority must have emitted gate markers for clan/friends/group.
  Wait-Log $worldLog 'SOCIAL_GATE_CLAN_OK' 'social_gate_clan_missing' | Out-Null
  Wait-Log $worldLog 'SOCIAL_GATE_FRIENDS_OK' 'social_gate_friends_missing' | Out-Null
  Wait-Log $worldLog 'SOCIAL_GATE_GROUP_OK' 'social_gate_group_missing' | Out-Null

} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
  Stop-GateProcesses
  Show-Log 'WORLD SOCIAL' $worldLog 'SOCIAL_GATE_|SOCIAL_RELAY_|SOCIAL_RPC_|SOCIAL_RESULT'
  Show-Log 'ALICE SOCIAL' $aliceLog 'SOCIAL_PROBE_|SOCIAL_CLAN_|SOCIAL_FRIEND_|SOCIAL_GROUP_|SOCIAL_MAIL_'
  Show-Log 'BOB SOCIAL' $bobLog 'SOCIAL_PROBE_|SOCIAL_CLAN_|SOCIAL_FRIEND_|SOCIAL_GROUP_|SOCIAL_MAIL_'
}

if ($failure) {
  Write-Host "M14_SOCIAL_GATE_FAIL $failure"
  exit 1
}
Write-Host 'M14_SOCIAL_GATE_OK'
exit 0
