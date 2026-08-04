# APBGateCleanup.ps1 - hardened process teardown shared by the M7 gates.
# Contract: after Stop-APBGateProcesses returns without -BestEffort, no editor process for
# this project/engine is alive - leftover editors can never survive into the next gate run.
# The sweep matches the historical per-gate behavior (project match OR engine-bin path match)
# but adds a recursive tree kill plus a bounded sweep-and-verify loop with a loud leftover
# marker, and throws on persistent survivors unless -BestEffort is set (for finally blocks).
# NOTE: the engine-bin OR clause is intentionally destructive - it kills ANY editor/crash-
# reporter under the engine binaries regardless of project (matching the 5 engine-path gates'
# pre-existing behavior), so gate runs never inherit a live editor from any prior run.

function Stop-APBProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
      try {
        Stop-APBProcessTree (Get-Process -Id $child.ProcessId -ErrorAction Stop)
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

function Get-APBGateEditors([string]$Project, [string]$EngineBin) {
  $escapedProject = [regex]::Escape($Project)
  return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -in @('UnrealEditor.exe', 'UnrealEditor-Cmd.exe', 'APBReloaded.exe',
                    'CrashReportClient.exe', 'CrashReportClientEditor.exe') -and
      (($_.CommandLine -match $escapedProject) -or ($_.ExecutablePath -like "$EngineBin\*"))
    })
}

function Stop-APBGateProcesses {
  param(
    [Diagnostics.Process[]]$Tracked = @(),
    [string]$Project,
    [string]$EngineBin,
    [int]$WaitSec = 10,
    [switch]$BestEffort
  )
  foreach ($Process in $Tracked) { Stop-APBProcessTree $Process }
  $deadline = (Get-Date).AddSeconds($WaitSec)
  $survivors = @(Get-APBGateEditors -Project $Project -EngineBin $EngineBin)
  while ($survivors.Count -gt 0 -and (Get-Date) -lt $deadline) {
    foreach ($Survivor in $survivors) {
      Stop-Process -Id $Survivor.ProcessId -Force -ErrorAction SilentlyContinue
    }
    Start-Sleep -Milliseconds 500
    $survivors = @(Get-APBGateEditors -Project $Project -EngineBin $EngineBin)
  }
  foreach ($Survivor in $survivors) {
    Write-Host "APB_GATE_CLEANUP_LEFTOVER pid=$($Survivor.ProcessId) name=$($Survivor.Name)"
  }
  if ($survivors.Count -gt 0 -and -not $BestEffort) {
    throw "leftover_editors_persist:$($survivors.Count)"
  }
  return $survivors.Count
}
