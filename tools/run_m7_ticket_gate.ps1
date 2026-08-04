param(
  [string]$Scratch = "$env:TEMP\apb_m7_ticket_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 60
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")
. (Join-Path $PSScriptRoot "scripts\APBGateCleanup.ps1")
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$districtLog = Join-Path $Scratch "district_ticket.log"
$mintIndex = 0
$district = $null
$failure = $null

function Fail([string]$Reason) {
  throw [System.InvalidOperationException]::new($Reason)
}

function Stop-ProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" |
      ForEach-Object { Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue }
    if (-not $Process.HasExited) {
      Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
      $Process.WaitForExit(10000) | Out-Null
    }
  } catch {}
}

function Stop-GateProcesses([switch]$BestEffort) {
  Stop-APBGateProcesses -Tracked @($district) -Project $Project -EngineBin (Split-Path $Editor) -BestEffort:$BestEffort
}

function Start-Editor([string[]]$Arguments) {
  return Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden
}

function Wait-Log([string]$Path, [string]$Pattern, [string]$FailureName) {
  $watch = [Diagnostics.Stopwatch]::StartNew()
  while ($watch.Elapsed.TotalSeconds -lt $TimeoutSec) {
    if (Test-Path $Path) {
      $content = Get-Content $Path -Raw -ErrorAction SilentlyContinue
      if ($content -match $Pattern) { return $content }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

function Mint-Ticket([string]$Account, [string]$Character, [string]$Faction, [string]$DistrictId) {
  $script:mintIndex++
  $mintLog = Join-Path $Scratch "mint_$script:mintIndex.log"
  $mint = Start-Editor @(
    $Project, "-game", "-APBMintTicket=$Account,$Character,$Faction,$DistrictId",
    "-nullrhi", "-nosound", "-unattended", "-AbsLog=$mintLog"
  )
  try {
    $content = Wait-Log $mintLog "MINTED_TICKET=([^\r\n\s]+)" "mint_timeout_$script:mintIndex"
    $match = [regex]::Match($content, "MINTED_TICKET=([^\r\n\s]+)")
    if (-not $match.Success) { Fail "mint_parse_$script:mintIndex" }
    return $match.Groups[1].Value
  } finally {
    Stop-ProcessTree $mint
  }
}

function Connect-Client([string]$Name, [string]$TravelUrl, [string]$ExpectedMarker) {
  $clientLog = Join-Path $Scratch "client_$Name.log"
  $client = Start-Editor @(
    $Project, $TravelUrl, "-game", "-nullrhi", "-nosound", "-unattended", "-AbsLog=$clientLog"
  )
  try {
    [void](Wait-Log $districtLog $ExpectedMarker "client_$Name`_marker_timeout")
  } finally {
    Stop-ProcessTree $client
  }
}

try {
  if (-not (Test-Path -LiteralPath $Project)) { Fail "project_missing" }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail "editor_binary_missing" }

  $catalog = Get-Content -LiteralPath (Join-Path $projectRoot "Content\Data\districts.json") -Raw | ConvertFrom-Json
  $financial = $catalog | Where-Object { $_.id -eq "Financial" } | Select-Object -First 1
  if ($null -eq $financial) { Fail "financial_catalog_missing" }
  $districtPort = Get-APBDistrictPort -Ports $ports -NumericId ([int]$financial.numeric_id)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  # M16 zero-trust: world/district processes preflight APB_DEPLOYMENT_SECRET and halt when it
  # is missing. The spine exports it for child gates; standalone leg runs must set it too.
  [Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
  # M7 teardown hardening: kill any leftover editors from a prior run BEFORE launching,
  # so this gate never starts with a live ghost. Sweep-and-verify throws if any persist.
  Stop-GateProcesses

  $validToken = Mint-Ticket "probe" "ProbeChar" "Criminal" $financial.id
  $wrongDistrictToken = Mint-Ticket "probe" "ProbeChar" "Criminal" "Waterfront"
  $tamperedTail = if ($validToken.EndsWith("0")) { "1" } else { "0" }
  $tamperedToken = $validToken.Substring(0, $validToken.Length - 1) + $tamperedTail

  $freeroamMap = "/Game/Maps/$($financial.map)"
  $district = Start-Editor @(
    $Project, "$freeroamMap`?listen?MaxPlayers=$([int]$financial.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    "-game", "-Port=$districtPort", "-DistrictId=$($financial.id)", "-NumericId=$([int]$financial.numeric_id)",
    "-RequireTicket", "-nullrhi", "-nosound", "-unattended", "-AbsLog=$districtLog"
  )
  [void](Wait-Log $districtLog "APB District session .*district=$($financial.id)" "district_bootstrap_timeout")

  Connect-Client "valid" "127.0.0.1:$districtPort`?APBTicket=$validToken" "DISTRICT_TICKET_OK account=probe char=ProbeChar district=$($financial.id)"
  [void](Wait-Log $districtLog "DISTRICT_TICKET_ADMITTED account=probe char=ProbeChar faction=Criminal" "valid_admission_timeout")

  Connect-Client "tampered" "127.0.0.1:$districtPort`?APBTicket=$tamperedToken" "DISTRICT_TICKET_FAIL reason=invalid"
  Connect-Client "replay" "127.0.0.1:$districtPort`?APBTicket=$validToken" "DISTRICT_TICKET_FAIL reason=replay"
  Connect-Client "wrong_district" "127.0.0.1:$districtPort`?APBTicket=$wrongDistrictToken" "DISTRICT_TICKET_FAIL reason=district_mismatch"
  Connect-Client "missing" "127.0.0.1:$districtPort`?APBTicket=" "DISTRICT_TICKET_FAIL reason=missing"

  $districtContent = Get-Content $districtLog -Raw
  $admittedCount = ([regex]::Matches($districtContent, "DISTRICT_TICKET_ADMITTED account=probe char=ProbeChar faction=Criminal")).Count
  if ($admittedCount -ne 1) { Fail "unexpected_admission_count_$admittedCount" }
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-GateProcesses -BestEffort
  Write-Host "===== district_ticket.log ====="
  if (Test-Path $districtLog) {
    Get-Content $districtLog | Where-Object { $_ -match "DISTRICT_TICKET_" }
  } else {
    Write-Host "(no district ticket log written)"
  }
  Get-ChildItem -LiteralPath $Scratch -Filter "mint_*.log" -ErrorAction SilentlyContinue |
    Sort-Object Name | ForEach-Object {
      Write-Host "===== $($_.Name) ====="
      Get-Content $_.FullName | Where-Object { $_ -match "MINTED_TICKET" }
    }
}

if (-not [string]::IsNullOrWhiteSpace($failure)) {
  Write-Host "DISTRICT_TICKET_GATE_FAIL $failure"
  exit 1
}

Write-Host "DISTRICT_TICKET_GATE_OK"
exit 0
