#!/usr/bin/env pwsh
# Helper script to map captured uMod texture hashes to 2011 replacement textures.
# Place captured .dds files in the same folder as this script, then run it.

$modDir = $PSScriptRoot
$textures = Get-ChildItem -Path $modDir -Filter "*.dds" | Where-Object { $_.Name -notmatch '^[0-9A-Fa-f]{8}\.dds$' }

Write-Host "=== APB 2011 Menu Mod - Hash Mapper ===" -ForegroundColor Cyan
Write-Host "This script helps rename 2011 replacement textures to match captured uMod hashes." -ForegroundColor Gray
Write-Host ""

$captured = Get-ChildItem -Path $modDir -Filter "*.dds" | Where-Object { $_.Name -match '^[0-9A-Fa-f]{8}\.dds$' }
if (-not $captured) {
    Write-Host "No captured hash files found (format: XXXXXXXX.dds)." -ForegroundColor Yellow
    Write-Host "Capture textures in uMod first and copy them to this folder." -ForegroundColor Yellow
    exit 0
}

Write-Host "Captured hashes:" -ForegroundColor Green
$captured | ForEach-Object { Write-Host "  $($_.Name)" }
Write-Host ""

foreach ($cap in $captured) {
    Write-Host "Captured hash: $($cap.Name)" -ForegroundColor Green
    Write-Host "Available 2011 replacements:" -ForegroundColor Gray
    $i = 1
    $textures | ForEach-Object { Write-Host "  $i. $($_.Name)"; $i++ }
    $choice = Read-Host "Enter number of replacement texture (or press Enter to skip)"
    if ([string]::IsNullOrWhiteSpace($choice)) { continue }
    if ($choice -match '^\d+$') {
        $idx = [int]$choice - 1
        if ($idx -ge 0 -and $idx -lt $textures.Count) {
            $src = $textures[$idx]
            $dstName = $cap.Name
            $dst = Join-Path $modDir $dstName
            Copy-Item -Path $src.FullName -Destination $dst -Force
            Write-Host "Mapped $($src.Name) -> $dstName" -ForegroundColor Cyan
        } else {
            Write-Host "Invalid selection." -ForegroundColor Red
        }
    }
}

Write-Host ""
Write-Host "Done. Load the renamed hash .dds files into uMod and click Update." -ForegroundColor Cyan
