# APB Content Studio — dev launcher.
# Starts the Python backend (:8777) + Vite frontend (:5173) in their own windows,
# then opens the browser. Close the two windows (or Ctrl+C in each) to stop.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$server = Join-Path $root "server"
$web = Join-Path $root "web"
$venvPy = Join-Path $server ".venv\Scripts\python.exe"

if (-not (Test-Path $venvPy)) {
    Write-Host "Backend venv missing. Creating it + installing deps..." -ForegroundColor Yellow
    python -m venv (Join-Path $server ".venv")
    & $venvPy -m pip install --disable-pip-version-check -r (Join-Path $server "requirements.txt")
}
if (-not (Test-Path (Join-Path $web "node_modules"))) {
    Write-Host "Frontend deps missing. Running npm install..." -ForegroundColor Yellow
    Push-Location $web; npm install --no-audit --no-fund; Pop-Location
}

Write-Host "Starting backend on http://127.0.0.1:8777 ..." -ForegroundColor Cyan
Start-Process pwsh -ArgumentList '-NoExit','-Command',"& '$venvPy' -m uvicorn main:app --port 8777 --reload" -WorkingDirectory $server

Write-Host "Starting frontend on http://localhost:5173 ..." -ForegroundColor Cyan
Start-Process pwsh -ArgumentList '-NoExit','-Command','npm run dev' -WorkingDirectory $web

Start-Sleep -Seconds 4
Start-Process "http://localhost:5173/"
Write-Host "Viewer launching in your browser. Close the two new windows to stop the servers." -ForegroundColor Green
