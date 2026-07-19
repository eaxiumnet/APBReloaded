# Offline Domain lobby emulator entry point (UE frontend uses the same WorldService).
param(
    [string]$DataDir = "D:\APBReloaded\Content\Data",
    [string]$Scratch = $env:APB_SCRATCH
)
$ErrorActionPreference = "Stop"
if (-not $Scratch) {
    $Scratch = "C:\Users\Support\AppData\Local\Temp\grok-goal-259c86d3b37e\implementer"
}
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
$log = Join-Path $Scratch "emulator_start.log"

function Write-Log([string]$m) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $m
    Add-Content -Path $log -Value $line
    Write-Host $line
}

"" | Set-Content -Path $log
Write-Log "=== Domain lobby start (WorldService offline emulator) ==="
Write-Log "DataDir=$DataDir"
Write-Log "Steam ApbPrivateServer LobbyServer needs MySQL; Domain is shipped offline lobby."

$build = "D:\APBReloaded\tests\build_and_run.ps1"
Write-Log "Running domain tests via $build"
& powershell -NoProfile -ExecutionPolicy Bypass -File $build 2>&1 | Tee-Object -FilePath (Join-Path $Scratch "domain_tests.log")
$code = $LASTEXITCODE
Write-Log "domain_tests exit=$code"

$exe = "D:\APBReloaded\Binaries\Win64\APBDomainTests.exe"
if (Test-Path $exe) {
    Write-Log "APBDomainTests present"
} else {
    Write-Log "WARN: APBDomainTests.exe missing"
}

$psRoot = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\ApbPrivateServer\bin\Release"
$lobbyExe = Join-Path $psRoot "LobbyServer.exe"
if (Test-Path $lobbyExe) {
    Write-Log "Found LobbyServer.exe - non-blocking probe (3s timeout)"
    try {
        $p = Start-Process -FilePath $lobbyExe -WorkingDirectory $psRoot -PassThru -WindowStyle Hidden -ErrorAction Stop
        $deadline = (Get-Date).AddSeconds(3)
        while ((Get-Date) -lt $deadline -and $p -and -not $p.HasExited) {
            Start-Sleep -Milliseconds 200
        }
        if ($p -and -not $p.HasExited) {
            Write-Log ("LobbyServer PID={0} still running; stopping (MySQL may be missing)" -f $p.Id)
            Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
        } else {
            $ec = if ($p) { $p.ExitCode } else { "n/a" }
            Write-Log ("LobbyServer exited code={0}" -f $ec)
        }
    } catch {
        Write-Log ("LobbyServer start error: {0}" -f $_)
    }
} else {
    Write-Log "LobbyServer.exe not found"
}

# Ensure lobby_flow from dedicated proof if present
$flowExe = "D:\APBReloaded\Binaries\Win64\APBLobbyFlowProof.exe"
$lobbyOut = Join-Path $Scratch "lobby_flow.txt"
if (Test-Path $flowExe) {
    & $flowExe $DataDir $lobbyOut
    Write-Log ("lobby_flow proof exit={0}" -f $LASTEXITCODE)
} else {
    @(
        "LOBBY_BACKEND=Domain_WorldService"
        "DATA_DIR=$DataDir"
        "PHASE_ORDER=Register/Login -> EnterWorld(W1) -> CreateCharacter(faction) -> ListDistricts -> JoinDistrict"
        "FRONTEND_STAGES=Splash,Login,CharacterSelect,CharacterCreate,DistrictSelect,Settings,Loading"
        "DOMAIN_TESTS_EXIT=$code"
        "START_SCRIPT=tools/scripts/Start_DomainLobby.ps1"
        "NOTE=Run tools/scripts/build_lobby_flow.ps1 for LOBBY_FLOW_PASS=1 detail"
    ) | Set-Content -Path $lobbyOut -Encoding UTF8
    Write-Log "Wrote summary lobby_flow.txt (proof exe missing)"
}

Write-Log "Done"
exit $code
