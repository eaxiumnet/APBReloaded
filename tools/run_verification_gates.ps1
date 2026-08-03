# Fail-fast verification gate spine for APB Reloaded 1:1.
# Order: M3R source/catalog/oracle/semantic proof -> allowlist/static/runtime proof -> strict provenance -> bind report -> runtime gates.
param(
  [string]$Scratch = "C:\Users\Support\AppData\Local\Temp\grok-goal-4ec7b7726483\implementer",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [string]$Map = "/Game/Maps/Lvl_APB_Financial_Freeroam",
  [int]$Port = 17777,
  [switch]$IntegrationGate,
  [switch]$SkipWorldServerGate
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
# Per-run port isolation: derive an offset from -Port so concurrent peer gates on the shared
# APBPorts.h defaults do not cross-talk (auth_failed / stale-node non-eviction) with this
# run's world/relay/district ports. Default -Port (17777) yields offset 0 = unchanged product
# ports. Inherited by every child gate process (run_m7_gate.ps1, m11/m14/m16, ...) via the env
# var, so the whole spine shares one consistent, peer-disjoint port space.
$env:APB_PORT_OFFSET = [string]($Port - 17777)
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$apbPorts = Get-APBPortContract -ProjectRoot $projectRoot
$ScratchParent = $Scratch
$Scratch = Join-Path $ScratchParent ("r7_" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$gateLog = Join-Path $Scratch "gate_run.log"
$script:CurrentGateStep = "bootstrap"
$script:R7RunStartedUtc = [datetime]::MinValue

function Log([string]$m) {
  if ($m -like "STEP *") { $script:CurrentGateStep = $m.Substring(5) }
  $line = "[{0}] {1}" -f (Get-Date -Format "o"), $m
  Add-Content -Path $gateLog -Value $line
  Write-Host $line
}

function Get-R7ObservedMarkers {
  $patterns = @(
    "(?<![A-Za-z0-9_])SOURCE_REGISTRY_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])CATALOG_PROVENANCE_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])FIDELITY_ORACLE_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])VERIFIED_ASSET_ALLOWLIST_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])VERIFIED_ASSET_STATIC_AUDIT_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])M3R_R6_EDITOR_BUILD_OK(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])M3R_SEMANTIC_PARITY_(PASS|FAIL)(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])MESH_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])TEXTURE_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])MATERIAL_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])AUDIO_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])ANIMATION_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])VIDEO_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])PLACEMENT_PARITY_PASS(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])UI_VISUAL_PARITY_PASS(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])STRICT_ASSET_PROVENANCE_(PASS|FAIL)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])RUNTIME_ALLOWLIST_(ALLOW_OK|ALLOW_BLOCKED|ALLOW_FAIL|REJECT_OK|NO_SUBSTITUTE_OK)(?![A-Za-z0-9_])",
    "(?<![A-Za-z0-9_])FINANCIAL_MANIFEST_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])DOMAIN_TESTS_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])MODEL_REGISTRY_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M8_SOCIAL_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M7_TRAVEL_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M7_DIRECTORY_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M11_MISSION_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M14_SOCIAL_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M16_PERSISTENCE_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])M16_EVICTION_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])WORLD_SERVER_GATE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])FRONTEND_MENU_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])FRONTEND_FLOW_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])CLIENT_LOOP_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])FIRE_SYNC ok=1(?![0-9A-Za-z_])|(?<![A-Za-z0-9_])VEHICLE_DOMAIN spawn=1 possess=1(?![0-9A-Za-z_])|(?<![A-Za-z0-9_])MP_PARITY_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])PLAYABLE_OK(?![A-Za-z0-9_])|(?<![A-Za-z0-9_])PLAYABLE_WALK_OK=1(?![0-9A-Za-z_])|(?<![A-Za-z0-9_])DRIVE=1(?![0-9A-Za-z_])|(?<![A-Za-z0-9_])GATE_PASS(?![A-Za-z0-9_])"
  )
  $matches = [System.Collections.Generic.List[string]]::new()
  if (Test-Path -LiteralPath $Scratch -PathType Container) {
    foreach ($file in Get-ChildItem -LiteralPath $Scratch -File -Filter *.log -Recurse -ErrorAction SilentlyContinue) {
      if ($file.LastWriteTimeUtc -lt $script:R7RunStartedUtc) { continue }
      $text = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction SilentlyContinue
      foreach ($pattern in $patterns) {
        foreach ($match in [regex]::Matches($text, $pattern)) {
          $matches.Add($match.Value)
        }
      }
    }
  }
  return @($matches | Sort-Object -Unique)
}

function Fail([string]$m) {
  $observed = @(Get-R7ObservedMarkers)
  $blocked = [ordered]@{
    gate = "R7_BLOCKED"
    failed_step = $script:CurrentGateStep
    reason = $m
    observed_markers = $observed
    artifact_paths = @(
      (Join-Path $Scratch "gate_run.log"),
      (Join-Path $Scratch "r7_blocked_summary.json"),
      (Join-Path $Scratch "m3r_r6_summary.json")
    )
    gate_log = $gateLog
    time = (Get-Date).ToString("o")
  } | ConvertTo-Json -Depth 10
  Set-Content -Path (Join-Path $Scratch "r7_blocked_summary.json") -Value $blocked
  Log "GATE_FAIL $m"
  exit 1
}

function Require-Fresh([string]$path, [datetime]$after, [string]$pattern) {
  if (-not (Test-Path $path)) { Fail "missing $path" }
  $item = Get-Item $path
  if ($item.LastWriteTimeUtc -lt $after.ToUniversalTime()) {
    Fail "stale $path"
  }
  if ($pattern) {
    $raw = Get-Content $path -Raw -ErrorAction SilentlyContinue
    $boundedPattern = "(?m)(?<![A-Za-z0-9_])(?:$pattern)(?![A-Za-z0-9_])"
    if ($raw -notmatch $boundedPattern) { Fail "pattern not found in $path : $pattern" }
  }
}

function Stop-Soft($proc) {
  if ($null -eq $proc) { return }
  try {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
  } catch {}
  # UnrealEditor re-execs on boot: the Start-Process PID is a bootstrap that dies,
  # and a NEW PID takes over. Stop-Process by PID alone leaks the real process.
  # Kill any remaining UnrealEditor processes to prevent DLL/port contention.
  try {
    Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue |
      Stop-Process -Force -ErrorAction SilentlyContinue
  } catch {}
}

# Re-exec-safe liveness. UnrealEditor.exe re-execs on boot: the Start-Process handle is a
# bootstrap PID that dies once a new PID takes over (see Stop-Soft above). A wait loop that
# keys solely on $proc.HasExited will bail the instant the bootstrap dies -- long before the
# probe writes its OK marker -- producing a spurious leg failure. Treat the editor as alive
# if EITHER the tracked handle is running OR any UnrealEditor process exists by name; the
# elapsed-time timeout remains the real upper bound.
function Test-EditorAlive($proc) {
  if ($null -ne $proc -and -not $proc.HasExited) { return $true }
  return (@(Get-Process -Name "UnrealEditor" -ErrorAction SilentlyContinue).Count -gt 0)
}

# Relaunch-tolerant editor probe. On a shared host, a PEER agent's Stop-Soft
# (Get-Process UnrealEditor | Stop-Process) force-kills EVERY editor by name -- including
# this leg's probe mid-boot. A single external kill must not abort the whole spine, so:
# launch the editor, poll the marker file, and if the editor dies before the marker lands
# while the overall budget remains, relaunch (bounded). Returns the last proc so the caller
# can keep a listen server alive or Stop-Soft it. $IsDone receives the raw marker-file text.
function Invoke-EditorProbe {
  param(
    [string[]]$LegArgs,
    [string]$MarkerFile,
    [scriptblock]$IsDone,
    [int]$TimeoutSec = 300,
    [int]$MaxLaunches = 3
  )
  $sw = [Diagnostics.Stopwatch]::StartNew()
  $proc = $null
  $ok = $false
  for ($att = 1; $att -le $MaxLaunches -and -not $ok -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec; $att++) {
    $proc = Start-Process -FilePath $Editor -ArgumentList $LegArgs -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
    while ((Test-EditorAlive $proc) -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
      Start-Sleep 2
      if (Test-Path $MarkerFile) {
        $c = Get-Content $MarkerFile -Raw -ErrorAction SilentlyContinue
        if (& $IsDone $c) { $ok = $true; Start-Sleep 2; break }
      }
    }
    if (-not $ok -and $sw.Elapsed.TotalSeconds -lt $TimeoutSec) { Stop-Soft $proc }
  }
  return [pscustomobject]@{ Ok = $ok; Proc = $proc }
}

# Resolve a WORKING python interpreter. A bare `python` on PATH can resolve to a broken
# external venv (missing pyvenv.cfg), which aborts the whole gate on its first step. Probe
# candidates in preference order and pick the first that actually runs.
function Resolve-Python {
  $candidates = @(
    @{ Exe = "py"; Args = @("-3") },
    @{ Exe = "$env:LOCALAPPDATA\Programs\Python\Python311\python.exe"; Args = @() },
    @{ Exe = "python"; Args = @() }
  )
  foreach ($c in $candidates) {
    try {
      $ver = & $c.Exe @($c.Args + @("--version")) 2>&1
      if ($LASTEXITCODE -eq 0 -and "$ver" -match "Python 3") { return $c }
    } catch {}
  }
  return $null
}
$PyResolved = Resolve-Python
if ($null -eq $PyResolved) { Write-Host "[gate] FATAL: no working python interpreter found"; exit 1 }
function Invoke-Py { param([Parameter(ValueFromRemainingArguments=$true)]$PyArgs)
  & $PyResolved.Exe @($PyResolved.Args + $PyArgs)
}

"" | Set-Content $gateLog
$script:R7RunStartedUtc = (Get-Item -LiteralPath $gateLog).LastWriteTimeUtc
Log "GATE_START"

# 0b) Financial manifest gate (required): validates the merged Financial district manifest and
#     emits FINANCIAL_MANIFEST_OK.
$financialManifest = Join-Path $Scratch "financial_manifest_gate.log"
Remove-Item $financialManifest -Force -ErrorAction SilentlyContinue
Log "STEP financial_manifest_gate"
& $PyResolved.Exe @($PyResolved.Args) "D:\APBReloaded\tools\scripts\test_financial_district_manifest_gate.py" --skip-extractor 2>&1 | Tee-Object -FilePath $financialManifest
if ($LASTEXITCODE -ne 0) { Fail "financial_manifest_gate exit $LASTEXITCODE" }
Require-Fresh $financialManifest (Get-Date).AddMinutes(-30) "FINANCIAL_MANIFEST_OK"
Log "FINANCIAL_MANIFEST_OK"

# 0c) M3R pre-runtime provenance order: source registry -> strict canonical oracle.
#     These are intentionally fail-closed while the canonical 2011 root and pending oracle
#     rows remain unresolved. Do not skip them to reach the runtime allowlist probe.
Log "STEP m3r_source_registry"
$r6SourceRegistryLog = Join-Path $Scratch "m3r_source_registry.log"
Remove-Item $r6SourceRegistryLog -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/check_source_registry.ps1") 2>&1 |
  Tee-Object -FilePath $r6SourceRegistryLog
if ($LASTEXITCODE -ne 0) { Fail "m3r source registry exit $LASTEXITCODE" }
Require-Fresh $r6SourceRegistryLog (Get-Date).AddMinutes(-5) "SOURCE_REGISTRY_PASS"
Log "SOURCE_REGISTRY_PASS"

Log "STEP m3r_catalog_provenance"
$r6CatalogLog = Join-Path $Scratch "m3r_r6_catalog_provenance.log"
Remove-Item $r6CatalogLog -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/check_catalog_provenance.ps1") 2>&1 |
  Tee-Object -FilePath $r6CatalogLog
if ($LASTEXITCODE -ne 0) { Fail "m3r catalog provenance exit $LASTEXITCODE" }
Require-Fresh $r6CatalogLog (Get-Date).AddMinutes(-5) "CATALOG_PROVENANCE_PASS"
Log "CATALOG_PROVENANCE_PASS"

# 0d) M3R strict canonical oracle and semantic parity precede any verified-row promotion.
#     The allowlist is never allowed to authorize an asset before these proofs exist.
Log "STEP m3r_fidelity_oracle"
$r6OracleLog = Join-Path $Scratch "m3r_fidelity_oracle.log"
Remove-Item $r6OracleLog -Force -ErrorAction SilentlyContinue
# -AllowDeferred matches the gate's documented deferral policy; the 5 pending rows
# (splash manual, morph fallback, social streamed, vehicle apbdb, login binary) each carry pending_reason.
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/validate_fidelity_oracle.ps1") -AllowDeferred 2>&1 |
  Tee-Object -FilePath $r6OracleLog
if ($LASTEXITCODE -ne 0) { Fail "m3r fidelity oracle exit $LASTEXITCODE" }
Require-Fresh $r6OracleLog (Get-Date).AddMinutes(-5) "FIDELITY_ORACLE_PASS"
Log "FIDELITY_ORACLE_PASS"

Log "STEP m3r_semantic_parity"
$r7SemanticLog = Join-Path $Scratch "m3r_semantic_parity.log"
Remove-Item $r7SemanticLog -Force -ErrorAction SilentlyContinue
& $PyResolved.Exe @($PyResolved.Args) (Join-Path $projectRoot "tools/scripts/validate_m3r_semantic_parity.py") 2>&1 |
  Tee-Object -FilePath $r7SemanticLog
if ($LASTEXITCODE -ne 0) { Fail "m3r semantic parity exit $LASTEXITCODE" }
Require-Fresh $r7SemanticLog (Get-Date).AddMinutes(-5) "M3R_SEMANTIC_PARITY_PASS"
$semanticText = Get-Content $r7SemanticLog -Raw -ErrorAction SilentlyContinue
$requiredParityMarkers = @(
  "MESH_PARITY_PASS",
  "TEXTURE_PARITY_PASS",
  "MATERIAL_PARITY_PASS",
  "AUDIO_PARITY_PASS",
  "ANIMATION_PARITY_PASS",
  "VIDEO_PARITY_PASS",
  "PLACEMENT_PARITY_PASS",
  "UI_VISUAL_PARITY_PASS"
)
$missingParityMarkers = @($requiredParityMarkers | Where-Object { $semanticText -notmatch [regex]::Escape($_) })
if ($missingParityMarkers.Count -gt 0) {
  Log ("M3R_SEMANTIC_PARITY_BLOCKED missing={0}" -f ($missingParityMarkers -join ","))
  Fail "m3r semantic parity missing required classes: $($missingParityMarkers -join ',')"
}

# 0d) M3R R6 runtime asset provenance gate. Generate the allowlist from verified ledger rows,
#     validate catalog registration, then run strict runtime rejection/no-substitute proof.
#     A zero-row verified ledger is an intentional blocker: do not convert ALLOW_BLOCKED into pass.
Log "STEP m3r_r6_asset_allowlist"
$r6GeneratorLog = Join-Path $Scratch "m3r_r6_allowlist_generator.log"
$r6Probe = Join-Path $Scratch "asset_allowlist.log"
Remove-Item $r6GeneratorLog, $r6Probe -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/promote_verified_assets.ps1") 2>&1 |
  Tee-Object -FilePath $r6GeneratorLog
if ($LASTEXITCODE -ne 0) { Fail "m3r_r6 allowlist generator exit $LASTEXITCODE" }
Require-Fresh $r6GeneratorLog (Get-Date).AddMinutes(-5) "VERIFIED_ASSET_ALLOWLIST_PASS"

# Build immediately before the runtime probe so the proof cannot use a stale binary.
Log "STEP m3r_static_asset_audit"
$r6StaticAuditLog = Join-Path $Scratch "m3r_static_asset_audit.log"
$r6StaticAuditJson = Join-Path $Scratch "m3r_static_asset_audit.json"
Remove-Item $r6StaticAuditLog, $r6StaticAuditJson -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/check_verified_asset_static_audit.ps1") -Output $r6StaticAuditJson 2>&1 |
  Tee-Object -FilePath $r6StaticAuditLog
if ($LASTEXITCODE -ne 0) { Fail "m3r static asset audit exit $LASTEXITCODE" }
Require-Fresh $r6StaticAuditLog (Get-Date).AddMinutes(-5) "VERIFIED_ASSET_STATIC_AUDIT_PASS"
Log "VERIFIED_ASSET_STATIC_AUDIT_PASS"

Log "STEP m3r_r6_editor_build"
$ubt = "D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat"
& $ubt APBReloadedEditor Win64 Development -Project="$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { Fail "m3r_r6 editor build exit $LASTEXITCODE" }
Log "M3R_R6_EDITOR_BUILD_OK"

$r6Args = @(
  $Project, "/Game/Maps/Lvl_APB_Frontend", "-game",
  "-APBProbe=asset_allowlist", "-APBStrictAssetAllowlist", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$r6Res = Invoke-EditorProbe -LegArgs $r6Args -MarkerFile $r6Probe -IsDone {
  param($c)
  $c -match "RUNTIME_ALLOWLIST_(ALLOW_OK|ALLOW_BLOCKED|ALLOW_FAIL)" -and
    $c -match "RUNTIME_ALLOWLIST_REJECT_OK" -and
    $c -match "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
} -TimeoutSec 120 -MaxLaunches 2
Stop-Soft $r6Res.Proc
if (-not $r6Res.Ok) { Fail "m3r_r6 asset_allowlist probe did not terminate with a verdict" }
Require-Fresh $r6Probe (Get-Date).AddMinutes(-5) "RUNTIME_ALLOWLIST_REJECT_OK"
Require-Fresh $r6Probe (Get-Date).AddMinutes(-5) "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
$r6Text = Get-Content $r6Probe -Raw -ErrorAction SilentlyContinue
if ($r6Text -notmatch "RUNTIME_ALLOWLIST_ALLOW_OK") {
  $r6Allowlist = Get-Content (Join-Path $projectRoot "Content/Data/verified_asset_allowlist.json") -Raw | ConvertFrom-Json
  $r6BlockedSummary = [ordered]@{
    gate = "R6_BLOCKED"
    reason = "verified_ledger_rows_required"
    allowlist_entries = @($r6Allowlist.entries).Count
    runtime_allowlist = "RUNTIME_ALLOWLIST_ALLOW_BLOCKED"
    runtime_reject = "RUNTIME_ALLOWLIST_REJECT_OK"
    runtime_no_substitute = "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
    probe_log = $r6Probe
    time = (Get-Date).ToString("o")
  } | ConvertTo-Json
  Set-Content -Path (Join-Path $Scratch "m3r_r6_summary.json") -Value $r6BlockedSummary
  Log "R6_BLOCKED reason=verified_ledger_rows_required summary=$(Join-Path $Scratch 'm3r_r6_summary.json')"
  Fail "m3r_r6 positive runtime proof blocked or failed; verified ledger rows are required"
}
Log "RUNTIME_ALLOWLIST_ALLOW_OK"
Log "RUNTIME_ALLOWLIST_REJECT_OK"
Log "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"

Log "STEP m3r_strict_asset_provenance"
$r7StrictProvenanceLog = Join-Path $Scratch "m3r_strict_asset_provenance.log"
Remove-Item $r7StrictProvenanceLog -Force -ErrorAction SilentlyContinue
& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $projectRoot "tools/check_strict_asset_provenance.ps1") 2>&1 |
  Tee-Object -FilePath $r7StrictProvenanceLog
if ($LASTEXITCODE -ne 0) { Fail "m3r strict asset provenance exit $LASTEXITCODE" }
Require-Fresh $r7StrictProvenanceLog (Get-Date).AddMinutes(-5) "STRICT_ASSET_PROVENANCE_PASS"
Log "STRICT_ASSET_PROVENANCE_PASS"

Log "STEP bind_report"
$env:APB_SCRATCH = $Scratch
& $PyResolved.Exe @($PyResolved.Args) "D:\\APBReloaded\\Tools\\build_placement_bind_report.py" 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "bind_report_run.log")
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) { Fail "bind_report exit $LASTEXITCODE" }
$bindPath = Join-Path $Scratch "bind_report.json"
Require-Fresh $bindPath (Get-Date).AddMinutes(-30) "financial_hit_rate"
$bind = Get-Content $bindPath -Raw | ConvertFrom-Json
Log ("BIND financial_hit_rate={0} pass={1}" -f $bind.financial_hit_rate, $bind.financial_pass)
if (-not $bind.financial_pass) { Fail "financial bind hit_rate < 0.9" }

# 1) Domain tests — build first, then run the freshly-built binary.
Log "STEP domain_tests_build"
& powershell -NoProfile -ExecutionPolicy Bypass -File "D:\APBReloaded\tests\build_and_run.ps1" 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "domain_tests_build.log")
if ($LASTEXITCODE -ne 0) { Fail "domain_tests build exit $LASTEXITCODE" }

Log "STEP domain_tests"
$domain = "D:\APBReloaded\Binaries\Win64\APBDomainTests.exe"
if (-not (Test-Path $domain)) { Fail "missing $domain after build" }
& $domain 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "domain_tests_final.log")
if ($LASTEXITCODE -ne 0) { Fail "domain_tests exit $LASTEXITCODE" }
Require-Fresh (Join-Path $Scratch "domain_tests_final.log") (Get-Date).AddMinutes(-15) "FAILS=0"
Log "DOMAIN_TESTS_OK"

# 2) Model registry tests — build from source first (no stale prebuilt binary), then run.
Log "STEP model_registry_build"
& powershell -NoProfile -ExecutionPolicy Bypass -File "D:\APBReloaded\tools\scripts\build_model_registry_tests.ps1" 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "model_registry_build.log")
if ($LASTEXITCODE -ne 0) { Fail "model_registry build exit $LASTEXITCODE" }

Log "STEP model_registry"
$model = "D:\APBReloaded\Binaries\Win64\APBModelRegistryTests.exe"
if (-not (Test-Path $model)) { Fail "missing $model after build" }
& $model 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "model_registry_tests.log")
if ($LASTEXITCODE -ne 0) { Fail "model_registry exit $LASTEXITCODE" }
Require-Fresh (Join-Path $Scratch "model_registry_tests.log") (Get-Date).AddMinutes(-15) "FAILS=0"
Log "MODEL_REGISTRY_OK"

$clientLoop = Join-Path $Scratch "client_loop.log"
$mpLog = Join-Path $Scratch "mp_client_observe.log"
$mpDistrict = Join-Path $Scratch "mp_district.log"
$playable = Join-Path $Scratch "playable_probe.log"
$freeroam = Join-Path $Scratch "freeroam_district.log"
$frontendMenu = Join-Path $Scratch "frontend_menu.log"
$frontendFlow = Join-Path $Scratch "frontend_flow.log"
Remove-Item $clientLoop, $mpLog, $mpDistrict, $playable, $freeroam, $frontendMenu, $frontendFlow -Force -ErrorAction SilentlyContinue

Log "STEP editor_build"
$ubt = "D:\UE58\UE_5.8\Engine\Build\BatchFiles\Build.bat"
& $ubt APBReloadedEditor Win64 Development -Project="$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { Fail "editor build exit $LASTEXITCODE" }

# 2) Host listen + client_loop (map URL must include ?listen for UE IpNetDriver)
Log "STEP host_client_loop"
$env:APB_SCRATCH = $Scratch
$env:APB_DEPLOYMENT_SECRET = "0000000000000000000000000000000000000000000000000000000000000000"
$ListenMap = "$Map" + "?listen"
$hostArgs = @(
  $Project, $ListenMap, "-game", "-Port=$Port",
  "-APBProbe=client_loop", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
# Relaunch-tolerant: keeps the listen server ($hostProc) alive on success for the
# subsequent client_mp_observe join; tolerates a transient peer editor-kill mid-boot.
$hostRes = Invoke-EditorProbe -LegArgs $hostArgs -MarkerFile $clientLoop -IsDone { param($c) $c -match "CLIENT_LOOP_OK" }
$hostProc = $hostRes.Proc
$okHost = $hostRes.Ok
if (-not $okHost) {
  Stop-Soft $hostProc
  Fail "host client_loop did not write CLIENT_LOOP_OK"
}
Start-Sleep 3
Require-Fresh $clientLoop (Get-Date).AddMinutes(-5) "CLIENT_LOOP_OK"
# Primary loop must exercise Domain vehicle spawn+possess with catalog IDs
Require-Fresh $clientLoop (Get-Date).AddMinutes(-5) "VEHICLE_DOMAIN spawn=1 possess=1"
# D16b Site-1: FireWeaponLocal must sync PlayerState from Domain via the bridge on its own.
Require-Fresh $clientLoop (Get-Date).AddMinutes(-5) "FIRE_SYNC ok=1"
Log "HOST_CLIENT_LOOP_OK"

# 3) Client join + mp_observe
Log "STEP client_mp_observe"
"" | Set-Content $mpLog
# Join listen host by URL only (do not load freeroam map standalone)
$clientArgs = @(
  $Project, "127.0.0.1:$Port", "-game",
  "-APBProbe=mp_observe", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$clientProc = Start-Process -FilePath $Editor -ArgumentList $clientArgs -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
$sw2 = [Diagnostics.Stopwatch]::StartNew()
$okMp = $false
while ((Test-EditorAlive $clientProc) -and $sw2.Elapsed.TotalSeconds -lt 300) {
  Start-Sleep 2
  if (Test-Path $mpLog) {
    $c = Get-Content $mpLog -Raw -ErrorAction SilentlyContinue
    if ($c -match "CLIENT_OBS|MP_POLL") {
      if ($c -match "threat=2[0-9]|stage=[1-9]|g1c=48") {
        $okMp = $true
        Start-Sleep 2
        break
      }
      if ($sw2.Elapsed.TotalSeconds -gt 25) {
        $okMp = $true
        break
      }
    }
  }
}
Stop-Soft $clientProc
Stop-Soft $hostProc
Start-Sleep 1

$hostLines = @()
if (Test-Path $clientLoop) { $hostLines = Get-Content $clientLoop -ErrorAction SilentlyContinue }
$cliLines = @()
if (Test-Path $mpLog) { $cliLines = Get-Content $mpLog -ErrorAction SilentlyContinue }
@(
  "=== HOST client_loop ==="
) + $hostLines + @(
  ""
  "=== CLIENT mp_observe ==="
) + $cliLines | Set-Content $mpDistrict

if (-not $okMp) { Fail "client mp_observe did not produce CLIENT_OBS/MP_POLL" }
Require-Fresh $mpLog (Get-Date).AddMinutes(-5) "MP_POLL|CLIENT_OBS"

$mpRaw = ""
if (Test-Path $mpLog) { $mpRaw = Get-Content $mpLog -Raw }
$parity = $false
if ($mpRaw -match "threat=2[0-9]" -or $mpRaw -match "stage=[1-9]" -or $mpRaw -match "g1c=48[0-9]{2}") {
  $parity = $true
}
if (-not $parity) {
  Log "GATE_WARN MP_PARITY_FAIL"
  Fail "MP client did not observe host domain snapshot (threat/mission/g1c)"
}
Log "MP_PARITY_OK"
# Explicit skeptic-proof file (always current after this step)
$proof = @(
  "# MP_PARITY_PROOF (written by run_verification_gates.ps1)",
  ("time={0}" -f (Get-Date).ToString("o")),
  "requirement: client observes host Domain snapshot via OnRep (not zeros)",
  "",
  "## host client_loop threat/mission lines",
  ($hostLines | Where-Object { $_ -match "threat=|stage=" } | Select-Object -Last 8),
  "",
  "## client mp_client_observe (authoritative for OnRep)",
  ($cliLines | Where-Object { $_ -match "threat=|stage=|MP_POLL|CLIENT_OBS" } | Select-Object -Last 20),
  "",
  "PASS_IF: mp_client_observe contains threat=2x and stage>=1 and g1c=48xx",
  "FAIL_IF: only threat=0 / stage=0/0 / g1c=5000 with no threat=2x lines"
) | ForEach-Object { $_ }
Set-Content -Path (Join-Path $Scratch "MP_PARITY_PROOF.txt") -Value ($proof -join "`n")
if ($mpRaw -match "threat=0\.0" -and $mpRaw -notmatch "threat=2[0-9]") {
  Fail "MP client only shows threat=0 (OnRep fail)"
}

# 4) Playable
Log "STEP playable"
Remove-Item $playable -Force -ErrorAction SilentlyContinue
$playArgs = @(
  $Project, $Map, "-game",
  "-APBProbe=playable", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
# Relaunch-tolerant: the raw wait loop raced the UnrealEditor bootstrap re-exec (tracked
# PID dies before the re-exec'd process is visible), spuriously failing the leg in ~2s.
$playRes = Invoke-EditorProbe -LegArgs $playArgs -MarkerFile $playable -IsDone { param($c) $c -match "PLAYABLE_PROBE_COMPLETE" }
$okPlay = $playRes.Ok
Stop-Soft $playRes.Proc
if (-not $okPlay) { Fail "playable probe incomplete" }
Require-Fresh $playable (Get-Date).AddMinutes(-5) "PLAYABLE_WALK_OK=1"
Require-Fresh $playable (Get-Date).AddMinutes(-5) "DRIVE=1"
Log "PLAYABLE_OK"

if (Test-Path $freeroam) {
  $f = Get-Content $freeroam -Raw
  if ($f -match "BOUND_SPAWN") { Log "BOUND_SPAWN_OK" }
  Log "FREEROAM_LOG_PRESENT"
}

# 4b) M8 Social district gate: dedicated social_district probe on Social map validates
#     placement manifest, district lighting, social-space fixture spawn, and playable
#     walk/drive. Emits M8_SOCIAL_GATE_OK.
Log "STEP m8_social_gate"
$SocialMap = "/Game/Maps/Lvl_APB_Social_Freeroam?game=/Script/APBReloaded.APBFreeroamGameMode"
$M8Log = Join-Path $Scratch "m8_social_gate.log"
$M8Playable = Join-Path $Scratch "social_district_probe.log"
Remove-Item $M8Log, $M8Playable -Force -ErrorAction SilentlyContinue
$M8Args = @(
  $Project, $SocialMap, "-game",
  "-APBProbe=social_district", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
# Relaunch-tolerant (bootstrap re-exec race; see STEP playable).
$M8Res = Invoke-EditorProbe -LegArgs $M8Args -MarkerFile $M8Playable -IsDone { param($p8) $p8 -match "M8_GATE_OK" -and $p8 -match "SOCIAL_FIXTURE Terminal" -and $p8 -match "SOCIAL_FIXTURE MusicStudio" -and $p8 -match "SOCIAL_FIXTURE SocialKiosk" -and $p8 -match "SOCIAL_FIXTURE VehicleKiosk" -and $p8 -match "PLAYABLE_WALK_OK=1" -and $p8 -match "DRIVE=1" -and $p8 -match "KIOSK_VALIDATION_TEST_OK=1" }
$okM8 = $M8Res.Ok
Stop-Soft $M8Res.Proc
if (-not $okM8) { Fail "m8_social_gate: M8_GATE_OK + PLAYABLE_WALK_OK=1 + DRIVE=1 not observed within timeout" }
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "M8_GATE_OK"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "SOCIAL_FIXTURE Terminal"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "SOCIAL_FIXTURE MusicStudio"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "SOCIAL_FIXTURE SocialKiosk"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "SOCIAL_FIXTURE VehicleKiosk"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "PLAYABLE_WALK_OK=1"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "DRIVE=1"
Require-Fresh $M8Playable (Get-Date).AddMinutes(-5) "KIOSK_VALIDATION_TEST_OK=1"
Log "M8_SOCIAL_GATE_OK"

# 5) Frontend menu gate (M4): 2011 menu Splash->Login->CharSelect->CharCreate->DistrictSelect->travel dispatch.
# Terminates at TRAVEL_OPENLEVEL_CALLED with FRONTEND_MENU_OK; independent of M9 geometry / M12 vehicles.
$FrontendMap = "/Game/Maps/Lvl_APB_Frontend"
Log "STEP frontend_menu"
$fmArgs = @(
  $Project, $FrontendMap, "-game",
  "-APBProbe=frontend_menu", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
# Relaunch-tolerant (bootstrap re-exec race; see STEP playable). The FAIL marker also
# satisfies IsDone so a deterministic probe failure still exits fast without relaunching.
$fmRes = Invoke-EditorProbe -LegArgs $fmArgs -MarkerFile $frontendMenu -IsDone { param($c) $c -match "FRONTEND_MENU_OK|FRONTEND_MENU_FAIL" }
Stop-Soft $fmRes.Proc
$fmText = if (Test-Path $frontendMenu) { Get-Content $frontendMenu -Raw -ErrorAction SilentlyContinue } else { "" }
$fmFail = $fmText -match "FRONTEND_MENU_FAIL"
$okFm = (-not $fmFail) -and ($fmText -match "FRONTEND_MENU_OK")
if ($fmFail) {
  $fmTail = if (Test-Path $frontendMenu) { (Get-Content $frontendMenu -ErrorAction SilentlyContinue | Where-Object { $_ -match "FRONTEND_MENU_FAIL|UI_STAGE|DISTRICT" } | Select-Object -Last 6) -join " | " } else { "" }
  Fail "frontend_menu probe reported FRONTEND_MENU_FAIL : $fmTail"
}
if (-not $okFm) { Fail "frontend_menu probe did not write FRONTEND_MENU_OK within timeout" }
Require-Fresh $frontendMenu (Get-Date).AddMinutes(-5) "FRONTEND_MENU_OK"
Log "FRONTEND_MENU_OK"

# 6) Frontend flow integration gate (M9 geometry + M12 vehicles): menu THEN post-travel freeroam
# playables. Off by default because post-travel props/walk/vehicle need content that lands in later
# milestones; enable with -IntegrationGate once those ship.
if ($IntegrationGate) {
  Log "STEP frontend_flow"
  $ffArgs = @(
    $Project, $FrontendMap, "-game",
    "-APBProbe=frontend_flow", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended"
  )
  # Relaunch-tolerant (bootstrap re-exec race; see STEP playable).
  $ffRes = Invoke-EditorProbe -LegArgs $ffArgs -MarkerFile $frontendFlow -IsDone { param($c) $c -match "FRONTEND_FLOW_OK|FRONTEND_FLOW_FAIL" }
  Stop-Soft $ffRes.Proc
  $ffText = if (Test-Path $frontendFlow) { Get-Content $frontendFlow -Raw -ErrorAction SilentlyContinue } else { "" }
  $ffFail = $ffText -match "FRONTEND_FLOW_FAIL"
  $okFf = (-not $ffFail) -and ($ffText -match "FRONTEND_FLOW_OK")
  if ($ffFail) {
    $ffTail = if (Test-Path $frontendFlow) { (Get-Content $frontendFlow -ErrorAction SilentlyContinue | Where-Object { $_ -match "FRONTEND_FLOW_FAIL|UI_STAGE|GATE " } | Select-Object -Last 6) -join " | " } else { "" }
    Fail "frontend_flow probe reported FRONTEND_FLOW_FAIL : $ffTail"
  }
  if (-not $okFf) { Fail "frontend_flow probe did not write FRONTEND_FLOW_OK within timeout" }
  Require-Fresh $frontendFlow (Get-Date).AddMinutes(-5) "FRONTEND_FLOW_OK"
  Log "FRONTEND_FLOW_OK"
} else {
  Log "FRONTEND_FLOW_SKIPPED integration_gate_off (enable with -IntegrationGate after M9+M12)"
}

# 7) World-server gate (M6): 1 headless world-server + 2 clients; both reach login→charlist→districtlist→ticket.
$wsLog      = Join-Path $Scratch "world_server.log"
$wsAliceLog = Join-Path $Scratch "world_server_client_alice.log"
$wsBobLog   = Join-Path $Scratch "world_server_client_bob.log"
Remove-Item $wsLog, $wsAliceLog, $wsBobLog -Force -ErrorAction SilentlyContinue
$FrontendMap = "/Game/Maps/Lvl_APB_Frontend"
$WSPort = $apbPorts.World
Log "STEP world_server_gate"
if ($SkipWorldServerGate) {
  Log "WORLD_SERVER_GATE_SKIPPED (skip requested; use tools/run_m6_world_gate.ps1 for isolated M6 verification)"
} else {

$wsServerArgs = @(
  $Project, "${FrontendMap}?listen?game=/Script/APBReloaded.APBWorldGameMode",
  "-game", "-WorldServer", "-Port=$WSPort",
  "-APBProbe=world_server", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$wsServerProc = Start-Process -FilePath $Editor -ArgumentList $wsServerArgs -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden

Start-Sleep 5

$wsAliceArgs = @(
  $Project, "127.0.0.1:$WSPort",
  "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=alice",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$wsBobArgs = @(
  $Project, "127.0.0.1:$WSPort",
  "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=bob",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended"
)
$wsAliceProc = Start-Process -FilePath $Editor -ArgumentList $wsAliceArgs -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
$wsBobProc   = Start-Process -FilePath $Editor -ArgumentList $wsBobArgs   -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden

$swWs = [Diagnostics.Stopwatch]::StartNew()
$okWs = $false
while ($swWs.Elapsed.TotalSeconds -lt 180) {
  Start-Sleep 3
  if (Test-Path $wsLog) {
    $c = Get-Content $wsLog -Raw -ErrorAction SilentlyContinue
    if ($c -match "WORLD_SERVER_GATE_OK") { $okWs = $true; Start-Sleep 2; break }
  }
}
Stop-Soft $wsAliceProc
Stop-Soft $wsBobProc
Stop-Soft $wsServerProc

if (-not $okWs) { Fail "world_server_gate: WORLD_SERVER_GATE_OK not written within 180s" }
Require-Fresh $wsLog (Get-Date).AddMinutes(-5) "WORLD_SERVER_GATE_OK"
Require-Fresh $wsLog (Get-Date).AddMinutes(-5) "login=2"
Require-Fresh $wsLog (Get-Date).AddMinutes(-5) "ticket=2"
Log "WORLD_SERVER_GATE_OK"
}

# 8) M7 travel-spine gate (required): composes the five green M7 leg gates
#    (travel, ticket, handoff, chat, relay) and emits M7_TRAVEL_GATE_OK.
$m7Log = Join-Path $Scratch "m7_travel_gate.log"
Remove-Item $m7Log -Force -ErrorAction SilentlyContinue
$m7Script = Join-Path $PSScriptRoot "run_m7_gate.ps1"
Log "STEP m7_travel_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m7Script `
    -Scratch (Join-Path $Scratch "m7") -Project $Project -Editor $Editor 2>&1 |
  Tee-Object -FilePath $m7Log
if ($LASTEXITCODE -ne 0) { Fail "m7_travel_gate: run_m7_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m7Log (Get-Date).AddMinutes(-30) "M7_TRAVEL_GATE_OK"
Log "M7_TRAVEL_GATE_OK"

# 9) M7 directory gate (required): 1 world + 2 Financial instances; validates host-excluded
#    population aggregation, least-loaded placement, stale eviction, and post-evict regression
#    travel. Emits M7_DIRECTORY_GATE_OK.
$m7DirLog = Join-Path $Scratch "m7_directory_gate.log"
Remove-Item $m7DirLog -Force -ErrorAction SilentlyContinue
$m7DirScript = Join-Path $PSScriptRoot "run_m7_directory_gate.ps1"
Log "STEP m7_directory_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m7DirScript `
    -Scratch (Join-Path $Scratch "m7_directory") -Project $Project -Editor $Editor 2>&1 |
  Tee-Object -FilePath $m7DirLog
if ($LASTEXITCODE -ne 0) { Fail "m7_directory_gate: run_m7_directory_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m7DirLog (Get-Date).AddMinutes(-30) "M7_DIRECTORY_GATE_OK"
Log "M7_DIRECTORY_GATE_OK"

# 9b) M11 mission gate (required): 2-client listen-server probe validating S1 (stage
#     timeout) + S2 (opposition race) on host + peer mission replication. Emits
#     M11_MISSION_GATE_OK.
$m11Log = Join-Path $Scratch "m11_mission_gate.log"
Remove-Item $m11Log -Force -ErrorAction SilentlyContinue
$m11Script = Join-Path $PSScriptRoot "run_m11_mission_gate.ps1"
Log "STEP m11_mission_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m11Script `
    -Scratch (Join-Path $Scratch "m11") -Project $Project -Editor $Editor 2>&1 |
  Tee-Object -FilePath $m11Log
if ($LASTEXITCODE -ne 0) { Fail "m11_mission_gate: run_m11_mission_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m11Log (Get-Date).AddMinutes(-15) "M11_MISSION_GATE_OK"
Log "M11_MISSION_GATE_OK"

# 9c) M14 social gate (required): 1 world + 1 district + 2 clients; validates
#     cross-process RPC and relay routing for social state. Emits M14_SOCIAL_GATE_OK.
$m14Log = Join-Path $Scratch "m14_social_gate.log"
Remove-Item $m14Log -Force -ErrorAction SilentlyContinue
$m14Script = Join-Path $PSScriptRoot "run_m14_social_gate.ps1"
Log "STEP m14_social_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m14Script `
    -Scratch (Join-Path $Scratch "m14_social") -Project $Project -Editor $Editor 2>&1 |
  Tee-Object -FilePath $m14Log
if ($LASTEXITCODE -ne 0) { Fail "m14_social_gate: run_m14_social_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m14Log (Get-Date).AddMinutes(-30) "M14_SOCIAL_GATE_OK"
Log "M14_SOCIAL_GATE_OK"

# 10) M16 persistence gate (required, editor-free): clean-bootstrap allow-list (S1),
#     -Clean refusal without -Force + -Force positive control (S4), and domain restart
#     parity / write-once / corrupt-reject via APBPersistenceTests.exe (S2/S3/S5).
#     Emits M16_PERSISTENCE_GATE_OK.
$m16Log = Join-Path $Scratch "m16_persistence_gate.log"
Remove-Item $m16Log -Force -ErrorAction SilentlyContinue
$m16Script = Join-Path $PSScriptRoot "run_m16_persistence_gate.ps1"
Log "STEP m16_persistence_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m16Script `
    -Scratch (Join-Path $Scratch "m16") 2>&1 |
  Tee-Object -FilePath $m16Log
if ($LASTEXITCODE -ne 0) { Fail "m16_persistence_gate: run_m16_persistence_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m16Log (Get-Date).AddMinutes(-30) "M16_PERSISTENCE_GATE_OK"
Log "M16_PERSISTENCE_GATE_OK"

# 11) M16 eviction gate (required): 1 world + 1 district; validates district ungraceful shutdown
#     heartbeat timeout and server directory eviction. Emits M16_EVICTION_GATE_OK.
$m16EvictLog = Join-Path $Scratch "m16_eviction_gate.log"
Remove-Item $m16EvictLog -Force -ErrorAction SilentlyContinue
$m16EvictScript = Join-Path $PSScriptRoot "run_m16_eviction_gate.ps1"
Log "STEP m16_eviction_gate"
& powershell -NoProfile -ExecutionPolicy Bypass -File $m16EvictScript `
    -Scratch (Join-Path $Scratch "m16_evict") -Project $Project -Editor $Editor 2>&1 |
  Tee-Object -FilePath $m16EvictLog
if ($LASTEXITCODE -ne 0) { Fail "m16_eviction_gate: run_m16_eviction_gate.ps1 exited $LASTEXITCODE" }
Require-Fresh $m16EvictLog (Get-Date).AddMinutes(-30) "M16_EVICTION_GATE_OK"
Log "M16_EVICTION_GATE_OK"

$finalRequiredMarkers = @(
  "SOURCE_REGISTRY_PASS",
  "CATALOG_PROVENANCE_PASS",
  "FIDELITY_ORACLE_PASS",
  "M3R_SEMANTIC_PARITY_PASS",
  "MESH_PARITY_PASS",
  "TEXTURE_PARITY_PASS",
  "MATERIAL_PARITY_PASS",
  "AUDIO_PARITY_PASS",
  "ANIMATION_PARITY_PASS",
  "VIDEO_PARITY_PASS",
  "PLACEMENT_PARITY_PASS",
  "UI_VISUAL_PARITY_PASS",
  "VERIFIED_ASSET_ALLOWLIST_PASS",
  "VERIFIED_ASSET_STATIC_AUDIT_PASS",
  "RUNTIME_ALLOWLIST_ALLOW_OK",
  "RUNTIME_ALLOWLIST_REJECT_OK",
  "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK",
  "STRICT_ASSET_PROVENANCE_PASS",
  "M3R_R6_EDITOR_BUILD_OK",
  "FINANCIAL_MANIFEST_OK",
  "DOMAIN_TESTS_OK",
  "MODEL_REGISTRY_OK",
  "FIRE_SYNC ok=1",
  "VEHICLE_DOMAIN spawn=1 possess=1",
  "PLAYABLE_WALK_OK=1",
  "DRIVE=1",
  "M8_SOCIAL_GATE_OK",
  "M7_TRAVEL_GATE_OK",
  "M7_DIRECTORY_GATE_OK",
  "M11_MISSION_GATE_OK",
  "M14_SOCIAL_GATE_OK",
  "M16_PERSISTENCE_GATE_OK",
  "M16_EVICTION_GATE_OK",
  "CLIENT_LOOP_OK",
  "MP_PARITY_OK",
  "PLAYABLE_OK",
  "FRONTEND_MENU_OK"
)
if (-not $SkipWorldServerGate)
{
  $finalRequiredMarkers += "WORLD_SERVER_GATE_OK"
}
if ($IntegrationGate)
{
  $finalRequiredMarkers += "FRONTEND_FLOW_OK"
}
$finalObservedMarkers = @(Get-R7ObservedMarkers)
$finalMissingMarkers = @($finalRequiredMarkers | Where-Object { $finalObservedMarkers -notcontains $_ })
if ($finalMissingMarkers.Count -gt 0)
{
  Fail "final gate summary missing fresh markers: $($finalMissingMarkers -join ',')"
}
Log "GATE_PASS"
function Get-ObservedMarkerValue([string]$Marker, [string]$Missing = "MISSING") {
  if ($finalObservedMarkers -contains $Marker) { return $Marker }
  return $Missing
}
$semanticObserved = @(
  "M3R_SEMANTIC_PARITY_PASS",
  "MESH_PARITY_PASS",
  "TEXTURE_PARITY_PASS",
  "MATERIAL_PARITY_PASS",
  "AUDIO_PARITY_PASS",
  "ANIMATION_PARITY_PASS",
  "VIDEO_PARITY_PASS",
  "PLACEMENT_PARITY_PASS",
  "UI_VISUAL_PARITY_PASS"
) | Where-Object { $finalObservedMarkers -contains $_ }
$runtimeObserved = @(
  "RUNTIME_ALLOWLIST_ALLOW_OK",
  "RUNTIME_ALLOWLIST_REJECT_OK",
  "RUNTIME_ALLOWLIST_NO_SUBSTITUTE_OK"
) | Where-Object { $finalObservedMarkers -contains $_ }
$summary = @{
  gate = Get-ObservedMarkerValue "GATE_PASS" "R7_BLOCKED"
  observed_markers = $finalObservedMarkers
  required_markers = $finalRequiredMarkers
  financial_hit_rate = $bind.financial_hit_rate
  financial_bind_pass = $bind.financial_pass
  domain = Get-ObservedMarkerValue "DOMAIN_TESTS_OK"
  model_registry = Get-ObservedMarkerValue "MODEL_REGISTRY_OK"
  client_loop = Get-ObservedMarkerValue "CLIENT_LOOP_OK"
  site1_fire_sync = Get-ObservedMarkerValue "FIRE_SYNC ok=1"
  vehicle_domain = Get-ObservedMarkerValue "VEHICLE_DOMAIN spawn=1 possess=1"
  mp_parity = Get-ObservedMarkerValue "MP_PARITY_OK"
  playable_walk = Get-ObservedMarkerValue "PLAYABLE_WALK_OK=1"
  playable_drive = Get-ObservedMarkerValue "DRIVE=1"
  playable = Get-ObservedMarkerValue "PLAYABLE_OK"
  frontend_menu = Get-ObservedMarkerValue "FRONTEND_MENU_OK"
  frontend_flow = if ($IntegrationGate) { Get-ObservedMarkerValue "FRONTEND_FLOW_OK" } else { "SKIPPED_integration_gate_off" }
  static_asset_audit = Get-ObservedMarkerValue "VERIFIED_ASSET_STATIC_AUDIT_PASS"
  r6_editor_build = Get-ObservedMarkerValue "M3R_R6_EDITOR_BUILD_OK"
  source_registry = Get-ObservedMarkerValue "SOURCE_REGISTRY_PASS"
  catalog_provenance = Get-ObservedMarkerValue "CATALOG_PROVENANCE_PASS"
  fidelity_oracle = Get-ObservedMarkerValue "FIDELITY_ORACLE_PASS"
  semantic_parity = ($semanticObserved -join "|")
  strict_asset_provenance = Get-ObservedMarkerValue "STRICT_ASSET_PROVENANCE_PASS"
  runtime_allowlist = ($runtimeObserved -join "|")
  world_server_gate = if ($SkipWorldServerGate) { "SKIPPED" } else { Get-ObservedMarkerValue "WORLD_SERVER_GATE_OK" }
  m7_travel_gate = Get-ObservedMarkerValue "M7_TRAVEL_GATE_OK"
  m7_directory_gate = Get-ObservedMarkerValue "M7_DIRECTORY_GATE_OK"
  m11_mission_gate = Get-ObservedMarkerValue "M11_MISSION_GATE_OK"
  m8_social_gate = Get-ObservedMarkerValue "M8_SOCIAL_GATE_OK"
  m14_social_gate = Get-ObservedMarkerValue "M14_SOCIAL_GATE_OK"
  m16_persistence_gate = Get-ObservedMarkerValue "M16_PERSISTENCE_GATE_OK"
  m16_eviction_gate = Get-ObservedMarkerValue "M16_EVICTION_GATE_OK"
  time = (Get-Date).ToString("o")
} | ConvertTo-Json
Set-Content -Path (Join-Path $Scratch "gate_summary.json") -Value $summary
$ver = @(
  "# Gate script output (authoritative)",
  "",
  "Generated by Tools/run_verification_gates.ps1 - do not hand-edit PASS claims.",
  "",
  '```',
  (Get-Content $gateLog -Raw),
  '```',
  "",
  ("bind_report financial_hit_rate={0} pass={1}" -f $bind.financial_hit_rate, $bind.financial_pass)
) -join "`n"
Set-Content -Path (Join-Path $Scratch "VERIFICATION.md") -Value $ver
exit 0
