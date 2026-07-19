$ErrorActionPreference = "Stop"
$proj = "D:\APBReloaded"
$out = "$proj\Binaries\Win64"
$scratch = if ($env:APB_SCRATCH) { $env:APB_SCRATCH } else { "C:\Users\Support\AppData\Local\Temp\grok-goal-4381756b8529\implementer" }
New-Item -ItemType Directory -Force -Path $out,$scratch | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" }
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vsdev = Join-Path $install "Common7\Tools\VsDevCmd.bat"
$exe = "$out\APBLayoutMathTests.exe"
$src = "$proj\tests\run_layout_math_tests.cpp"
$cmd = @"
call `"$vsdev`" -arch=amd64 -host_arch=amd64 >nul
cl /nologo /EHsc /std:c++17 /O2 /Fe:`"$exe`" `"$src`"
if errorlevel 1 exit /b 1
`"$exe`" `"$proj\Content`" `"$scratch\scale_path.txt`" `"$scratch\stage_beds.txt`"
exit /b %ERRORLEVEL%
"@
$bat = Join-Path $env:TEMP "apb_layout_math_build.bat"
Set-Content $bat $cmd -Encoding ASCII
cmd /c $bat
exit $LASTEXITCODE
