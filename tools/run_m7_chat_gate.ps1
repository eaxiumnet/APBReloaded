param(
  [string]$Scratch = "$env:TEMP\apb_m7_chat_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  # Default budget also covers the duplicate-Social ghost boot added by the M7 ghost-process
  # regression below (the spine and run_m7_gate.ps1 do not override this default).
  [int]$TimeoutSec = 180
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'scripts\APBPortContract.ps1')
. (Join-Path $PSScriptRoot 'scripts\APBGateCleanup.ps1')
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
$world = $null
$financial = $null
$social = $null
$socialGhost = $null
$alice = $null
$bob = $null
$worldLog = $null
$financialLog = $null
$socialLog = $null
$socialGhostLog = $null
$aliceLog = $null
$bobLog = $null
$deadline = $null

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

function Stop-GateProcesses([switch]$BestEffort) {
  Stop-APBGateProcesses -Tracked @($bob, $alice, $social, $socialGhost, $financial, $world) `
    -Project $Project -EngineBin (Split-Path $Editor) -BestEffort:$BestEffort
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
  Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
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
    '-nullrhi', '-nosound', '-unattended', "-AbsLog=$LogPath"
  )
}

function Start-ChatClient([string]$Id, [string]$Character, [string]$District, [string]$ExecCommands, [string]$LogPath) {
  Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", '-game', '-WorldServerHost=127.0.0.1',
    '-APBProbe=world_chat_client', "-WSClientId=$Id", "-APBChatCharacter=$Character", "-APBChatDistrict=$District", "-WorldPort=$($ports.World)",
    "-ExecCmds=`"$ExecCommands`"", '-nullrhi', '-nosound', '-unattended', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
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
  # M16 zero-trust: world/district processes preflight APB_DEPLOYMENT_SECRET and halt when it
  # is missing. The spine exports it for child gates; standalone leg runs must set it too.
  [Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
  Stop-GateProcesses
  Assert-PortsFree @($financialPort, $socialPort)
  $deadline = [Diagnostics.Stopwatch]::GetTimestamp() + ($TimeoutSec * [Diagnostics.Stopwatch]::Frequency)
  $worldLog = Join-Path $Scratch 'world.log'
  $financialLog = Join-Path $Scratch 'financial.log'
  $socialLog = Join-Path $Scratch 'social.log'
  $socialGhostLog = Join-Path $Scratch 'social_ghost.log'
  $aliceLog = Join-Path $Scratch 'alice.log'
  $bobLog = Join-Path $Scratch 'bob.log'
  $world = Start-Editor @(
    $Project, '/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode',
    '-game', '-WorldServer', "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    '-nullrhi', '-nosound', '-unattended', "-AbsLog=$worldLog"
  )
  $financial = Start-District $financialEntry $financialPort $financialLog
  $social = Start-District $socialEntry $socialPort $socialLog
  # M7 ghost-process regression: a duplicate Social district (leftover ghost process) must not
  # wedge the live relay. The ghost is launched only after the live district has registered, so
  # the duplicate-close guard sees a fresh live socket and retains both. It reuses the live
  # numeric_id AND port (the world rejects a same-id register with a different port); its UE
  # listen bind fails, but its relay client still registers and heartbeats - exactly the
  # scenario that ping-ponged before the staleness guard.
  # Negative control: if the guard is reverted, the world's pre-guard RELAY_DUPLICATE_CLIENT
  # line has no stale= field, so the stale=0 wait below times out (duplicate_guard_not_exercised)
  # and the ping-pong yields post-registration reconnects (live/ghost_relay_reconnected).
  # Live-first accept order is load-bearing: QueueToDistrict delivers to the first matching
  # socket only, so the live (accepted before the ghost) must be the first in the Clients array
  # for the cross-district whisper to reach it - do not parallelize the ghost launch.
  Wait-Log $worldLog "RELAY_REGISTER district=Social numeric_id=$([int]$socialEntry.numeric_id) ok=1" 'social_not_registered' | Out-Null
  $socialGhost = Start-District $socialEntry $socialPort $socialGhostLog
  Wait-Log $socialGhostLog "RELAY_CLIENT_REGISTERED district=Social numeric_id=$([int]$socialEntry.numeric_id)" 'ghost_not_registered' | Out-Null
  Wait-Log $worldLog "RELAY_DUPLICATE_CLIENT.*numeric_id=$([int]$socialEntry.numeric_id) stale=0" 'duplicate_guard_not_exercised' | Out-Null
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
  # Ghost regression assertions: after the live and ghost districts were registered, neither
  # relay may have reconnected (the pre-guard ping-pong). Boot-time connect races BEFORE each
  # district's RELAY_CLIENT_REGISTERED marker are excluded by checking only lines after it.
  $liveRegisteredAt = @(Get-Content -LiteralPath $socialLog | Select-String 'RELAY_CLIENT_REGISTERED district=Social' | Select-Object -First 1).LineNumber
  if ($null -eq $liveRegisteredAt) { Fail 'live_registered_marker_missing' }
  if (Get-Content -LiteralPath $socialLog | Select-Object -Skip $liveRegisteredAt | Select-String 'RELAY_CLIENT_RECONNECT') { Fail 'live_relay_reconnected' }
  $ghostRegisteredAt = @(Get-Content -LiteralPath $socialGhostLog | Select-String 'RELAY_CLIENT_REGISTERED district=Social' | Select-Object -First 1).LineNumber
  if ($null -eq $ghostRegisteredAt) { Fail 'ghost_registered_marker_missing' }
  if (Get-Content -LiteralPath $socialGhostLog | Select-Object -Skip $ghostRegisteredAt | Select-String 'RELAY_CLIENT_RECONNECT') { Fail 'ghost_relay_reconnected' }
  Wait-Log $financialLog 'CHAT_DENIED reason=Muted' 'typed_denial_missing' | Out-Null
} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
  Stop-GateProcesses -BestEffort
  Show-Log 'WORLD CHAT' $worldLog 'CHAT_|RELAY_'
  Show-Log 'FINANCIAL CHAT' $financialLog 'CHAT_|RELAY_'
  Show-Log 'SOCIAL CHAT' $socialLog 'CHAT_|RELAY_'
  Show-Log 'SOCIAL GHOST CHAT' $socialGhostLog 'RELAY_|CHAT_'
  Show-Log 'ALICE CHAT' $aliceLog 'CHAT_'
}

if ($failure) {
  Write-Host "CHAT_GATE_FAIL $failure"
  exit 1
}
Write-Host 'CHAT_GATE_OK'
exit 0
