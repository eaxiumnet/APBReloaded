<#
.SYNOPSIS
  Unified / required M7 travel-spine gate. Composes the five proven-green M7 leg
  gates as sequential child processes and emits exactly one terminal marker on
  success: M7_TRAVEL_GATE_OK (binding contract literal - do NOT rename).

  The legs cover the full M7 acceptance surface (spec work/m7_spec.md N7):
    Travel  - client login -> district travel round-trip   (TRAVEL_GATE_OK)
    Ticket  - single-redeem district ticket mint/validate  (DISTRICT_TICKET_GATE_OK)
    Handoff - char handoff + tamper rejection              (HANDOFF_GATE_OK)
    Chat    - two-client in-district + cross-district chat  (CHAT_GATE_OK)
    Relay   - relay restart -> district client reconnect    (RELAY_DISTRICT_CLIENT_GATE_OK)

  Legs run STRICTLY sequentially: each spawns its own world+district+editor
  processes on the shared APB port contract, so parallel execution would collide.

  Default (no leg switch) runs all five. Individual legs may be selected via
  -Travel / -Ticket / -Handoff / -Chat / -Relay (any combination).
#>
param(
  [string]$Scratch = "$env:TEMP\apb_m7_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor  = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [switch]$Travel,
  [switch]$Ticket,
  [switch]$Handoff,
  [switch]$Chat,
  [switch]$Relay
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

# Ordered leg table: name -> { script, marker }. Order follows the M7 spine.
$legs = [ordered]@{
  Travel  = @{ Script = "run_m7_travel_gate.ps1";          Marker = "TRAVEL_GATE_OK" }
  Ticket  = @{ Script = "run_m7_ticket_gate.ps1";          Marker = "DISTRICT_TICKET_GATE_OK" }
  Handoff = @{ Script = "run_m7_handoff_gate.ps1";         Marker = "HANDOFF_GATE_OK" }
  Chat    = @{ Script = "run_m7_chat_gate.ps1";            Marker = "CHAT_GATE_OK" }
  Relay   = @{ Script = "run_m7_district_client_gate.ps1"; Marker = "RELAY_DISTRICT_CLIENT_GATE_OK" }
}

$selected = @($legs.Keys | Where-Object { (Get-Variable $_ -ValueOnly) })
$runAll = ($selected.Count -eq 0)

function Invoke-Leg([string]$Name, [hashtable]$Leg) {
  $script = Join-Path $PSScriptRoot $Leg.Script
  if (-not (Test-Path $script)) { throw "leg_missing:$Name ($($Leg.Script))" }
  $legScratch = Join-Path $Scratch $Name.ToLower()
  New-Item -ItemType Directory -Force -Path $legScratch | Out-Null
  $legLog = Join-Path $legScratch "leg_run.log"
  Write-Host "===== M7 leg: $Name -> $($Leg.Script) ====="

  & powershell -NoProfile -ExecutionPolicy Bypass -File $script `
      -Scratch $legScratch -Project $Project -Editor $Editor 2>&1 |
    Tee-Object -FilePath $legLog
  $code = $LASTEXITCODE

  $raw = Get-Content $legLog -Raw -ErrorAction SilentlyContinue
  $hasMarker = ($null -ne $raw) -and ($raw -match [regex]::Escape($Leg.Marker))
  if ($code -ne 0)     { throw "leg_failed:$Name (exit=$code)" }
  if (-not $hasMarker) { throw "leg_marker_missing:$Name ($($Leg.Marker))" }
  Write-Host "M7 leg OK: $Name ($($Leg.Marker))"
}

$failure = $null
try {
  foreach ($name in $legs.Keys) {
    if ($runAll -or ($selected -contains $name)) {
      Invoke-Leg $name $legs[$name]
    }
  }
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
}

if ($failure) {
  Write-Host "M7_TRAVEL_GATE_FAIL $failure"
  exit 1
}

Write-Host "M7_TRAVEL_GATE_OK"
exit 0
