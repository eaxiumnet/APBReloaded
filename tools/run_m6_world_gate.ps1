# M6-only world-server gate (isolated from the full spine's M9/M12-dependent steps).
# Launches 1 headless world-server (Game target, -WorldServer role) + 2 clients (alice/bob);
# passes when both clients reach login -> charlist -> districtlist -> ticket and the
# authority writes WORLD_SERVER_GATE_OK with login=2 and ticket=2.
#
# Rationale: run_verification_gates.ps1 runs steps 0-6 first (bind/playable/vehicles) which
# depend on district geometry (M9) and vehicles (M12). This runner exercises ONLY the M6
# deliverable (step 7) so M6 can be verified independently of later content milestones.
param(
  [string]$Scratch = "$env:TEMP\apb_m6_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor  = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 220
)
$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
$Port = (Get-APBPortContract -ProjectRoot $projectRoot).World
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$FrontendMap = "/Game/Maps/Lvl_APB_Frontend"
$wsLog = Join-Path $Scratch "world_server.log"
Remove-Item "$Scratch\world_server*.log" -Force -ErrorAction SilentlyContinue
$oldDeploymentSecret = [Environment]::GetEnvironmentVariable('APB_DEPLOYMENT_SECRET', 'Process')
[Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')

function Launch($argList) {
  return Start-Process -FilePath $Editor -ArgumentList $argList -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

Write-Host "[M6] launching world-server on port $Port"
$srv = Launch @(
  $Project, "$FrontendMap`?listen?game=/Script/APBReloaded.APBWorldGameMode",
  "-game", "-WorldServer", "-Port=$Port",
  "-APBProbe=world_server", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
Start-Sleep 6
Write-Host "[M6] launching clients alice + bob"
$alice = Launch @(
  $Project, "127.0.0.1:$Port", "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=alice",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)
$bob = Launch @(
  $Project, "127.0.0.1:$Port", "-game", "-WorldServerHost=127.0.0.1", "-WSClientId=bob",
  "-APBProbe=world_server_client", "-APBScratch=$Scratch",
  "-nosplash", "-nosound", "-nullrhi", "-unattended", "-log"
)

$sw = [Diagnostics.Stopwatch]::StartNew()
$ok = $false
while ($sw.Elapsed.TotalSeconds -lt $TimeoutSec) {
  Start-Sleep 4
  if (Test-Path $wsLog) {
    $c = Get-Content $wsLog -Raw -ErrorAction SilentlyContinue
    if ($c -match "WORLD_SERVER_GATE_OK") { $ok = $true; Start-Sleep 2; break }
  }
}
foreach ($p in @($alice, $bob, $srv)) {
  try { if ($p -and -not $p.HasExited) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue } } catch {}
}
[Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', $oldDeploymentSecret, 'Process')

Write-Host "===== world_server.log ====="
if (Test-Path $wsLog) { Get-Content $wsLog } else { Write-Host "(no world_server.log written)" }
Write-Host "===== alice tail ====="
if (Test-Path "$Scratch\world_server_client_alice.log") { Get-Content "$Scratch\world_server_client_alice.log" -Tail 8 }
Write-Host "===== bob tail ====="
if (Test-Path "$Scratch\world_server_client_bob.log") { Get-Content "$Scratch\world_server_client_bob.log" -Tail 8 }

if (-not $ok) { Write-Host "M6_WORLD_GATE_FAIL"; exit 1 }
$raw = Get-Content $wsLog -Raw
if ($raw -notmatch "login=2") { Write-Host "M6_WORLD_GATE_FAIL login!=2"; exit 1 }
if ($raw -notmatch "ticket=2") { Write-Host "M6_WORLD_GATE_FAIL ticket!=2"; exit 1 }
Write-Host "M6_WORLD_GATE_PASS"
exit 0
