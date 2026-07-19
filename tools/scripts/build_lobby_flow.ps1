$ErrorActionPreference = "Stop"
$proj = "D:\APBReloaded"
$dom = "$proj\Source\APBReloaded\Domain"
$out = "$proj\Binaries\Win64"
$scratch = if ($env:APB_SCRATCH) { $env:APB_SCRATCH } else { "C:\Users\Support\AppData\Local\Temp\grok-goal-259c86d3b37e\implementer" }
New-Item -ItemType Directory -Force -Path $out,$scratch | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vsdev = Join-Path $install "Common7\Tools\VsDevCmd.bat"
$exe = "$out\APBLobbyFlowProof.exe"
$src = "$proj\tools\scripts\run_lobby_flow_proof.cpp"
$outFile = Join-Path $scratch "lobby_flow.txt"
$cmd = @"
call `"$vsdev`" -arch=amd64 -host_arch=amd64 >nul
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe`" `"$dom\APBCatalog.cpp`" `"$dom\APBInventory.cpp`" `"$dom\APBArmas.cpp`" `"$dom\APBAuction.cpp`" `"$dom\APBMission.cpp`" `"$dom\APBCombat.cpp`" `"$dom\APBCustomization.cpp`" `"$dom\APBModelRegistry.cpp`" `"$dom\APBWorldService.cpp`" `"$src`"
if errorlevel 1 exit /b 1
`"$exe`" `"$proj\Content\Data`" `"$outFile`"
exit /b %ERRORLEVEL%
"@
$bat = Join-Path $env:TEMP "apb_lobby_flow_build.bat"
Set-Content $bat $cmd -Encoding ASCII
cmd /c $bat
exit $LASTEXITCODE
