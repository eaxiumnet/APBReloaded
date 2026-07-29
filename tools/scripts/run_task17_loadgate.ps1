<#
  run_task17_loadgate.ps1 - Task 17 conductor: launch the world + Social district
  listen-servers via UnrealEditor.exe (the installed engine cannot build the Game-target
  APBReloaded.exe; see work/m6_server_target_limit.md), wait for readiness, run the
  64-client load gate against them with -ServerPerfLog wired to the district's -APBPerfLog,
  then tear down every process this script started. Bounded and self-cleaning.

  Role model (Source/APBReloaded/Systems/Server/APBServerControl.cpp): world passes
  -WorldServer (login/charlist/districtlist/ticket + relay listener on Relay port);
  district is the default role (?listen) and reports over the relay. Roles CANNOT
  co-locate in one process (bWorldServerRole is picked once per process), so a real
  authenticated run needs world + district as separate processes.

  -DryRun prints the exact world/district command lines and the gate invocation WITHOUT
  launching anything. Use it to validate before spending RAM on a shared box.
#>
[CmdletBinding()]
param(
    [string]$Editor       = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
    [string]$Project      = "D:\APBReloaded\APBReloaded.uproject",
    [string]$District     = "Social",
    [string]$WorldMap     = "Lvl_APB_Frontend",
    [int]$Clients         = 2,
    [int]$DurationSeconds = 90,
    [int]$WarmupSec       = 45,
    [int]$ReadyTimeoutSec = 240,
    [string]$LogDir       = "D:\APBReloaded\work\task17-loadgate",
    [switch]$DryRun
)
$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
. (Join-Path $PSScriptRoot "APBPortContract.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot

function Write-Log([string]$m) { Write-Host ("[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m) }

# --- resolve the Social district from the authoritative catalog ------------------------
$districtsJson = Join-Path $projectRoot "Content\Data\districts.json"
if (-not (Test-Path -LiteralPath $districtsJson)) { throw "districts.json not found at $districtsJson" }
$catalog = Get-Content -Raw -LiteralPath $districtsJson | ConvertFrom-Json
$match = $catalog | Where-Object { $_.id -ieq $District -or ($_.numeric_id -as [string]) -eq $District } | Select-Object -First 1
if (-not $match) { throw "Unknown district '$District'." }
$numericId    = [int]$match.numeric_id
$districtPort  = Get-APBDistrictPort -Ports $ports -NumericId $numericId
$maxPlayers    = if ($match.max_players) { [int]$match.max_players } else { 64 }
$districtMap   = [string]$match.map

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null
$worldLog    = Join-Path $LogDir ("world_{0}.log"    -f $ports.World)
$districtLog = Join-Path $LogDir ("district_{0}_{1}.log" -f $match.id, $districtPort)
$perfLog     = Join-Path $LogDir ("district_{0}_perf.log" -f $match.id)
$gateRaw     = Join-Path $LogDir "gate_raw.log"
$gateJson    = Join-Path $LogDir "gate_result.json"

# UnrealEditor.exe <project> <map> -game <role args>. The map URL MUST carry ?game= to select
# the GameMode: map-prefix routing (DefaultEngine.ini) is bypassed by the map's baked
# WorldSettings DefaultGameMode, so without ?game= the world boots APBFrontendGameMode and never
# creates UAPBServerControl (dead login). This mirrors every canonical M6/M7 launcher.
$worldMapArg = "{0}?listen?game=/Script/APBReloaded.APBWorldGameMode" -f $WorldMap
$worldArgs = @(
    $Project, $worldMapArg, "-game", "-WorldServer",
    "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    "-nullrhi", "-nosound", "-unattended", "-nosplash", "-nosteam",
    "-log", "-abslog=$worldLog"
)
$districtMapArg = "{0}?listen?MaxPlayers={1}?game=/Script/APBReloaded.APBFreeroamGameMode" -f $districtMap, $maxPlayers
$districtArgs = @(
    $Project, $districtMapArg, "-game",
    "-Port=$districtPort", "-RelayHost=127.0.0.1", "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($match.id)", "-NumericId=$numericId",
    "-APBPerfLog=$perfLog",
    "-nullrhi", "-nosound", "-unattended", "-nosplash", "-nosteam",
    "-log", "-abslog=$districtLog"
)

$gateScript = Join-Path $projectRoot "tools\run_64_client_gate.ps1"
# -ServerPort is the client's initial ClientTravel target and MUST be the WORLD login server
# (Port=$($ports.World)), NOT the district. -WorldServerHost only toggles world-server-client
# mode; it does not redirect the connection (APBGameInstanceSubsystem.cpp:64-68). Login /
# charlist / districtlist / ticket are UE RPCs (PS->Server_LoginRequest) that run on whichever
# server the client's PlayerState lives on, so the client authenticates on world, then
# self-travels to the district using host+port read from the world's relay-registered ticket
# reservation. Pointing this at the district port strands every client in an endless sent_login
# retry -> LOAD_GATE_FAIL reason=missing_authenticated_identities. The district is reached via
# the ticket, never via this arg; -ServerPerfLog still points at the district's perf log.
$gateArgs = @(
    "-Map", $match.id, "-Clients", $Clients, "-DurationSeconds", $DurationSeconds,
    "-ServerAddress", "127.0.0.1", "-ServerPort", $ports.World,
    "-ServerPerfLog", $perfLog, "-OutputPath", $gateJson, "-RawLogPath", $gateRaw
)

Write-Log ("WORLD    : `"{0}`" {1}" -f $Editor, ($worldArgs -join ' '))
Write-Log ("DISTRICT : `"{0}`" {1}" -f $Editor, ($districtArgs -join ' '))
Write-Log ("GATE     : pwsh -NoProfile -File `"{0}`" {1}" -f $gateScript, ($gateArgs -join ' '))
Write-Log ("PERFLOG  : {0}" -f $perfLog)

if ($DryRun) { Write-Log "DryRun: nothing launched. Command lines validated above."; exit 0 }
if (-not (Test-Path -LiteralPath $Editor -PathType Leaf))  { throw "Editor missing: $Editor" }
if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) { throw "Project missing: $Project" }
if (-not (Test-Path -LiteralPath $gateScript -PathType Leaf)) { throw "Gate missing: $gateScript" }

$started = New-Object System.Collections.Generic.List[object]
function Stop-Started {
    foreach ($p in $started) {
        $live = Get-Process -Id $p.Id -ErrorAction SilentlyContinue
        if ($live) { Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue; Write-Log ("stopped PID={0}" -f $p.Id) }
    }
}
try {
    Write-Log "Launching world server..."
    $world = Start-Process -FilePath $Editor -ArgumentList $worldArgs -PassThru -WorkingDirectory (Split-Path -Parent $Editor)
    $started.Add($world)
    Write-Log ("world PID={0}; log={1}" -f $world.Id, $worldLog)

    Write-Log "Launching Social district listen-server..."
    $dist = Start-Process -FilePath $Editor -ArgumentList $districtArgs -PassThru -WorkingDirectory (Split-Path -Parent $Editor)
    $started.Add($dist)
    Write-Log ("district PID={0}; log={1}" -f $dist.Id, $districtLog)

    # Readiness: both procs alive through warmup AND the district perf log starts emitting
    # scope=server APB_PERF_METRIC (the authoritative Tick is running). Bounded by ReadyTimeoutSec.
    $sw = [Diagnostics.Stopwatch]::StartNew()
    $ready = $false
    while ($sw.Elapsed.TotalSeconds -lt $ReadyTimeoutSec) {
        Start-Sleep -Seconds 3
        if (-not (Get-Process -Id $world.Id -ErrorAction SilentlyContinue)) { throw "world server exited during warmup (see $worldLog)" }
        if (-not (Get-Process -Id $dist.Id  -ErrorAction SilentlyContinue)) { throw "district server exited during warmup (see $districtLog)" }
        if ($sw.Elapsed.TotalSeconds -ge $WarmupSec -and (Test-Path -LiteralPath $perfLog -PathType Leaf)) {
            $perf = Get-Content -LiteralPath $perfLog -Raw -ErrorAction SilentlyContinue
            if ($perf -and ($perf -match 'APB_PERF_METRIC[^\r\n]*scope=server')) { $ready = $true; break }
        }
    }
    if (-not $ready) { throw "servers not ready within ${ReadyTimeoutSec}s (no scope=server perf telemetry in $perfLog)" }
    Write-Log ("servers ready after {0:n0}s; running gate ({1} clients, {2}s)..." -f $sw.Elapsed.TotalSeconds, $Clients, $DurationSeconds)

    & pwsh -NoProfile -File $gateScript @gateArgs
    $gateExit = $LASTEXITCODE
    Write-Log ("gate exit={0}; result={1}" -f $gateExit, $gateJson)
    exit $gateExit
} finally {
    Write-Log "tearing down servers this script started..."
    Stop-Started
}
