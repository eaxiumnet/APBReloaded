# Builds the fully static content studio bundle:
#   1. frontend built with VITE_STATIC=1 (every /api URL resolves to data/)
#   2. offline bake: catalogs + GLBs + PNGs -> studio-static/
#
# The result is a plain static directory deployable to GitHub Pages (copy into
# docs/studio/), any CDN, or a file server - no backend, no downloads for the
# visitor. Run from tools/content-studio/ (PowerShell).
$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Push-Location (Join-Path $root "web")
npm run build:static
Pop-Location

$py = Join-Path $root "server\.venv\Scripts\python.exe"
if (-not (Test-Path $py)) {
    throw "Backend venv missing - run run.ps1 once, or: python -m venv server/.venv; server/.venv/Scripts/pip install -r server/requirements.txt"
}
& $py (Join-Path $root "server\bake_static.py") --web-dist (Join-Path $root "web\dist") --out (Join-Path $root "studio-static")
