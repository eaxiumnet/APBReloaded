# run_m16_zerotrust_gate.ps1 — M16 zero-trust assertion-registry harness.
#
# WHY -Only EXISTS:
#   The gate reports only FAIL reason=<first-failure>. A whole-gate run cannot produce a
#   per-assertion RED transcript: the second RED is masked by the first, and an assertion
#   that legitimately goes GREEN cannot turn the whole gate green while a later one is
#   still RED. -Only runs exactly one (or more named) assertion(s) and reports only that
#   assertion's own state. Each owning task adds ONE assertion, runs it alone with -Only,
#   captures RED, makes the COMPLETE production change, then re-runs -Only for GREEN.
#
# REGISTRY IS EMPTY. Later tasks append exactly one record using this shape:
#
#   [pscustomobject]@{
#     Name       = 'UPPER_SNAKE_ASSERTION_NAME'   # UPPER_SNAKE
#     Scenario   = 'S-Wx-y'
#     OwningTask = 'TxxY'
#     Run        = { <scriptblock: return $null on pass; return 'reason' on fail> }
#   }
#
# Assertion ownership (each added RED-first by its owning task; do NOT add live records):
#   # ENCRYPTION_ACTIVE              T00C  S-W0-7
#   # AUTH_REFUSED_PLAINTEXT         T00C  S-W0-8
#   # SECRET_PROVIDER_HALTS          T00A  S-W0-5
#   # STALE_AUTH_CALLBACK_REJECTED   T02A  S-W2-8
#   # CLAN_ACTOR_SPOOF_DENIED        T03B  S-W3-3
#   # ISSUE_TICKET_DENIED            T03B  S-W3-4
#   # OWNER_ONLY_REPLICATION_OK      T03C  S-W3-7
#   # SERVER_REGISTER_ROUTED         T07A  S-W7-1
#   # NATIVECONSTRUCT_SEED_GATED     T07A  S-W7-2
#   # DISTRICT_EPOCH_RESTART_REFUSED T04A  S-W4-5
#   # ONE_SESSION_KICK_DENIES_MUTATION T07B S-W7-3
param(
  [string[]]$Only       = @(),
  [string]  $Scratch    = "$env:TEMP\apb_m16_zerotrust_gate",
  [string]  $Project    = "D:\APBReloaded\APBReloaded.uproject",
  [string]  $Editor     = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]     $TimeoutSec = 180
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot  = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'scripts\APBPortContract.ps1')
$ports        = Get-APBPortContract -ProjectRoot $projectRoot
$failure      = $null
$leaked       = -1
$portsToCheck = @()

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

function Stop-ProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
      try {
        Stop-ProcessTree (Get-Process -Id $child.ProcessId -ErrorAction Stop)
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

function Get-GateProcesses {
  $escapedProject = [regex]::Escape($Project)
  $escapedScratch = [regex]::Escape("-APBScratch=$Scratch")
  return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -in @('APBReloaded.exe', 'CrashReportClient.exe', 'UnrealEditor.exe', 'CrashReportClientEditor.exe') -and
      $_.CommandLine -match $escapedProject -and $_.CommandLine -match $escapedScratch
    })
}

function Stop-AllGateProcesses {
  Get-GateProcesses | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
  }
}

function Get-BoundPortCount([int[]]$DistrictPorts) {
  $tcp = @(Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
    Where-Object { $_.LocalPort -eq $ports.Relay })
  $udp = @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue |
    Where-Object { $_.LocalPort -in @($ports.World) + $DistrictPorts })
  return $tcp.Count + $udp.Count
}

function Assert-PortsFree([int[]]$DistrictPorts) {
  if ((Get-BoundPortCount $DistrictPorts) -ne 0) { Fail 'ports_not_free' }
}

function Start-Editor([string[]]$Arguments) {
  return Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru `
    -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

function Launch-World([string]$LogPath) {
  return Start-Editor @(
    $Project, '/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode',
    '-game', '-WorldServer', "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
  )
}

function Launch-District([pscustomobject]$District, [string]$InstanceId,
  [int]$InstanceNumericId, [int]$InstancePort, [string]$LogPath) {
  return Start-Editor @(
    $Project, "/Game/Maps/$($District.map)?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    '-game', "-Port=$InstancePort", '-RelayHost=127.0.0.1', "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$InstanceNumericId", '-RequireTicket',
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
  )
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

function Read-LogRaw([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  return Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
}

function Show-Log([string]$Title, [string]$Path, [string]$Pattern) {
  Write-Host "===== $Title ====="
  if ($Path -and (Test-Path -LiteralPath $Path)) {
    Get-Content -LiteralPath $Path | Where-Object { $_ -match $Pattern }
  } else {
    Write-Host '(no log written)'
  }
}

# ---------------------------------------------------------------------------
# ASSERTION REGISTRY — ships EMPTY. Each owning task appends exactly one record.
#
# Record shape to append:
#   [pscustomobject]@{
#     Name       = 'UPPER_SNAKE_ASSERTION_NAME'   # UPPER_SNAKE
#     Scenario   = 'S-Wx-y'
#     OwningTask = 'TxxY'
#     Run        = { <scriptblock: return $null on pass; return 'reason' on fail> }
#   }
#
# Ownership (added RED-first by owning task; DO NOT add a live record here):
#   # ENCRYPTION_ACTIVE              T00C  S-W0-7
#   # AUTH_REFUSED_PLAINTEXT         T00C  S-W0-8
#   # SECRET_PROVIDER_HALTS          T00A  S-W0-5
#   # STALE_AUTH_CALLBACK_REJECTED   T02A  S-W2-8
#   # CLAN_ACTOR_SPOOF_DENIED        T03B  S-W3-3
#   # ISSUE_TICKET_DENIED            T03B  S-W3-4
#   # OWNER_ONLY_REPLICATION_OK      T03C  S-W3-7
#   # SERVER_REGISTER_ROUTED         T07A  S-W7-1
#   # NATIVECONSTRUCT_SEED_GATED     T07A  S-W7-2
#   # DISTRICT_EPOCH_RESTART_REFUSED T04A  S-W4-5
#   # ONE_SESSION_KICK_DENIES_MUTATION T07B S-W7-3
# ---------------------------------------------------------------------------
$assertionRegistry = [System.Collections.Generic.List[pscustomobject]]::new()
# <<< owning tasks append records here — one at a time, RED before production edit >>>
$assertionRegistry.Add([pscustomobject]@{
  Name       = 'ENCRYPTION_ACTIVE'
  Scenario   = 'S-W0-7'
  OwningTask = 'T00C'
  Run        = {
    # Prove the world server negotiates AES-GCM on startup:
    # it must log TRANSPORT_ENCRYPTION_ACTIVE when the encryption delegate is bound
    # and the PacketHandler encryption component is wired into the world NetDriver.
    $logPath = Join-Path $Scratch 'encryption_active_world.log'
    [Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
    $world = $null
    try {
      $world = Launch-World $logPath
      $content = Wait-Log $logPath 'TRANSPORT_ENCRYPTION_ACTIVE|DEPLOYMENT_SECRET_PROVIDER_HALT' 'encryption_inactive'
      if ($content -notmatch 'TRANSPORT_ENCRYPTION_ACTIVE') { return 'encryption_inactive' }
      return $null
    } finally {
      Stop-ProcessTree $world
    }
  }
})

$assertionRegistry.Add([pscustomobject]@{
  Name       = 'SECRET_PROVIDER_HALTS'
  Scenario   = 'S-W0-5'
  OwningTask = 'T00A'
  Run        = {
    $logPath = Join-Path $Scratch 'missing_secret_world.log'
    $secretName = 'APB_DEPLOYMENT_SECRET'
    $secretFileName = 'APB_DEPLOYMENT_SECRET_FILE'
    $oldSecret = [Environment]::GetEnvironmentVariable($secretName, 'Process')
    $oldSecretFile = [Environment]::GetEnvironmentVariable($secretFileName, 'Process')
    $world = $null
    try {
      [Environment]::SetEnvironmentVariable($secretName, $null, 'Process')
      [Environment]::SetEnvironmentVariable($secretFileName, $null, 'Process')
      $world = Launch-World $logPath
      $content = Wait-Log $logPath `
        'DEPLOYMENT_SECRET_PROVIDER_HALT reason=|APBServerControl role=WorldServer' `
        'secret_provider_halt_not_observed'
      Start-Sleep -Milliseconds 500
      if ((Get-BoundPortCount @()) -ne 0) { return 'role_listened_without_secret' }
      if ($content -notmatch 'DEPLOYMENT_SECRET_PROVIDER_HALT reason=missing_secret') {
        return 'missing_secret_halt_not_logged'
      }

      Stop-ProcessTree $world
      $world = $null
      $logPath = Join-Path $Scratch 'malformed_secret_world.log'
      [Environment]::SetEnvironmentVariable($secretName, 'not-hex', 'Process')
      $world = Launch-World $logPath
      $content = Wait-Log $logPath `
        'DEPLOYMENT_SECRET_PROVIDER_HALT reason=|APBServerControl role=WorldServer' `
        'malformed_secret_halt_not_observed'
      Start-Sleep -Milliseconds 500
      if ((Get-BoundPortCount @()) -ne 0) { return 'role_listened_with_malformed_secret' }
      if ($content -notmatch 'DEPLOYMENT_SECRET_PROVIDER_HALT reason=malformed_secret') {
        return 'malformed_secret_halt_not_logged'
      }
      return $null
    } finally {
      Stop-ProcessTree $world
      [Environment]::SetEnvironmentVariable($secretName, $oldSecret, 'Process')
      [Environment]::SetEnvironmentVariable($secretFileName, $oldSecretFile, 'Process')
    }
  }
})

try {
  if ($TimeoutSec -le 0) { Fail 'invalid_timeout' }
  if (-not (Test-Path -LiteralPath $Project)) { Fail 'project_missing' }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail 'editor_missing' }

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Stop-AllGateProcesses
  Assert-PortsFree $portsToCheck
  $deadline = [Diagnostics.Stopwatch]::GetTimestamp() + ($TimeoutSec * [Diagnostics.Stopwatch]::Frequency)

  $toRun = if ($Only.Count -gt 0) {
    $resolved = [System.Collections.Generic.List[pscustomobject]]::new()
    foreach ($name in $Only) {
      $rec = $assertionRegistry | Where-Object { $_.Name -eq $name } | Select-Object -First 1
      if ($null -eq $rec) { Fail "unknown_assertion_$name" }
      $resolved.Add($rec)
    }
    $resolved
  } else {
    $assertionRegistry
  }

  foreach ($assertion in $toRun) {
    Write-Host "ASSERT $($assertion.Name) scenario=$($assertion.Scenario) task=$($assertion.OwningTask)"
    $reason = & $assertion.Run
    if ($null -ne $reason -and $reason -ne '') { Fail $reason }
    Write-Host "ASSERT_OK $($assertion.Name)"
  }
} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
  Stop-AllGateProcesses
  Start-Sleep -Milliseconds 500
  $leaked = @(Get-GateProcesses).Count + (Get-BoundPortCount $portsToCheck)
  if ($leaked -ne 0 -and -not $failure) { $failure = "cleanup_leaked_$leaked" }
  Write-Host "LEAKED=$leaked"
}

if ($failure) {
  Write-Host "FAIL reason=$failure"
  exit 1
}

Write-Host 'M16_ZEROTRUST_GATE_OK'
exit 0
