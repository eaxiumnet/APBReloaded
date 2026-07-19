#!/usr/bin/env pwsh
# capture_and_build.ps1
# Use this if the pre-hashed mod does not replace textures in-game.
# It helps you capture live uMod hashes and build a correctly-named mod folder.

param(
    [string]$RetailAPBPath = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\Binaries\APB.exe",
    [string]$uModPath = ".\..\uMod\uMod\uMod.exe",
    [string]$CaptureFolder = ".\uMod_Captures",
    [string]$ReplacementFolder = ".\..\textures\2011_dds",
    [string]$OutputFolder = ".\APB_2011_Menu_Mod_live"
)

Write-Host "=== APB 2011 Menu Mod - Live Capture Builder ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "This script will:" -ForegroundColor Gray
Write-Host "  1. Start uMod." -ForegroundColor Gray
Write-Host "  2. Launch APB from Steam." -ForegroundColor Gray
Write-Host "  3. Wait for you to capture textures with uMod (save to: $CaptureFolder)." -ForegroundColor Gray
Write-Host "  4. Map captured hashes to 2011 replacements and build $OutputFolder." -ForegroundColor Gray
Write-Host ""

# Ensure capture folder exists
New-Item -ItemType Directory -Force -Path $CaptureFolder | Out-Null
New-Item -ItemType Directory -Force -Path $OutputFolder | Out-Null

# Start uMod
Write-Host "Starting uMod..." -ForegroundColor Green
Start-Process -FilePath $uModPath -WorkingDirectory (Split-Path $uModPath)

# Launch APB
Write-Host "Launching APB..." -ForegroundColor Green
Start-Process -FilePath "steam://rungameid/339610"

Read-Host "Press Enter after you have captured textures with uMod to $CaptureFolder"

# Build mapping from captured hashes to replacement textures
$replacements = @{
    "Constant_BG" = "Constant_BG"
    "NewBackgroundImage" = "Constant_BG"
    "LoadingScreen_APB" = "LoadingScreen_APB"
    "LoadingScreen_APBFlame_Alpha" = "LoadingScreen_APBFlame_Alpha"
    "CriminalFactionicon" = "CriminalFactionicon"
    "CriminalFactionicon_Unselected" = "CriminalFactionicon_Unselected"
    "EnforcerFactionicon" = "EnforcerFactionicon"
    "EnforcerFactionicon_Unselected" = "EnforcerFactionicon_Unselected"
    "factionheadericon" = "factionheadericon"
    "FactionSelectbulletpoint" = "FactionSelectbulletpoint"
    "FactionSelectbulletpoint_Unselected" = "FactionSelectbulletpoint_Unselected"
    "factionselectbuttongrey" = "factionselectbuttongrey"
    "frontendFooter" = "frontendFooter"
    "JKICON_login_header_key" = "JKICON_login_header_key"
    "LoadingArrows_BG" = "LoadingArrows_BG"
    "LoadingArrows_Mask" = "LoadingArrows_Mask"
    "LoadingArrows_Ring" = "LoadingArrows_Ring"
    "LoadingIcon_MAIN" = "LoadingIcon_MAIN"
    "newCriminalcon" = "newCriminalcon"
    "newEnforcerIcon" = "newEnforcerIcon"
    "splatter1" = "splatter1"
    "worldselecticon" = "worldselecticon"
    "ArcIcon64x64" = "CharacterSelectIcon"
    "EpicGamesIcon64x64" = "CharacterSelectIcon"
    "KongregateIcon64x64" = "CharacterSelectIcon"
    "SteamIcon64x64" = "CharacterSelectIcon"
    "KongregateLogo" = "LoadingScreen_APB"
    "SteamLogo" = "LoadingScreen_APB"
    "Login_APB_Logo" = "LoadingScreen_APB"
}

$captured = Get-ChildItem -Path $CaptureFolder -Filter "*.dds"
if (-not $captured) {
    Write-Host "No captured .dds files found in $CaptureFolder" -ForegroundColor Red
    exit 1
}

foreach ($cap in $captured) {
    # uMod saves captured textures as <hash>.dds or <name>_<hash>.dds
    if ($cap.Name -match "^[0-9A-Fa-f]{8}\.dds$") {
        $hash = $cap.BaseName
    } elseif ($cap.Name -match "_([0-9A-Fa-f]{8})\.dds$") {
        $hash = $Matches[1]
    } else {
        Write-Host "Skipping unrecognized capture: $($cap.Name)" -ForegroundColor Yellow
        continue
    }

    # Try to infer retail name from uMod's displayed name if present
    # Otherwise ask user
    $retailName = Read-Host "Retail texture name for capture $($cap.Name) (press Enter to skip)"
    if ([string]::IsNullOrWhiteSpace($retailName)) { continue }

    if (-not $replacements.ContainsKey($retailName)) {
        Write-Host "No known replacement for $retailName" -ForegroundColor Yellow
        continue
    }

    $replacementName = $replacements[$retailName]
    $replacementPath = Join-Path $ReplacementFolder "$replacementName.dds"
    if (-not (Test-Path $replacementPath)) {
        Write-Host "Replacement not found: $replacementPath" -ForegroundColor Red
        continue
    }

    $outPath = Join-Path $OutputFolder "$hash.dds"
    Copy-Item -Path $replacementPath -Destination $outPath -Force
    Write-Host "Mapped $retailName -> $hash.dds" -ForegroundColor Cyan
}

Write-Host ""
Write-Host "Live mod built in: $OutputFolder" -ForegroundColor Green
Write-Host "Load these .dds files into uMod and click Update." -ForegroundColor Green
