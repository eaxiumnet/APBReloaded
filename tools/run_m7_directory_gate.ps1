param(
  [string]$Scratch = "$env:TEMP\apb_m7_directory_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$TimeoutSec = 120
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
. (Join-Path $PSScriptRoot 'scripts\APBPortContract.ps1')
$ports = Get-APBPortContract -ProjectRoot $projectRoot
$failure = $null
$world = $null
$financialA = $null
$financialB = $null
$seedClient = $null
$leastClient = $null
$survivorClient = $null
$worldLog = $null
$financialALog = $null
$financialBLog = $null
$seedEngineLog = $null
$leastEngineLog = $null
$survivorEngineLog = $null
$seedProbeLog = $null
$leastProbeLog = $null
$survivorProbeLog = $null
$deadline = $null
$leaked = -1
$allBoundPorts = @()
$oldDeploymentSecret = [Environment]::GetEnvironmentVariable('APB_DEPLOYMENT_SECRET', 'Process')

function Fail([string]$Reason) { throw [InvalidOperationException]::new($Reason) }

function Get-APBRelayHeartbeatIntervalMs {
  $headerPath = Join-Path $projectRoot 'Source\APBReloaded\Domain\APBRelayProtocol.h'
  if (-not (Test-Path -LiteralPath $headerPath)) { Fail 'relay_protocol_header_missing' }
  $header = Get-Content -LiteralPath $headerPath -Raw
  $match = [regex]::Match($header, 'inline\s+constexpr\s+int64_t\s+kRelayHeartbeatIntervalMs\s*=\s*(?<value>\d+)\s*;')
  if (-not $match.Success) { Fail 'relay_heartbeat_interval_missing' }
  return [int]$match.Groups['value'].Value
}

$heartbeatIntervalMs = Get-APBRelayHeartbeatIntervalMs
$staleThresholdMs = 10000
# StaleThresholdMs == 10000 + up to 1000 ms PruneStale latency + 5000 ms teardown/CI slack.
$evictionPollBudgetMs = 10000 + 1000 + 5000

function Stop-ProcessTree([Diagnostics.Process]$Process) {
  if ($null -eq $Process) { return }
  try {
    $children = @(Get-CimInstance Win32_Process -Filter "ParentProcessId=$($Process.Id)" -ErrorAction SilentlyContinue)
    foreach ($child in $children) {
      try {
        Stop-ProcessTree (Get-Process -Id $child.ProcessId -ErrorAction Stop)
      } catch {
        Write-Verbose "child_process_already_stopped id=$($child.ProcessId)"
      }
    }
    if (-not $Process.HasExited) {
      Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
      $Process.WaitForExit(10000) | Out-Null
    }
  } catch {
    Write-Verbose "process_tree_cleanup_failed id=$($Process.Id)"
  }
}

function Get-GateProcesses {
  $escapedProject = [regex]::Escape($Project)
  $escapedScratch = [regex]::Escape("-APBScratch=$Scratch")
  return @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue |
    Where-Object {
      $_.Name -in @('APBReloaded.exe', 'CrashReportClient.exe', 'UnrealEditor.exe', 'CrashReportClientEditor.exe') -and
      $_.CommandLine -match $escapedProject -and $_.CommandLine -match $escapedScratch
    })
}

function Stop-AllGateProcesses {
  foreach ($client in @($survivorClient, $leastClient, $seedClient)) {
    if ($null -ne $client) { Stop-ProcessTree $client.Process }
  }
  foreach ($gateProcess in @($financialB, $financialA, $world)) {
    Stop-ProcessTree $gateProcess
  }
  Get-GateProcesses | ForEach-Object {
    Stop-Process -Id $_.ProcessId -Force -ErrorAction SilentlyContinue
  }
}

function Get-BoundPortCount([int[]]$DistrictPorts) {
  $tcp = @(Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue |
    Where-Object { $_.LocalPort -eq $ports.Relay })
  $udp = @(Get-NetUDPEndpoint -ErrorAction SilentlyContinue |
    Where-Object { $_.LocalPort -in @($ports.World) + $DistrictPorts })
  return $tcp.Count + $udp.Count
}

function Assert-PortsFree([int[]]$DistrictPorts) {
  if ((Get-BoundPortCount $DistrictPorts) -ne 0) { Fail 'ports_not_free' }
}

function Start-Editor([string[]]$Arguments) {
  return Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru `
    -WorkingDirectory (Split-Path $Editor) -NoNewWindow
}

function Launch-World([string]$LogPath) {
  return Start-Editor @(
    $Project, '/Game/Maps/Lvl_APB_Frontend?listen?game=/Script/APBReloaded.APBWorldGameMode',
    '-game', '-WorldServer', "-Port=$($ports.World)", "-RelayPort=$($ports.Relay)",
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
  )
}

function Launch-District([pscustomobject]$District, [string]$InstanceId,
  [int]$InstanceNumericId, [int]$InstancePort, [string]$LogPath) {
  return Start-Editor @(
    $Project, "/Game/Maps/$($District.map)?listen?MaxPlayers=$([int]$District.max_players)?game=/Script/APBReloaded.APBFreeroamGameMode",
    '-game', "-Port=$InstancePort", '-RelayHost=127.0.0.1', "-RelayPort=$($ports.Relay)",
    "-DistrictId=$($District.id)", "-NumericId=$InstanceNumericId", '-RequireTicket',
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$LogPath", "-APBScratch=$Scratch"
  )
}

function Start-TravelClient([string]$Id) {
  $probeLog = Join-Path $Scratch "world_travel_client_$Id.log"
  $engineLog = Join-Path $Scratch "travel_$Id.log"
  $process = Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", '-game', '-WorldServerHost=127.0.0.1',
    '-APBProbe=world_travel_client', "-WSClientId=$Id", '-WSTravelDistrict=Financial',
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$engineLog", "-APBScratch=$Scratch"
  )
  return [pscustomobject]@{ Process = $process; ProbeLog = $probeLog; EngineLog = $engineLog }
}

function Start-DirectoryTicketClient([string]$Id) {
  $probeLog = Join-Path $Scratch "world_server_client_$Id.log"
  $engineLog = Join-Path $Scratch "directory_ticket_$Id.log"
  $process = Start-Editor @(
    $Project, "127.0.0.1:$($ports.World)", '-game', '-WorldServerHost=127.0.0.1',
    '-APBProbe=world_server_client', "-WSClientId=$Id",
    '-nullrhi', '-nosound', '-unattended', '-log', "-AbsLog=$engineLog", "-APBScratch=$Scratch"
  )
  return [pscustomobject]@{ Process = $process; ProbeLog = $probeLog; EngineLog = $engineLog }
}

function Wait-Log([string]$Path, [string]$Pattern, [string]$FailureName) {
  while ([Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline) {
    if (Test-Path -LiteralPath $Path) {
      $content = Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
      if ($content -match $Pattern) { return $content }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

function Read-LogRaw([string]$Path) {
  if (-not (Test-Path -LiteralPath $Path)) { return '' }
  return Get-Content -LiteralPath $Path -Raw -ErrorAction SilentlyContinue
}

function Wait-WorldEviction([string]$Path, [int]$StartOffset) {
  $watch = [Diagnostics.Stopwatch]::StartNew()
  while ($watch.ElapsedMilliseconds -lt $evictionPollBudgetMs) {
    $content = Read-LogRaw $Path
    $tail = if ($content.Length -gt $StartOffset) { $content.Substring($StartOffset) } else { '' }
    $match = [regex]::Match($tail, 'RELAY_EVICT\s+stale=(?<count>\d+)')
    if ($match.Success) {
      $staleCount = [int]$match.Groups['count'].Value
      if ($staleCount -ge 1) { return $staleCount }
    }
    Start-Sleep -Milliseconds 500
  }
  Fail 'eviction_timeout_within_16000ms'
}

function Get-PropertyValue([object]$Object, [string[]]$Names) {
  if ($null -eq $Object) { return $null }
  foreach ($name in $Names) {
    $property = $Object.PSObject.Properties[$name]
    if ($null -ne $property) { return $property.Value }
  }
  return $null
}

function Get-LatestDistrictListJson([string]$Path) {
  $content = Read-LogRaw $Path
  $lines = @($content -split "`r?`n")
  for ($i = $lines.Count - 1; $i -ge 0; $i--) {
    $line = $lines[$i]
    $start = $line.IndexOf('districtList=')
    if ($start -lt 0) { continue }
    $start += 'districtList='.Length
    $end = $line.IndexOf(' ticket=', $start)
    if ($end -gt $start) { return $line.Substring($start, $end - $start) }
  }
  return $null
}

function Get-LatestTicketJson([string]$Path) {
  $content = Read-LogRaw $Path
  $lines = @($content -split "`r?`n")
  for ($i = $lines.Count - 1; $i -ge 0; $i--) {
    $line = $lines[$i]
    $start = $line.IndexOf('ticket=')
    if ($start -lt 0) { continue }
    $json = $line.Substring($start + 'ticket='.Length).Trim()
    if ($json.StartsWith('{') -and $json.EndsWith('}')) { return $json }
  }
  return $null
}

function Find-FinancialAggregate([object[]]$DistrictList) {
  foreach ($entry in $DistrictList) {
    $districtId = Get-PropertyValue $entry @('id', 'district', 'districtId')
    if ($districtId -eq 'Financial') { return $entry }
  }
  return $null
}

function Wait-LiveDirectorySnapshot([string]$Path, [string]$FailureName) {
  while ([Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline) {
    $json = Get-LatestDistrictListJson $Path
    if ($null -ne $json) {
      try {
        $districtList = $json | ConvertFrom-Json -ErrorAction Stop
        $financialAggregate = Find-FinancialAggregate $districtList
        if ($null -ne $financialAggregate -and $null -ne (Get-PropertyValue $financialAggregate @('instanceCount'))) {
          return [pscustomobject]@{ Json = $json; DistrictList = $districtList; Financial = $financialAggregate }
        }
      } catch {
        Write-Verbose 'district_list_json_not_ready'
      }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

function Wait-TicketSnapshot([string]$Path, [string]$FailureName) {
  while ([Diagnostics.Stopwatch]::GetTimestamp() -lt $deadline) {
    $json = Get-LatestTicketJson $Path
    if ($null -ne $json) {
      try {
        $ticket = $json | ConvertFrom-Json -ErrorAction Stop
        $error = Get-PropertyValue $ticket @('error')
        if ($null -ne $error) { Fail "ticket_error_$error" }
        if ($null -ne (Get-PropertyValue $ticket @('port'))) {
          return [pscustomobject]@{ Json = $json; Ticket = $ticket }
        }
      } catch {
        if ($_.Exception.Message -like 'ticket_error_*') { throw }
        Write-Verbose 'ticket_json_not_ready'
      }
    }
    Start-Sleep -Milliseconds 250
  }
  Fail $FailureName
}

function Assert-FinancialAggregation([object]$FinancialAggregate, [int]$ExpectedPopulation) {
  $instanceCount = Get-PropertyValue $FinancialAggregate @('instanceCount')
  $population = Get-PropertyValue $FinancialAggregate @('population')
  if ($null -eq $instanceCount -or $null -eq $population) {
    Fail 'financial_aggregate_schema_invalid'
  }
  if ([int]$instanceCount -ne 2) { Fail "financial_instance_count_$instanceCount" }
  if ([int]$population -ne $ExpectedPopulation) {
    Fail "financial_population_expected_$ExpectedPopulation`_actual_$population"
  }
  return [pscustomobject]@{
    InstanceCount = [int]$instanceCount
    Population = [int]$population
  }
}

function Show-Log([string]$Title, [string]$Path, [string]$Pattern) {
  Write-Host "===== $Title ====="
  if ($Path -and (Test-Path -LiteralPath $Path)) {
    Get-Content -LiteralPath $Path | Where-Object { $_ -match $Pattern }
  } else {
    Write-Host '(no log written)'
  }
}

try {
  if ($TimeoutSec -le 0) { Fail 'invalid_timeout' }
  if (-not (Test-Path -LiteralPath $Project)) { Fail 'project_missing' }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail 'editor_missing' }
  if (($heartbeatIntervalMs * 2) -ne $staleThresholdMs) {
    Write-Host "DIRECTORY_HEARTBEAT_DISCREPANCY cadence_ms=$heartbeatIntervalMs threshold_ms=$staleThresholdMs"
  }

  $catalog = Get-Content -LiteralPath (Join-Path $projectRoot 'Content\Data\districts.json') -Raw | ConvertFrom-Json
  $financial = $catalog | Where-Object { $_.id -eq 'Financial' } | Select-Object -First 1
  if ($null -eq $financial) { Fail 'financial_catalog_missing' }
  $financialAId = [int]$financial.numeric_id
  $financialBId = $financialAId + 1
  $financialAPort = Get-APBDistrictPort -Ports $ports -NumericId $financialAId
  $financialBPort = Get-APBDistrictPort -Ports $ports -NumericId $financialBId
  $allBoundPorts = @($financialAPort, $financialBPort)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Stop-AllGateProcesses
	Assert-PortsFree $allBoundPorts
	[Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
  $deadline = [Diagnostics.Stopwatch]::GetTimestamp() + ($TimeoutSec * [Diagnostics.Stopwatch]::Frequency)
  $worldLog = Join-Path $Scratch 'world.log'
  $financialALog = Join-Path $Scratch 'financial_a.log'
  $financialBLog = Join-Path $Scratch 'financial_b.log'

  $world = Launch-World $worldLog
  [void](Wait-Log $worldLog "RELAY_LISTEN port=$($ports.Relay)" 'world_listener_timeout')
  $financialA = Launch-District $financial 'A' $financialAId $financialAPort $financialALog
  $financialB = Launch-District $financial 'B' $financialBId $financialBPort $financialBLog
  [void](Wait-Log $worldLog "RELAY_REGISTER district=Financial numeric_id=$financialAId ok=1" 'financial_a_register_timeout')
  [void](Wait-Log $worldLog "RELAY_REGISTER district=Financial numeric_id=$financialBId ok=1" 'financial_b_register_timeout')
  [void](Wait-Log $financialALog 'RELAY_CLIENT_HEARTBEAT seq=2' 'financial_a_heartbeat_timeout')
  [void](Wait-Log $financialBLog 'RELAY_CLIENT_HEARTBEAT seq=2' 'financial_b_heartbeat_timeout')

  $seedClient = Start-TravelClient 'directory_seed'
  $seedEngineLog = $seedClient.EngineLog
  $seedProbeLog = $seedClient.ProbeLog
  [void](Wait-Log $seedEngineLog "TRAVEL_OK district=Financial host=127.0.0.1 port=$financialAPort" 'seed_travel_to_financial_a_timeout')
  [void](Wait-Log $financialALog 'DISTRICT_TICKET_ADMITTED account=ACC-travel_directory_seed char=Operative' 'seed_admission_timeout')

  $leastClient = Start-DirectoryTicketClient 'directory_least'
  $leastEngineLog = $leastClient.EngineLog
  $leastProbeLog = $leastClient.ProbeLog
  [void](Wait-Log $leastProbeLog 'WORLD_CLIENT_OK login=1 charlist=1 districtlist=1 ticket=1 id=directory_least' 'least_client_timeout')
  $directorySnapshot = Wait-LiveDirectorySnapshot $leastEngineLog 'live_directory_json_timeout'
  $expectedFinancialPopulation = 1
  $aggregation = Assert-FinancialAggregation $directorySnapshot.Financial $expectedFinancialPopulation
  Write-Host "DIRECTORY_HAPPY_1_OK instanceCount=$($aggregation.InstanceCount) population=$($aggregation.Population) expectedPopulation=$expectedFinancialPopulation"

  $leastTicket = Wait-TicketSnapshot $leastEngineLog 'least_ticket_json_timeout'
  if ([int]$leastTicket.Ticket.port -ne $financialBPort) {
    Fail "least_loaded_ticket_port_expected_$financialBPort`_actual_$($leastTicket.Ticket.port)"
  }
  [void](Wait-Log $worldLog "TRAVEL_RESERVATION_ISSUED .*district=Financial port=$financialBPort" 'least_loaded_reservation_log_timeout')
  Write-Host "DIRECTORY_HAPPY_2_OK lowerLoadPort=$financialBPort ticketPort=$($leastTicket.Ticket.port)"

  $evictionStartOffset = (Read-LogRaw $worldLog).Length
  Stop-ProcessTree $financialB
  $financialB = $null
  $evictedCount = Wait-WorldEviction $worldLog $evictionStartOffset
  Write-Host "DIRECTORY_FAILURE_EVICT_OK stale=$evictedCount budget_ms=$evictionPollBudgetMs"

  $survivorClient = Start-DirectoryTicketClient 'directory_survivor'
  $survivorEngineLog = $survivorClient.EngineLog
  $survivorProbeLog = $survivorClient.ProbeLog
  [void](Wait-Log $survivorProbeLog 'WORLD_CLIENT_OK login=1 charlist=1 districtlist=1 ticket=1 id=directory_survivor' 'survivor_client_timeout')
  $survivorTicket = Wait-TicketSnapshot $survivorEngineLog 'survivor_ticket_json_timeout'
  if ([int]$survivorTicket.Ticket.port -ne $financialAPort -or [int]$survivorTicket.Ticket.port -eq $financialBPort) {
    Fail "evicted_node_selected_port_$($survivorTicket.Ticket.port)"
  }
  [void](Wait-Log $worldLog "TRAVEL_RESERVATION_ISSUED .*district=Financial port=$financialAPort" 'survivor_reservation_log_timeout')
  Write-Host "DIRECTORY_REGRESSION_TRAVEL_OK survivorPort=$financialAPort ticketPort=$($survivorTicket.Ticket.port)"
} catch {
  $failure = $_.Exception.Message.Replace("`r", ' ').Replace("`n", ' ')
} finally {
	Stop-AllGateProcesses
	[Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', $oldDeploymentSecret, 'Process')
  Start-Sleep -Milliseconds 500
  $leaked = @(Get-GateProcesses).Count + (Get-BoundPortCount $allBoundPorts)
  if ($leaked -ne 0 -and -not $failure) { $failure = "cleanup_leaked_$leaked" }
  Show-Log 'WORLD DIRECTORY' $worldLog 'RELAY_|TRAVEL_'
  Show-Log 'FINANCIAL A DIRECTORY' $financialALog 'RELAY_CLIENT_|DISTRICT_TICKET_'
  Show-Log 'FINANCIAL B DIRECTORY' $financialBLog 'RELAY_CLIENT_|DISTRICT_TICKET_'
  Show-Log 'SEED TRAVEL' $seedEngineLog 'TRAVEL_'
  Show-Log 'LEAST DIRECTORY TICKET' $leastEngineLog 'OnRep_WorldAuth|TRAVEL_'
  Show-Log 'SURVIVOR DIRECTORY TICKET' $survivorEngineLog 'OnRep_WorldAuth|TRAVEL_'
  Write-Host "LEAKED=$leaked"
}

if ($failure) {
  Write-Host "M7_DIRECTORY_GATE_FAIL $failure"
  exit 1
}

Write-Host 'M7_DIRECTORY_GATE_OK'
exit 0
