# APB Content Studio — dev launcher.
# Kills any stale listener on the studio ports, then starts the Python backend
# (:8777) + Vite frontend (:5173) in their own windows and opens the browser.
# Close the two windows (or Ctrl+C in each) to stop.
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
$server = Join-Path $root "server"
$web = Join-Path $root "web"
$venvPy = Join-Path $server ".venv\Scripts\python.exe"

# Leftover uvicorn reloaders from a previous run bind :8777 with SO_REUSEADDR
# and silently shadow a fresh instance (Windows routes new connections to the
# first binder, so the viewer keeps talking to stale code). Before booting,
# kill every process listening on the studio ports or running our uvicorn.
function Stop-StaleListener([int]$Port, [string]$Name) {
    # uvicorn workers inherit the listening socket, so netstat keeps attributing
    # it to the (dead) reloader PID. Kill the socket owner AND its spawned
    # worker, then re-check until the port is actually free.
    for ($attempt = 0; $attempt -lt 3; $attempt++) {
        $listeners = Get-NetTCPConnection -LocalPort $Port -State Listen -ErrorAction SilentlyContinue
        if (-not $listeners) { break }
        foreach ($conn in $listeners) {
            $owner = $conn.OwningProcess
            Write-Host "Stale $Name listener on :$Port -> killing PID $owner (attempt $($attempt + 1))" -ForegroundColor Yellow
            Stop-Process -Id $owner -Force -ErrorAction SilentlyContinue
            Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
                Where-Object { $_.CommandLine -match "parent_pid=$owner" } |
                ForEach-Object {
                    Write-Host "  killing orphaned worker PID $($_.ProcessId)" -ForegroundColor Yellow
                    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
                }
        }
        Start-Sleep -Milliseconds 600
    }
}

$staleUvicorn = Get-CimInstance Win32_Process -Filter "Name = 'python.exe'" -ErrorAction SilentlyContinue |
    Where-Object { $_.CommandLine -match 'uvicorn' -and $_.CommandLine -match '8777' }
foreach ($proc in $staleUvicorn) {
    Write-Host "Stale uvicorn -> killing PID $($proc.ProcessId)" -ForegroundColor Yellow
    Stop-Process -Id $proc.ProcessId -Force
}
Stop-StaleListener 8777 "backend"
Stop-StaleListener 5173 "frontend"
Start-Sleep -Seconds 1

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
