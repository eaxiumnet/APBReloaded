# run_m16_persistence_gate.ps1 - M16 (Task 16): persistence + restart-safe travel gate.
# EDITOR-FREE. Proves: S1 clean-bootstrap allow-list, S4 -Clean refusal without -Force
# (+ -Force positive control), and S2/S3/S5 domain restart-parity / write-once / corrupt-
# reject via the already-built APBPersistenceTests.exe. Emits M16_PERSISTENCE_GATE_OK.
param(
  [string]$Scratch = "$env:TEMP\apb_m16_persistence_gate"
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$failure = $null
$bootstrapScript = Join-Path $PSScriptRoot 'scripts\bootstrap_server.ps1'
$persistExe = 'D:\APBReloaded\Binaries\Win64\APBPersistenceTests.exe'

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

try {
  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  if (-not (Test-Path -LiteralPath $bootstrapScript)) { Fail 'bootstrap_script_missing' }
  if (-not (Test-Path -LiteralPath $persistExe)) { Fail 'persistence_exe_missing' }

  # S1: clean-bootstrap allow-list -- -DryRun must report BOOTSTRAP_CLEAN_OK and mutate nothing.
  $s1Persist = Join-Path $Scratch 'Saved\persist_s1'
  New-Item -ItemType Directory -Force -Path $s1Persist | Out-Null
  $s1Output = & powershell -NoProfile -ExecutionPolicy Bypass -File $bootstrapScript `
      -PersistDir $s1Persist -DryRun 2>&1
  if ($LASTEXITCODE -ne 0) { Fail "s1_bootstrap_dryrun_exit_$LASTEXITCODE" }
  if (($s1Output -join "`n") -notmatch 'BOOTSTRAP_CLEAN_OK') { Fail 's1_bootstrap_clean_ok_not_printed' }
  $s1Items = @(Get-ChildItem -LiteralPath $s1Persist -Recurse -ErrorAction SilentlyContinue)
  if ($s1Items.Count -ne 0) { Fail "s1_dryrun_mutated_persist_count_$($s1Items.Count)" }
  Write-Host 'M16_S1_BOOTSTRAP_ALLOWLIST_OK'

  # S4a: -Clean WITHOUT -Force must refuse (zero FS mutation) and print CLEAN_REFUSED.
  $s4Persist = Join-Path $Scratch 'Saved\persist_s4'
  New-Item -ItemType Directory -Force -Path $s4Persist | Out-Null
  $dummyFile = Join-Path $s4Persist 'accounts.json'
  Set-Content -LiteralPath $dummyFile -Value '{"accounts":[]}'
  $s4aOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $bootstrapScript `
      -PersistDir $s4Persist -Clean 2>&1
  if ($LASTEXITCODE -ne 0) { Fail "s4a_clean_refused_exit_$LASTEXITCODE" }
  if (-not (Test-Path -LiteralPath $dummyFile)) { Fail 's4a_state_deleted_without_force' }
  if (($s4aOutput -join "`n") -notmatch 'CLEAN_REFUSED') { Fail 's4a_clean_refused_marker_missing' }
  Write-Host 'M16_S4_CLEAN_REFUSED_OK'

  # S4b: -Clean -Force positive control -- state IS deleted and CLEAN_REFUSED must NOT print.
  $s4bOutput = & powershell -NoProfile -ExecutionPolicy Bypass -File $bootstrapScript `
      -PersistDir $s4Persist -Clean -Force 2>&1
  if ($LASTEXITCODE -ne 0) { Fail "s4b_clean_force_exit_$LASTEXITCODE" }
  if (Test-Path -LiteralPath $dummyFile) { Fail 's4b_state_not_deleted_with_force' }
  if (($s4bOutput -join "`n") -match 'CLEAN_REFUSED') { Fail 's4b_force_wrongly_printed_clean_refused' }
  Write-Host 'M16_S4_CLEAN_FORCE_OK'

  # S2/S3/S5: domain restart-parity (H: next_id > pre-restart max), write-once (I: one
  # snapshot per logout), corrupt-reject (F: partial/corrupt JSON rejected, good files
  # preserved) -- all proven by the already-built durability suite. Require exit 0 + FAILS=0.
  $exeOutput = & $persistExe 2>&1
  $exeExit = $LASTEXITCODE
  $exeText = $exeOutput -join "`n"
  if ($exeExit -ne 0) { Fail "persistence_exe_exit_$exeExit" }
  if ($exeText -notmatch 'FAILS=0') { Fail 'persistence_exe_not_fails_0' }
  if ($exeText -match 'FAILS=[1-9]') { Fail 'persistence_exe_reported_failures' }
  Write-Host 'M16_S2_RESTART_PARITY_OK'
  Write-Host 'M16_S3_WRITE_ONCE_OK'
  Write-Host 'M16_S5_CORRUPT_REJECT_OK'
} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
}

if ($failure) {
  Write-Host "M16_PERSISTENCE_GATE_FAIL $failure"
  exit 1
}

Write-Host 'M16_PERSISTENCE_GATE_OK'
exit 0
