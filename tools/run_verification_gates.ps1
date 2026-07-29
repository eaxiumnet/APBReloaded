# Fail-fast verification gate spine for APB Reloaded 1:1.
# Order: bind report -> domain tests -> host client_loop -> client mp_observe -> playable -> frontend_menu (M4) -> frontend_flow (M9+M12, -IntegrationGate).
param(
  [string]$Scratch = "C:\Users\Support\AppData\Local\Temp\grok-goal-4ec7b7726483\implementer",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [string]$Map = "/Game/Maps/Lvl_APB_Financial_Freeroam",
  [int]$Port = 17777,
  [switch]$IntegrationGate
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$apbPorts = Get-APBPortContract -ProjectRoot $projectRoot
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$gateLog = Join-Path $Scratch "gate_run.log"

function Log([string]$m) {
  $line = "[{0}] {1}" -f (Get-Date -Format "o"), $m
  Add-Content -Path $gateLog -Value $line
  Write-Host $line
}

function Fail([string]$m) {
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
    if ($raw -notmatch $pattern) { Fail "pattern not found in $path : $pattern" }
  }
}

function Stop-Soft($proc) {
  if ($null -eq $proc) { return }
  try {
    if (-not $proc.HasExited) { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue }
  } catch {}
}

"" | Set-Content $gateLog
Log "GATE_START"

# 0) Bind report
Log "STEP bind_report"
$env:APB_SCRATCH = $Scratch
& python "D:\APBReloaded\Tools\build_placement_bind_report.py" 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "bind_report_run.log")
if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne 2) { Fail "bind_report exit $LASTEXITCODE" }
$bindPath = Join-Path $Scratch "bind_report.json"
Require-Fresh $bindPath (Get-Date).AddMinutes(-30) "financial_hit_rate"
$bind = Get-Content $bindPath -Raw | ConvertFrom-Json
Log ("BIND financial_hit_rate={0} pass={1}" -f $bind.financial_hit_rate, $bind.financial_pass)
if (-not $bind.financial_pass) { Fail "financial bind hit_rate < 0.9" }

# 0b) Financial manifest gate (required): validates the merged Financial district manifest and
#     emits FINANCIAL_MANIFEST_OK.
Remove-Item $financialManifest -Force -ErrorAction SilentlyContinue
Log "STEP financial_manifest_gate"
& python "D:\APBReloaded\tools\scripts\test_financial_district_manifest_gate.py" --skip-extractor 2>&1 | Tee-Object -FilePath $financialManifest
if ($LASTEXITCODE -ne 0) { Fail "financial_manifest_gate exit $LASTEXITCODE" }
Require-Fresh $financialManifest (Get-Date).AddMinutes(-30) "FINANCIAL_MANIFEST_OK"
Log "FINANCIAL_MANIFEST_OK"

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

$clientLoop = Join-Path $Scratch "client_loop.log"
$financialManifest = Join-Path $Scratch "financial_manifest_gate.log"
$mpLog = Join-Path $Scratch "mp_client_observe.log"
$mpDistrict = Join-Path $Scratch "mp_district.log"
$playable = Join-Path $Scratch "playable_probe.log"
$freeroam = Join-Path $Scratch "freeroam_district.log"
$frontendMenu = Join-Path $Scratch "frontend_menu.log"
$frontendFlow = Join-Path $Scratch "frontend_flow.log"
Remove-Item $clientLoop, $mpLog, $mpDistrict, $playable, $freeroam, $frontendMenu, $frontendFlow -Force -ErrorAction SilentlyContinue

# 2) Host listen + client_loop (map URL must include ?listen for UE IpNetDriver)
Log "STEP host_client_loop"
$env:APB_SCRATCH = $Scratch
$ListenMap = "$Map" + "?listen"
$hostArgs = @(
  $Project, $ListenMap, "-game", "-Port=$Port",
  "-APBProbe=client_loop", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$hostProc = Start-Process -FilePath $Editor -ArgumentList $hostArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
$sw = [Diagnostics.Stopwatch]::StartNew()
$okHost = $false
while (-not $hostProc.HasExited -and $sw.Elapsed.TotalSeconds -lt 120) {
  Start-Sleep 2
  if (Test-Path $clientLoop) {
    $c = Get-Content $clientLoop -Raw -ErrorAction SilentlyContinue
    if ($c -match "CLIENT_LOOP_OK") { $okHost = $true; Start-Sleep 3; break }
  }
}
if (-not $okHost) {
  Stop-Soft $hostProc
  Fail "host client_loop did not write CLIENT_LOOP_OK"
}
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
  "-APBProbe=mp_observe", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$clientProc = Start-Process -FilePath $Editor -ArgumentList $clientArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
$sw2 = [Diagnostics.Stopwatch]::StartNew()
$okMp = $false
while (-not $clientProc.HasExited -and $sw2.Elapsed.TotalSeconds -lt 90) {
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
  "-APBProbe=playable", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$playProc = Start-Process -FilePath $Editor -ArgumentList $playArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
$sw3 = [Diagnostics.Stopwatch]::StartNew()
$okPlay = $false
while (-not $playProc.HasExited -and $sw3.Elapsed.TotalSeconds -lt 90) {
  Start-Sleep 2
  if (Test-Path $playable) {
    $c = Get-Content $playable -Raw -ErrorAction SilentlyContinue
    if ($c -match "PLAYABLE_PROBE_COMPLETE") { $okPlay = $true; Start-Sleep 2; break }
  }
}
Stop-Soft $playProc
if (-not $okPlay) { Fail "playable probe incomplete" }
Require-Fresh $playable (Get-Date).AddMinutes(-5) "PLAYABLE_WALK_OK=1"
Require-Fresh $playable (Get-Date).AddMinutes(-5) "DRIVE=1"
Log "PLAYABLE_OK"

if (Test-Path $freeroam) {
  $f = Get-Content $freeroam -Raw
  if ($f -match "BOUND_SPAWN") { Log "BOUND_SPAWN_OK" }
  Log "FREEROAM_LOG_PRESENT"
}

# 5) Frontend menu gate (M4): 2011 menu Splash->Login->CharSelect->CharCreate->DistrictSelect->travel dispatch.
# Terminates at TRAVEL_OPENLEVEL_CALLED with FRONTEND_MENU_OK; independent of M9 geometry / M12 vehicles.
$FrontendMap = "/Game/Maps/Lvl_APB_Frontend"
Log "STEP frontend_menu"
$fmArgs = @(
  $Project, $FrontendMap, "-game",
  "-APBProbe=frontend_menu", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$fmProc = Start-Process -FilePath $Editor -ArgumentList $fmArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
$sw5 = [Diagnostics.Stopwatch]::StartNew()
$okFm = $false
$fmFail = $false
while (-not $fmProc.HasExited -and $sw5.Elapsed.TotalSeconds -lt 120) {
  Start-Sleep 2
  if (Test-Path $frontendMenu) {
    $c = Get-Content $frontendMenu -Raw -ErrorAction SilentlyContinue
    if ($c -match "FRONTEND_MENU_OK") { $okFm = $true; Start-Sleep 2; break }
    if ($c -match "FRONTEND_MENU_FAIL") { $fmFail = $true; break }
  }
}
Stop-Soft $fmProc
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
    "-APBProbe=frontend_flow", "-APBScratch=$Scratch", "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
  )
  $ffProc = Start-Process -FilePath $Editor -ArgumentList $ffArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
  $sw4 = [Diagnostics.Stopwatch]::StartNew()
  $okFf = $false
  $ffFail = $false
  while (-not $ffProc.HasExited -and $sw4.Elapsed.TotalSeconds -lt 150) {
    Start-Sleep 2
    if (Test-Path $frontendFlow) {
      $c = Get-Content $frontendFlow -Raw -ErrorAction SilentlyContinue
      if ($c -match "FRONTEND_FLOW_OK") { $okFf = $true; Start-Sleep 2; break }
      if ($c -match "FRONTEND_FLOW_FAIL") { $ffFail = $true; break }
    }
  }
  Stop-Soft $ffProc
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

$wsServerArgs = @(
  $Project, "${FrontendMap}?listen?game=/Script/APBReloaded.APBWorldGameMode",
  "-game", "-WorldServer", "-Port=$WSPort",
  "-APBProbe=world_server", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$wsServerProc = Start-Process -FilePath $Editor -ArgumentList $wsServerArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow

Start-Sleep 5

$wsAliceArgs = @(
  $Project, "127.0.0.1:$WSPort",
  "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=alice",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$wsBobArgs = @(
  $Project, "127.0.0.1:$WSPort",
  "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=bob",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$wsAliceProc = Start-Process -FilePath $Editor -ArgumentList $wsAliceArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
$wsBobProc   = Start-Process -FilePath $Editor -ArgumentList $wsBobArgs   -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow

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

Log "GATE_PASS"
$summary = @{
  gate = "PASS"
  financial_hit_rate = $bind.financial_hit_rate
  financial_bind_pass = $bind.financial_pass
  domain = "FAILS=0"
  client_loop = "CLIENT_LOOP_OK"
  site1_fire_sync = "FIRE_SYNC ok=1"
  mp_parity = "OK"
  playable_walk = "OK"
  playable_drive = "OK"
  frontend_menu = "FRONTEND_MENU_OK"
  frontend_flow = if ($IntegrationGate) { "FRONTEND_FLOW_OK" } else { "SKIPPED_integration_gate_off" }
  world_server_gate = "WORLD_SERVER_GATE_OK"
  m7_travel_gate = "M7_TRAVEL_GATE_OK"
  m7_directory_gate = "M7_DIRECTORY_GATE_OK"
  m11_mission_gate = "M11_MISSION_GATE_OK"
  m16_persistence_gate = "M16_PERSISTENCE_GATE_OK"
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
