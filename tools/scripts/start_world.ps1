<#
  start_world.ps1 — M16 (brief #15): launch (or dry-run) the APB *world* routing-authority
  server for the UE5.8 recreation.

  Role model (see Source/APBReloaded/Systems/Server/APBServerControl.cpp): the world server
  passes -WorldServer, which flips UAPBServerControl into the world role (lobby/login,
  character list, district list, ticket issue). It listens on the world NetDriver port and
  owns the relay control channel (APBPorts.h Relay=17800) that district instances register /
  heartbeat into — consumed by the DistrictDirectory Domain service.

  Ports resolve from APBPorts.h ([APBServer] in Config/DefaultGame.ini mirrors them); the
  world map defaults to the project's GameDefaultMap (Lvl_APB_Frontend, which hosts the
  frontend/world GameMode).

  -DryRun resolves everything and prints the exact command line WITHOUT launching, so the
  script is verifiable before the APBReloaded(Server) binary exists. A missing binary in
  -DryRun is a warning, not an error.
#>
[CmdletBinding()]
param(
    [string]$ServerExe = "D:\APBReloaded\Binaries\Win64\APBReloaded.exe",
    [string]$WorldMap  = "Lvl_APB_Frontend",  # Config/DefaultEngine.ini GameDefaultMap
    [string]$LogDir    = "D:\APBReloaded\Saved\Logs",
    [string[]]$ExtraArgs = @(),
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
. (Join-Path $PSScriptRoot "APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$WorldPort = $ports.World
$RelayPort = $ports.Relay

function Write-Log([string]$m) {
    Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m)
}

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$logFile = Join-Path $LogDir ("world_{0}.log" -f $WorldPort)

# -WorldServer flips APBServerControl into the world routing role.
$argList = @(
    $WorldMap,
    "-WorldServer",
    "-Port=$WorldPort",
    "-RelayPort=$RelayPort",
    "-log", "-abslog=$logFile", "-nosteam"
) + $ExtraArgs

Write-Log ("World authority -> map={0} port={1} relay={2}" -f $WorldMap, $WorldPort, $RelayPort)
$cmdPreview = ('"{0}" {1}' -f $ServerExe, ($argList -join ' '))
Write-Log ("CMD: {0}" -f $cmdPreview)

$exeMissing = -not (Test-Path $ServerExe)
if ($exeMissing) { Write-Log "WARN: server binary not found at $ServerExe (build the APBReloaded target first)" }

if ($DryRun) {
    Write-Log "DryRun: not launching. Command validated above."
    exit 0
}
if ($exeMissing) { throw "Cannot launch: server binary missing at $ServerExe" }

Write-Log "Launching world server..."
$proc = Start-Process -FilePath $ServerExe -ArgumentList $argList -PassThru
Write-Log ("Started PID={0}; logging to {1}" -f $proc.Id, $logFile)
exit 0
