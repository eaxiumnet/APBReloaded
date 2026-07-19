# APB Reloaded offline client launcher
# IMPORTANT: Development APBReloaded.exe in Binaries\Win64 is a raw target binary —
# it often sits with NO WINDOW (no cooked UI). Default is UnrealEditor -game,
# which boots Lvl_APB_Frontend (splash -> login -> character -> district).
param(
  [ValidateSet("auto", "editor", "game", "staged")]
  [string]$Mode = "auto",
  [switch]$Fullscreen,
  [int]$Width = 1920,
  [int]$Height = 1080,
  [switch]$NoLog,
  [switch]$Wait
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if (-not $ProjectRoot) { $ProjectRoot = "D:\APBReloaded" }

$GameExe    = Join-Path $ProjectRoot "Binaries\Win64\APBReloaded.exe"
$StagedExe  = Join-Path $ProjectRoot "Saved\StagedBuilds\Windows\APBReloaded\Binaries\Win64\APBReloaded.exe"
$StagedPaks = Join-Path $ProjectRoot "Saved\StagedBuilds\Windows\APBReloaded\Content\Paks"
$UProject   = Join-Path $ProjectRoot "APBReloaded.uproject"
$EditorExe  = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$LogDir     = Join-Path $ProjectRoot "Saved\Logs"

function Test-StagedPlayable {
  if (-not (Test-Path $StagedExe)) { return $false }
  if (-not (Test-Path $StagedPaks)) { return $false }
  $pakCount = @(Get-ChildItem $StagedPaks -Filter "*.pak" -EA SilentlyContinue).Count
  return ($pakCount -gt 0)
}

function Resolve-Mode {
  param([string]$Want)
  switch ($Want) {
    "editor" {
      if (-not (Test-Path $EditorExe)) { throw "Editor missing: $EditorExe" }
      if (-not (Test-Path $UProject)) { throw "Project missing: $UProject" }
      return "editor"
    }
    "game" {
      if (-not (Test-Path $GameExe)) { throw "Game binary missing: $GameExe" }
      Write-Warning "Raw Development APBReloaded.exe often has NO WINDOW (not a packaged client)."
      Write-Warning "Prefer -Mode editor. Use -Mode game only for debugging."
      return "game"
    }
    "staged" {
      if (-not (Test-StagedPlayable)) {
        throw "Staged/cooked build not playable (missing exe or .pak under Saved\StagedBuilds\Windows\APBReloaded)."
      }
      return "staged"
    }
    default {
      # auto: Editor first (latest frontend UI + real window). Then staged package.
      # NEVER prefer raw Binaries\Win64\APBReloaded.exe — it stays headless (~50-130MB, no hwnd).
      if ((Test-Path $EditorExe) -and (Test-Path $UProject)) { return "editor" }
      if (Test-StagedPlayable) { return "staged" }
      if (Test-Path $GameExe) {
        Write-Warning "Falling back to raw Development game exe (usually NO WINDOW)."
        return "game"
      }
      throw "Nothing to launch. Need Unreal Editor at D:\UE58\UE_5.8 or a staged package."
    }
  }
}

$Resolved = Resolve-Mode $Mode
$WindowArgs = @()
if ($Fullscreen) {
  $WindowArgs += "-fullscreen"
} else {
  $WindowArgs += "-windowed"
  $WindowArgs += "-ResX=$Width"
  $WindowArgs += "-ResY=$Height"
}
if (-not $NoLog) { $WindowArgs += "-log" }

# Kill leftover headless zombies from prior failed launches
Get-Process -Name "APBReloaded" -EA SilentlyContinue | ForEach-Object {
  if ($_.MainWindowHandle -eq 0 -and $_.WorkingSet64 -lt 200MB) {
    Write-Host "  Stopping stuck headless APBReloaded PID $($_.Id)..." -ForegroundColor Yellow
    Stop-Process -Id $_.Id -Force -EA SilentlyContinue
  }
}

Write-Host ""
Write-Host "  APB RELOADED  (offline private client)" -ForegroundColor Cyan
Write-Host "  --------------------------------------"
Write-Host "  Mode     : $Resolved"
Write-Host "  Project  : $ProjectRoot"
Write-Host "  UI flow  : Splash -> Login -> Character -> District -> Freeroam"
Write-Host "  Controls : WASD | Mouse | LMB fire | E vehicle | F interact"
Write-Host "  Logs     : $LogDir\APBReloaded.log"
Write-Host ""

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

if ($Resolved -eq "editor") {
  $Exe = $EditorExe
  # Do NOT pass a map path — GameDefaultMap = Lvl_APB_Frontend
  $ArgList = @("`"$UProject`"", "-game") + $WindowArgs
  $WorkDir = $ProjectRoot
  Write-Host "  Launching Editor -game (VISIBLE frontend UI)" -ForegroundColor Green
  Write-Host "  $Exe"
  Write-Host "  $UProject -game $($WindowArgs -join ' ')"
  Write-Host ""
  Write-Host "  First boot can take 30-90s (shaders)." -ForegroundColor DarkYellow
  Write-Host "  You should see a DARK BLUE menu (not pure black) with:" -ForegroundColor DarkYellow
  Write-Host "    APB RELOADED  +  Screen: Splash/Login  +  CONTINUE button" -ForegroundColor Cyan
  Write-Host "  On-screen green/yellow debug text also prints stage names." -ForegroundColor DarkYellow
} elseif ($Resolved -eq "staged") {
  $Exe = $StagedExe
  $ArgList = $WindowArgs
  $WorkDir = Split-Path $StagedExe
  Write-Host "  Launching STAGED packaged client" -ForegroundColor Green
  Write-Host "  $Exe"
} else {
  $Exe = $GameExe
  # Point at project so it can find Content (still often windowless)
  $ArgList = @("`"$UProject`"") + $WindowArgs
  $WorkDir = $ProjectRoot
  Write-Host "  Launching raw Development game binary (may hang without window)" -ForegroundColor Yellow
  Write-Host "  $Exe"
  Write-Host "  If nothing appears, run:  .\Launch_APB.ps1 -Mode editor" -ForegroundColor Yellow
}

$proc = Start-Process -FilePath $Exe -ArgumentList $ArgList -WorkingDirectory $WorkDir -PassThru
Write-Host "  PID $($proc.Id) started." -ForegroundColor DarkGray

# Wait briefly and report whether a window appeared
$sawWindow = $false
for ($i = 0; $i -lt 25; $i++) {
  Start-Sleep -Milliseconds 400
  $live = Get-Process -Id $proc.Id -EA SilentlyContinue
  if (-not $live) {
    Write-Host "  Process exited early (code=$($proc.ExitCode)). Check $LogDir\APBReloaded.log" -ForegroundColor Red
    exit 1
  }
  if ($live.MainWindowHandle -ne 0) {
    $sawWindow = $true
    Write-Host "  Window OK: '$($live.MainWindowTitle)'" -ForegroundColor Green
    break
  }
}

if (-not $sawWindow) {
  $live = Get-Process -Id $proc.Id -EA SilentlyContinue
  $mb = if ($live) { [int]($live.WorkingSet64 / 1MB) } else { 0 }
  Write-Host ""
  Write-Host ("  WARNING: process is running ({0} MB) but still has NO WINDOW." -f $mb) -ForegroundColor Red
  if ($Resolved -eq "game") {
    Write-Host "  Raw Binaries\Win64\APBReloaded.exe is not packaged; often no window." -ForegroundColor Red
    Write-Host "  Stopping it. Re-run with:  .\Launch_APB.ps1 -Mode editor" -ForegroundColor Yellow
    Stop-Process -Id $proc.Id -Force -EA SilentlyContinue
    if ((Test-Path $EditorExe) -and (Test-Path $UProject)) {
      Write-Host ""
      Write-Host "  Auto-switching to Editor -game ..." -ForegroundColor Cyan
      $ArgList = @("`"$UProject`"", "-game") + $WindowArgs
      $proc = Start-Process -FilePath $EditorExe -ArgumentList $ArgList -WorkingDirectory $ProjectRoot -PassThru
      Write-Host ("  PID {0} (Editor). Wait for the game window (can take a minute)." -f $proc.Id) -ForegroundColor Green
    } else {
      exit 2
    }
  } else {
    Write-Host "  Still loading - wait up to 90s for first shader compile." -ForegroundColor Yellow
    $logHint = Join-Path $LogDir "APBReloaded.log"
    Write-Host ("  If never appears, open: {0}" -f $logHint) -ForegroundColor Yellow
  }
}

Write-Host ""
if ($Wait) {
  Wait-Process -Id $proc.Id
  exit $proc.ExitCode
}
