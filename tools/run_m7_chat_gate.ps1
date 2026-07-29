param(
  [string]$Scratch = "$env:TEMP\apb_m7_chat_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'scripts\APBPortContract.ps1')
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
$world = $null
$financial = $null
$social = $null
$alice = $null
$bob = $null
$worldLog = $null
$financialLog = $null
$socialLog = $null
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
  Stop-ProcessTree $social
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

function Start-ChatClient([string]$Id, [string]$Character, [string]$District, [string]$ExecCommands, [string]$LogPath) {
  Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", '-game', '-WorldServerHost=127.0.0.1',
    '-APBProbe=world_chat_client', "-WSClientId=$Id", "-APBChatCharacter=$Character", "-APBChatDistrict=$District", "-WorldPort=$($ports.World)",
    "-ExecCmds=`"$ExecCommands`"", '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
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
  $socialEntry = $catalog | Where-Object { $_.id -eq 'Social' } | Select-Object -First 1
  if ($null -eq $financialEntry -or $null -eq $socialEntry) { Fail 'chat_district_catalog_missing' }
  $financialPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financialEntry.numeric_id)
  $socialPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$socialEntry.numeric_id)
  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Stop-GateProcesses
  Assert-PortsFree @($financialPort, $socialPort)
  $deadline = [Diagnostics.Stopwatch]::GetTimestamp() + ($TimeoutSec * [Diagnostics.Stopwatch]::Frequency)
  $worldLog = Join-Path $Scratch 'world.log'
  $financialLog = Join-Path $Scratch 'financial.log'
  $socialLog = Join-Path $Scratch 'social.log'
  $aliceLog = Join-Path $Scratch 'alice.log'
  $bobLog = Join-Path $Scratch 'bob.log'
  $world = Start-Editor @(
    $Project, '/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode',
    '-game', '-WorldServer', "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$worldLog"
  )
  $financial = Start-District $financialEntry $financialPort $financialLog
  $social = Start-District $socialEntry $socialPort $socialLog
  $alice = Start-ChatClient 'alice' 'ChatAlice' 'Financial' 'APBChat 5000 /d m7_in_district,APBChat 26000 /w ChatBob m7_cross_district,APBChat 40000 /d flood_1,APBChat 40000 /d flood_2,APBChat 40000 /d flood_3,APBChat 40000 /d flood_4,APBChat 40000 /d flood_5,APBChat 40000 /d flood_6,APBChat 40000 /d flood_7,APBChat 40000 /d flood_8,APBChat 40000 /d flood_9' $aliceLog
  $bob = Start-ChatClient 'bob' 'ChatBob' 'Financial' 'APBChatTravel 10000 Social' $bobLog
  $aliceProbe = Join-Path $Scratch 'world_chat_client_alice.log'
  $bobProbe = Join-Path $Scratch 'world_chat_client_bob.log'
  Wait-Log $aliceProbe 'CHAT_CLIENT_ARRIVED char=ChatAlice district=Financial' 'alice_not_admitted' | Out-Null
  Wait-Log $bobProbe 'CHAT_CLIENT_ARRIVED char=ChatBob district=Financial' 'bob_not_admitted' | Out-Null
  Wait-Log $financialLog 'CHAT_DELIVERED channel=District from=ChatAlice to=ChatBob' 'in_district_delivery_missing' | Out-Null
  Wait-Log $bobProbe 'CHAT_CLIENT_ARRIVED char=ChatBob district=Social' 'bob_not_admitted_social' | Out-Null
  Wait-Log $financialLog 'CHAT_RELAY_FORWARD to=ChatBob' 'chat_relay_not_forwarded' | Out-Null
  Wait-Log $socialLog 'CHAT_DELIVERED channel=Whisper from=ChatAlice to=ChatBob' 'cross_district_delivery_missing' | Out-Null
  Wait-Log $financialLog 'CHAT_DENIED reason=Muted' 'typed_denial_missing' | Out-Null
} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
  Stop-GateProcesses
  Show-Log 'WORLD CHAT' $worldLog 'CHAT_|RELAY_'
  Show-Log 'FINANCIAL CHAT' $financialLog 'CHAT_|RELAY_'
  Show-Log 'SOCIAL CHAT' $socialLog 'CHAT_|RELAY_'
  Show-Log 'ALICE CHAT' $aliceLog 'CHAT_'
}

if ($failure) {
  Write-Host "CHAT_GATE_FAIL $failure"
  exit 1
}
Write-Host 'CHAT_GATE_OK'
exit 0
