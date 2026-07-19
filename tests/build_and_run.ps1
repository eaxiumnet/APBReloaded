$ErrorActionPreference = "Stop"
$proj = "D:\APBReloaded"
$dom = "$proj\Source\APBReloaded\Domain"
$out = "$proj\Binaries\Win64"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" }
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vsdev = Join-Path $install "Common7\Tools\VsDevCmd.bat"
$exe = "$out\APBDomainTests.exe"
$cmd = @"
call `"$vsdev`" -arch=amd64 -host_arch=amd64 >nul
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe`" `"$dom\APBCatalog.cpp`" `"$dom\APBInventory.cpp`" `"$dom\APBArmas.cpp`" `"$dom\APBAuction.cpp`" `"$dom\APBMission.cpp`" `"$dom\APBCombat.cpp`" `"$dom\APBCustomization.cpp`" `"$dom\APBModelRegistry.cpp`" `"$dom\APBWorldService.cpp`" `"$proj\tests\run_domain_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe`"
exit /b %ERRORLEVEL%
"@
$bat = Join-Path $env:TEMP "apb_domain_build.bat"
Set-Content $bat $cmd -Encoding ASCII
cmd /c $bat
exit $LASTEXITCODE
