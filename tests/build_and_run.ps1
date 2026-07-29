$ErrorActionPreference = "Stop"
$proj = "D:\APBReloaded"
$dom = "$proj\Source\APBReloaded\Domain"
$out = "$proj\Binaries\Win64"
New-Item -ItemType Directory -Force -Path $out | Out-Null
$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) { $vswhere = "C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe" }
$install = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$vsdev = Join-Path $install "Common7\Tools\VsDevCmd.bat"
$srcs = "`"$dom\APBCatalog.cpp`" `"$dom\APBInventory.cpp`" `"$dom\APBArmas.cpp`" `"$dom\APBAuction.cpp`" `"$dom\APBMission.cpp`" `"$dom\APBCombat.cpp`" `"$dom\APBCustomization.cpp`" `"$dom\APBModelRegistry.cpp`" `"$dom\APBProgression.cpp`" `"$dom\APBPersistence.cpp`" `"$dom\APBWorldService.cpp`" `"$dom\APBMatchmaking.cpp`" `"$dom\APBGroup.cpp`" `"$dom\APBClan.cpp`" `"$dom\APBFriends.cpp`" `"$dom\APBSocialStore.cpp`" `"$dom\APBMailClaimJournal.cpp`""
$exe = "$out\APBDomainTests.exe"
$exe2 = "$out\APBPersistenceTests.exe"
$exe3 = "$out\APBFidelityTests.exe"
$exe4 = "$out\APBAuthTests.exe"
$exe5 = "$out\APBChatTests.exe"
$exe6 = "$out\APBGroupTests.exe"
$exe7 = "$out\APBClanTests.exe"
$exe8 = "$out\APBFriendTests.exe"
$exe9 = "$out\APBSocialStoreTests.exe"
$exe10 = "$out\APBMailTests.exe"
$exe11 = "$out\APBRelayTests.exe"
$exe12 = "$out\APBMatchmakingTests.exe"
$exe13 = "$out\APBAuctionTests.exe"
$exe14 = "$out\APBVehicleTests.exe"
$exe15 = "$out\APBProgressionTests.exe"
$exe16 = "$out\APBAntiCheatTests.exe"
$exe17 = "$out\APBDistrictDirectoryTests.exe"
$exe18 = "$out\APBHandoffTests.exe"
$exe19 = "$out\APBMailClaimJournalTests.exe"
$exe20 = "$out\APBPlacementBindingTests.exe"
$exe21 = "$out\APBPlacementParseTests.exe"
$srcs_auth = "$srcs `"$dom\APBTicket.cpp`""
$cmd = @"
call `"$vsdev`" -arch=amd64 -host_arch=amd64 >nul
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe`" $srcs `"$proj\tests\run_domain_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe2`" $srcs `"$proj\tests\run_persistence_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe2`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe3`" $srcs `"$proj\tests\run_fidelity_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe3`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /DAPB_AUTH_V2 /DAPB_TICKET_AVAILABLE /Fe:`"$exe4`" $srcs_auth `"$proj\tests\run_auth_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe4`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe5`" `"$dom\APBChat.cpp`" `"$proj\tests\run_chat_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe5`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe6`" `"$dom\APBGroup.cpp`" `"$proj\tests\run_group_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe6`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe7`" `"$dom\APBClan.cpp`" `"$proj\tests\run_clan_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe7`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe8`" `"$dom\APBFriends.cpp`" `"$proj\tests\run_friend_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe8`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe9`" $srcs `"$proj\tests\run_social_store_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe9`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe10`" `"$proj\tests\run_mail_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe10`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe11`" `"$dom\APBRelayProtocol.cpp`" `"$proj\tests\run_relay_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe11`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe12`" `"$dom\APBMatchmaking.cpp`" `"$proj\tests\run_matchmaking_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe12`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe13`" `"$dom\APBAuction.cpp`" `"$dom\APBInventory.cpp`" `"$dom\APBCatalog.cpp`" `"$proj\tests\run_auction_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe13`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe14`" `"$dom\APBVehicle.cpp`" `"$dom\APBCatalog.cpp`" `"$proj\tests\run_vehicle_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe14`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe15`" `"$dom\APBProgression.cpp`" `"$dom\APBCatalog.cpp`" `"$proj\tests\run_progression_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe15`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe16`" `"$dom\APBAntiCheat.cpp`" `"$proj\tests\run_anticheat_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe16`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /I`"$proj\Source\APBReloaded\Systems\Server`" /Fe:`"$exe17`" `"$dom\APBDistrictDirectory.cpp`" `"$dom\APBRelayProtocol.cpp`" `"$proj\tests\run_directory_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe17`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe18`" $srcs `"$dom\APBRelayProtocol.cpp`" `"$dom\APBHandoff.cpp`" `"$proj\tests\run_handoff_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe18`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe19`" `"$dom\APBMailClaimJournal.cpp`" `"$proj\tests\run_mail_claim_journal_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe19`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe20`" `"$proj\tests\run_placement_binding_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe20`"
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /O2 /I`"$dom`" /Fe:`"$exe21`" `"$proj\tests\run_placement_parse_tests.cpp`"
if errorlevel 1 exit /b 1
`"$exe21`"
if errorlevel 1 exit /b 1
exit /b %ERRORLEVEL%
"@
$bat = Join-Path $env:TEMP "apb_domain_build.bat"
Set-Content $bat $cmd -Encoding ASCII
cmd /c $bat
exit $LASTEXITCODE
