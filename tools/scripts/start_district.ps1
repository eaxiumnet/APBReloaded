<#
  start_district.ps1 — M16 (brief #15): launch (or dry-run) a headless APB *district*
  server instance for the UE5.8 recreation.

  Role model (see Source/APBReloaded/Systems/Server/APBServerControl.cpp): the district is
  the DEFAULT server role — it does NOT pass -WorldServer. It hosts a Lvl_APB_*_Freeroam map
  with ?listen (per tools/DEDICATED_SERVER_GAP.md) and reports to the world routing authority
  over the relay control channel (APBPorts.h Relay=17800 / DistrictDirectory heartbeats).

  Maps and district types resolve from Content/Data/districts.json. Relay instance identity
  and the district NetDriver port can be overridden for additional instances; the first
  instance defaults to the catalog numeric_id and DistrictPortBase + catalog numeric_id.

  -DryRun resolves everything and prints the exact command line WITHOUT launching, so the
  script is verifiable before the APBReloaded(Server) binary exists. A missing binary in
  -DryRun is a warning, not an error.
#>
[CmdletBinding()]
param(
    # District id ("Financial") OR its apbdb numeric_id ("1"). Case-insensitive on id.
    [Parameter(Mandatory = $true)][string]$District,
    [string]$ServerExe        = "D:\APBReloaded\Binaries\Win64\APBReloaded.exe",
    [string]$DataDir          = "D:\APBReloaded\Content\Data",
    [string]$WorldHost        = "127.0.0.1",
    [string]$LogDir           = "D:\APBReloaded\Saved\Logs",
    [int]$NumericId,
    [int]$Port,
    [string[]]$ExtraArgs      = @(),
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
. (Join-Path $PSScriptRoot "APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$RelayPort = $ports.Relay

function Write-Log([string]$m) {
    Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m)
}

# --- resolve the district from the authoritative catalog -------------------------------
$districtsJson = Join-Path $DataDir "districts.json"
if (-not (Test-Path $districtsJson)) { throw "districts.json not found at $districtsJson" }
$catalog = Get-Content -Raw -Path $districtsJson | ConvertFrom-Json

$match = $catalog | Where-Object {
    $_.id -ieq $District -or ($_.numeric_id -as [string]) -eq $District
} | Select-Object -First 1

if (-not $match) {
    $known = ($catalog | ForEach-Object { "{0}({1})" -f $_.id, $_.numeric_id }) -join ", "
    throw "Unknown district '$District'. Known: $known"
}

$catalogNumericId = [int]$match.numeric_id
$instanceNumericId = if ($PSBoundParameters.ContainsKey("NumericId")) { $NumericId } else { $catalogNumericId }
if ($instanceNumericId -le 0) {
    throw "District '$($match.id)' has invalid instance numeric_id $instanceNumericId (must be > 0)"
}
$districtPort = if ($PSBoundParameters.ContainsKey("Port")) {
    $Port
} else {
    Get-APBDistrictPort -Ports $ports -NumericId $catalogNumericId
}
if ($districtPort -lt 1 -or $districtPort -gt 65535) {
    throw "District '$($match.id)' has invalid port $districtPort (must be 1..65535)"
}
if ($districtPort -eq $ports.World -or $districtPort -eq $ports.Relay) {
    throw "District '$($match.id)' cannot use reserved world/relay port $districtPort"
}
$maxPlayers = if ($match.max_players) { [int]$match.max_players } else { 64 }

# --- build the launch command line -----------------------------------------------------
New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$logFile = Join-Path $LogDir ("district_{0}_{1}.log" -f $match.id, $districtPort)

# District = default role (NO -WorldServer). ?listen host + relay coordinates.
$mapArg = "{0}?listen?MaxPlayers={1}" -f $match.map, $maxPlayers
$argList = @(
    $mapArg,
    "-Port=$districtPort",
    "-RelayHost=$WorldHost",
    "-RelayPort=$RelayPort",
    "-DistrictId=$($match.id)",
    "-NumericId=$instanceNumericId",
    "-log", "-abslog=$logFile", "-nosteam"
) + $ExtraArgs

Write-Log ("District '{0}' (numeric_id={1}) -> map={2} port={3} relay={4}:{5}" -f `
    $match.id, $instanceNumericId, $match.map, $districtPort, $WorldHost, $RelayPort)
$cmdPreview = ('"{0}" {1}' -f $ServerExe, ($argList -join ' '))
Write-Log ("CMD: {0}" -f $cmdPreview)

$exeMissing = -not (Test-Path $ServerExe)
if ($exeMissing) { Write-Log "WARN: server binary not found at $ServerExe (build the APBReloaded target first)" }

if ($DryRun) {
    Write-Log "DryRun: not launching. Command validated above."
    exit 0
}
if ($exeMissing) { throw "Cannot launch: server binary missing at $ServerExe" }

Write-Log "Launching district server..."
$proc = Start-Process -FilePath $ServerExe -ArgumentList $argList -PassThru
Write-Log ("Started PID={0}; logging to {1}" -f $proc.Id, $logFile)
exit 0
