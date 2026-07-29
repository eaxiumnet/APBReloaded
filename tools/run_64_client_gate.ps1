[CmdletBinding()]
param(
    [Alias('District')][string]$Map = 'Social',
    [string]$Clients = '64',
    [Alias('Duration', 'DurationMinutes')][string]$DurationSeconds = '1800',
    [Alias('Latency')][string]$LatencyMs = '0',
    [Alias('Loss', 'LossPercent')][string]$PacketLossPercent = '0',
    [string]$ScalabilityProfile = 'Server',
    [string]$HostsPath = '',
    [string]$OutputPath = '',
    [string]$RawLogPath = '',
    [string]$ServerAddress = '127.0.0.1',
    [string]$ServerPort = '17819',
    [string]$ServerPerfLog = '',
    [Alias('DryRun')][switch]$ValidateOnly
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'PerfTelemetry.psm1') -Force

if ([string]::IsNullOrWhiteSpace($HostsPath)) { $HostsPath = Join-Path $PSScriptRoot 'load_hosts.example.json' }
if ([string]::IsNullOrWhiteSpace($OutputPath)) { $OutputPath = Join-Path $root ('work\load64_{0:yyyyMMdd_HHmmss}.json' -f (Get-Date)) }
if ([string]::IsNullOrWhiteSpace($RawLogPath)) { $RawLogPath = [IO.Path]::ChangeExtension($OutputPath, '.log') }
$rawLogParent = Split-Path -Parent $RawLogPath
if (-not [string]::IsNullOrWhiteSpace($rawLogParent)) { New-Item -ItemType Directory -Force -Path $rawLogParent | Out-Null }
Set-Content -LiteralPath $RawLogPath -Encoding utf8 -Value "$(Get-Date -Format o) LOAD_GATE_RAW_LOG_START"

$failure = ''
$mapAsset = 'unresolved:' + $(if ([string]::IsNullOrWhiteSpace($Map)) { 'empty' } else { $Map })
$clientCount = 0
$duration = 0
$latency = 0.0
$loss = 0.0
$port = 0
$hosts = @()
$assignments = [System.Collections.Generic.List[object]]::new()
$launched = [System.Collections.Generic.List[object]]::new()
$authenticatedIdentities = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$workloadIdentities = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$fpsValues = [System.Collections.Generic.List[double]]::new()
$frameValues = [System.Collections.Generic.List[double]]::new()
$tickValues = [System.Collections.Generic.List[double]]::new()
$rttValues = [System.Collections.Generic.List[double]]::new()
$replicationValues = [System.Collections.Generic.List[double]]::new()
$vramValues = [System.Collections.Generic.List[double]]::new()
$cpuValues = [System.Collections.Generic.List[double]]::new()
$ramValues = [System.Collections.Generic.List[double]]::new()
$corrections = 0
$missedSteps = 0
$maxStepValues = [System.Collections.Generic.List[double]]::new()
$serverPerfObserved = $false
$serverPerfRead = $false
$runtimeTelemetryObserved = $false
$launchObserved = $false
$runStartedUtc = (Get-Date).ToUniversalTime().ToString('o')
$workload = @('movement', 'combat', 'vehicle')
# Per-district required workload. The Social district (Breakwater Marina) is a non-combat
# safe zone: no weapons drawn and no drivable vehicles in the plaza, so its clients can only
# ever satisfy the movement workload. Every other (action) district is combat-capable and
# runs the full taxonomy. $workload stays the canonical taxonomy emitted in
# load_contract.workload (telemetry schema requires >=3 entries); $districtWorkload is what
# clients are actually told to run and what workload receipts are validated against. It is
# resolved to the real district id after district resolution below.
$socialDistrictIds = @('Social')
$districtWorkload = $workload

function Add-RawLogLine {
    param([Parameter(Mandatory)][string]$Line)
    $parent = Split-Path -Parent $RawLogPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    Add-Content -LiteralPath $RawLogPath -Encoding utf8 -Value "$(Get-Date -Format o) $Line"
}

function Set-Failure {
    param([Parameter(Mandatory)][string]$Reason)
    if ([string]::IsNullOrWhiteSpace($script:failure)) {
        $script:failure = $Reason
        Write-Host "LOAD_GATE_CONFIG_FAIL reason=$Reason"
        Add-RawLogLine "LOAD_GATE_CONFIG_FAIL reason=$Reason"
    }
}

function ConvertTo-ApbInteger {
    param([string]$Text, [string]$Name, [int]$Minimum, [int]$Maximum)
    $parsed = 0
    if ([string]::IsNullOrWhiteSpace($Text) -or -not [int]::TryParse($Text, [ref]$parsed)) { Set-Failure "malformed_${Name}:$Text"; return 0 }
    if ($parsed -lt $Minimum -or $parsed -gt $Maximum) { Set-Failure "${Name}_out_of_range:$parsed"; return $parsed }
    return $parsed
}

function ConvertTo-ApbDecimal {
    param([string]$Text, [string]$Name, [double]$Minimum, [double]$Maximum)
    $parsed = 0.0
    if ([string]::IsNullOrWhiteSpace($Text) -or -not [double]::TryParse($Text, [Globalization.NumberStyles]::Float, [Globalization.CultureInfo]::InvariantCulture, [ref]$parsed) -or -not (Test-ApbFiniteNumber $parsed)) { Set-Failure "malformed_${Name}:$Text"; return 0.0 }
    if ($parsed -lt $Minimum -or $parsed -gt $Maximum) { Set-Failure "${Name}_out_of_range:$parsed"; return 0.0 }
    return $parsed
}

function Test-LocalHostAddress {
    param([Parameter(Mandatory)][string]$Address)
    return $Address -in @('127.0.0.1', 'localhost', '.', $env:COMPUTERNAME, [Environment]::MachineName)
}

function Get-OptionalHostString {
    param([Parameter(Mandatory)]$HostEntry, [Parameter(Mandatory)][string]$Name)
    $property = $HostEntry.PSObject.Properties[$Name]
    if ($null -eq $property) { return '' }
    return [string]$property.Value
}

function Add-MetricMatches {
    param([string]$Raw, [string]$Name, [System.Collections.Generic.List[double]]$Destination)
    foreach ($match in [regex]::Matches($Raw, "(?im)\b$Name\s*[:=]\s*([0-9]+(?:\.[0-9]+)?)")) { $Destination.Add([double]$match.Groups[1].Value) }
}

function Read-AssignmentProbeLog {
    param([Parameter(Mandatory)]$Assignment)
    $raw = ''
    try {
        if ($Assignment.transport -eq 'local') {
            if (Test-Path -LiteralPath $Assignment.scratch -PathType Container) {
                $raw = (@(Get-ChildItem -LiteralPath $Assignment.scratch -Filter '*.log' -File -ErrorAction SilentlyContinue | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
            }
        } else {
            $raw = Invoke-Command -ComputerName $Assignment.address -ErrorAction Stop -ScriptBlock {
                param($Scratch)
                if (Test-Path -LiteralPath $Scratch -PathType Container) {
                    return (@(Get-ChildItem -LiteralPath $Scratch -Filter '*.log' -File -ErrorAction SilentlyContinue | ForEach-Object { Get-Content -LiteralPath $_.FullName -Raw }) -join [Environment]::NewLine)
                }
                return ''
            } -ArgumentList $Assignment.scratch
        }
    } catch {
        Add-RawLogLine "LOAD_GATE_LOG_READ_FAIL identity=$($Assignment.identity) reason=$($_.Exception.Message)"
        return
    }
    if ([string]::IsNullOrWhiteSpace($raw)) { return }
    Add-Content -LiteralPath $RawLogPath -Encoding utf8 -Value $raw
    if ($raw -match '(?im)\b(?:APB_PERF|APB_PERF_METRIC)\b') { $script:runtimeTelemetryObserved = $true }
    if ($raw -match "(?im)WORLD_CLIENT_OK\b.*\bid=$([regex]::Escape($Assignment.identity))\b") { [void]$authenticatedIdentities.Add($Assignment.identity) }
    # Require <action>=1 only for the actions this assignment was told to run. The completion
    # line always emits movement/combat/vehicle in that canonical order, so filtering the
    # required actions into canonical order keeps the '.*' separators a valid forward match
    # (a Social movement-only client emits 'movement=1 combat=0 vehicle=0' and still passes).
    $canonicalActionOrder = @('movement', 'combat', 'vehicle')
    $requiredActions = @($Assignment.workload_sequence)
    $requiredTokenPattern = (($canonicalActionOrder | Where-Object { $requiredActions -contains $_ } | ForEach-Object { "\b$_=1\b" }) -join '.*')
    $completionPattern = "(?im)APB_LOAD_WORKLOAD_COMPLETE\b.*\bidentity=$([regex]::Escape($Assignment.identity))\b"
    if (-not [string]::IsNullOrWhiteSpace($requiredTokenPattern)) { $completionPattern += ".*$requiredTokenPattern" }
    if ($raw -match $completionPattern) { [void]$workloadIdentities.Add($Assignment.identity) }
    Add-MetricMatches $raw 'FPS' $fpsValues
    Add-MetricMatches $raw 'Frame(?:Time|Ms)' $frameValues
    Add-MetricMatches $raw 'Tick(?:Time|Ms)?' $tickValues
    Add-MetricMatches $raw 'RTT' $rttValues
    Add-MetricMatches $raw 'Replication(?:Rate)?' $replicationValues
    Add-MetricMatches $raw 'VRAM(?:_MB|MB)?' $vramValues
    $script:corrections += @([regex]::Matches($raw, '(?im)\bcorrection(?:s)?\s*[:=]\s*[1-9][0-9]*')).Count
    $script:missedSteps += @([regex]::Matches($raw, '(?im)\bmissed[_ ](?:authoritative[_ ])?steps?\s*[:=]\s*[1-9][0-9]*')).Count
}

function Read-ServerPerfLog {
    # Single-shot: the authoritative listen-server appends scope=server APB_PERF_METRIC
    # lines to -APBPerfLog. Clients never emit a Tick token, so server TickMs is the ONLY
    # source that satisfies the gate's tick-metric requirement on a live run. Guarded so the
    # collection-loop call and the finally-block call cannot double-count missed steps.
    if ([string]::IsNullOrWhiteSpace($ServerPerfLog) -or $script:serverPerfRead) { return }
    if (-not (Test-Path -LiteralPath $ServerPerfLog -PathType Leaf)) { return }
    $script:serverPerfRead = $true
    try {
        $raw = Get-Content -LiteralPath $ServerPerfLog -Raw -ErrorAction Stop
    } catch {
        Add-RawLogLine "LOAD_GATE_SERVER_PERF_READ_FAIL path=$ServerPerfLog reason=$($_.Exception.Message)"
        return
    }
    if ([string]::IsNullOrWhiteSpace($raw)) { return }
    $serverLines = @([regex]::Matches($raw, '(?im)^.*APB_PERF_METRIC\b.*scope=server\b.*$') | ForEach-Object { $_.Value })
    if ($serverLines.Count -eq 0) { return }
    $joined = $serverLines -join [Environment]::NewLine
    Add-Content -LiteralPath $RawLogPath -Encoding utf8 -Value $joined
    $script:serverPerfObserved = $true
    $script:runtimeTelemetryObserved = $true
    Add-MetricMatches $joined 'Tick(?:Time|Ms)?' $tickValues
    Add-MetricMatches $joined 'MaxStepMs' $maxStepValues
    $script:corrections += @([regex]::Matches($joined, '(?im)\bcorrection(?:s)?\s*[:=]\s*[1-9][0-9]*')).Count
    $script:missedSteps += @([regex]::Matches($joined, '(?im)\bmissed[_ ](?:authoritative[_ ])?steps?\s*[:=]\s*[1-9][0-9]*')).Count
}

function Start-LoadAssignment {
    param([Parameter(Mandatory)]$Assignment)

    $arguments = @(
        $Assignment.project,
        "$ServerAddress`:$port",
        '-game', '-nullrhi', '-nosound', '-unattended', '-nosplash', '-log',
        '-APBProbe=world_server_client',
        "-WorldServerHost=$ServerAddress",
        "-WSClientId=$($Assignment.identity)",
        "-APBLoadIdentity=$($Assignment.identity)",
        "-APBLoadAccount=$($Assignment.account)",
        "-APBLoadMap=$mapAsset",
        "-APBLoadWorkload=$($Assignment.workload_sequence -join ',')",
        "-APBLoadPrimary=$($Assignment.workload_primary)",
        "-APBScratch=$($Assignment.scratch)",
        "-NetPktLag=$latency",
        "-NetPktLoss=$loss"
    )
    if ($Assignment.transport -eq 'local') {
        $process = Start-Process -FilePath $Assignment.editor -ArgumentList $arguments -PassThru -WorkingDirectory (Split-Path -Parent $Assignment.editor)
        return [pscustomobject]@{ identity = $Assignment.identity; host = $Assignment.host; address = $Assignment.address; transport = 'local'; process_id = $process.Id; process = $process; assignment = $Assignment; alive = $true }
    }
    $remote = Invoke-Command -ComputerName $Assignment.address -ErrorAction Stop -ScriptBlock {
        param($Editor, $Arguments, $WorkingDirectory)
        $process = Start-Process -FilePath $Editor -ArgumentList $Arguments -PassThru -WorkingDirectory $WorkingDirectory
        return [pscustomobject]@{ process_id = $process.Id }
    } -ArgumentList $Assignment.editor, $arguments, (Split-Path -Parent $Assignment.editor)
    return [pscustomobject]@{ identity = $Assignment.identity; host = $Assignment.host; address = $Assignment.address; transport = 'powershell-remoting'; process_id = [int]$remote.process_id; process = $null; assignment = $Assignment; alive = $true }
}

function Test-LoadProcessAlive {
    param([Parameter(Mandatory)]$Launch)
    try {
        if ($Launch.transport -eq 'local') {
            Get-Process -Id $Launch.process_id -ErrorAction Stop | Out-Null
        } else {
            Invoke-Command -ComputerName $Launch.address -ErrorAction Stop -ScriptBlock { param($ProcessId) Get-Process -Id $ProcessId -ErrorAction Stop | Out-Null } -ArgumentList $Launch.process_id | Out-Null
        }
        return $true
    } catch {
        return $false
    }
}

function Stop-LoadProcess {
    param([Parameter(Mandatory)]$Launch)
    try {
        if ($Launch.transport -eq 'local') {
            $active = Get-Process -Id $Launch.process_id -ErrorAction SilentlyContinue
            if ($active) { Stop-Process -Id $Launch.process_id -Force -ErrorAction Stop; return $true }
        } else {
            return [bool](Invoke-Command -ComputerName $Launch.address -ErrorAction Stop -ScriptBlock { param($ProcessId) $active = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue; if ($active) { Stop-Process -Id $ProcessId -Force -ErrorAction Stop; return $true }; return $false } -ArgumentList $Launch.process_id)
        }
    } catch {
        Add-RawLogLine "LOAD_GATE_CLEANUP_FAIL identity=$($Launch.identity) process_id=$($Launch.process_id) reason=$($_.Exception.Message)"
    }
    return $false
}

function New-Result {
    param([Parameter(Mandatory)][string]$Marker)
    $hardware = Get-ApbHardware
    $frameInput = @($frameValues)
    if ($frameInput.Count -eq 0 -and $fpsValues.Count -gt 0) { $frameInput = @($fpsValues | ForEach-Object { 1000.0 / $_ }) }
    $metrics = New-ApbMetrics -Fps @($fpsValues) -FrameMs $frameInput -TickMs @($tickValues) -RttMs @($rttValues) -ReplicationRate @($replicationValues) -CpuPercent $(if ($cpuValues.Count) { (Get-ApbPercentiles @($cpuValues)).p95 } else { 0.0 }) -RamMb $(if ($ramValues.Count) { (Get-ApbPercentiles @($ramValues)).p95 } else { 0.0 }) -VramMb $(if ($vramValues.Count) { (Get-ApbPercentiles @($vramValues)).p95 } else { 0.0 }) -PacketLossPercent $loss -Corrections $corrections -MissedAuthoritativeSteps $missedSteps
    $observed = $workloadIdentities.Count
    return [ordered]@{
        schema_version = 'apb.perf.v1'
        captured_at_utc = (Get-Date).ToUniversalTime().ToString('o')
        terminal_marker = $Marker
        run = [ordered]@{
            map = if ([string]::IsNullOrWhiteSpace($Map)) { '(empty)' } else { $Map }
            map_asset = $mapAsset
            scalability_profile = if ([string]::IsNullOrWhiteSpace($ScalabilityProfile)) { '(empty)' } else { $ScalabilityProfile }
            clients = [int]$clientCount
            duration_seconds = [int][math]::Max(0, $duration)
            latency_ms = [double][math]::Max(0.0, $latency)
            packet_loss_percent = [double][math]::Max(0.0, $loss)
            build_sha = Get-ApbBuildSha $root
        }
        hardware = $hardware
        metrics = $metrics
        evidence = [ordered]@{
            raw_log = (Resolve-Path -LiteralPath $RawLogPath -ErrorAction SilentlyContinue).Path ?? $RawLogPath
            launch_observed = [bool]($launched.Count -gt 0)
            synthetic = [bool](-not $runtimeTelemetryObserved)
            claims_64_player = [bool]($clientCount -eq 64 -and $observed -eq 64 -and $runtimeTelemetryObserved -and [string]::IsNullOrWhiteSpace($failure))
        }
        load_contract = [ordered]@{
            hosts_manifest = (Resolve-Path -LiteralPath $HostsPath -ErrorAction SilentlyContinue).Path ?? $HostsPath
            workload = $workload
            identities = @($assignments | ForEach-Object { $_.identity })
            assignments = @($assignments)
            observed_live_clients = [int]$observed
            required_live_clients = [int]$clientCount
        }
        failure_reason = $failure
        cleanup = [ordered]@{
            run_started_utc = $runStartedUtc
            process_ids_started = @($launched | ForEach-Object { $_.process_id })
            process_ids_stopped = @()
        }
    }
}

try {
    $clientCount = ConvertTo-ApbInteger $Clients 'clients' 1 64
    $duration = ConvertTo-ApbInteger $DurationSeconds 'duration_seconds' 1 86400
    $latency = ConvertTo-ApbDecimal $LatencyMs 'latency_ms' 0 600000
    $loss = ConvertTo-ApbDecimal $PacketLossPercent 'loss_percent' 0 100
    $port = ConvertTo-ApbInteger $ServerPort 'server_port' 1 65535
    if (-not $failure -and [string]::IsNullOrWhiteSpace($Map)) { Set-Failure 'unknown_map:empty' }

    $district = $null
    if (-not $failure) {
        $district = Get-ApbDistrictRecord -DistrictPath (Join-Path $root 'Content\Data\districts.json') -Map $Map
        if ($null -eq $district) { Set-Failure "unknown_map:$Map" }
        elseif ($district.MapAssetCount -ne 1) { Set-Failure "ambiguous_map_asset:$Map" }
        else {
            $mapAsset = $district.MapAsset
            if (-not [bool]$district.Record.joinable) { Set-Failure "map_not_joinable:$Map" }
            elseif ([int]$district.Record.max_players -lt $clientCount) { Set-Failure "clients_exceed_map_capacity:$clientCount/$($district.Record.max_players)" }
            else {
                # Resolve required workload from the real district. Social (safe zone) runs
                # movement only; every combat-capable district runs the full taxonomy.
                [string[]]$districtWorkload = if ($socialDistrictIds -contains [string]$district.Record.id) { @('movement') } else { $workload }
                Add-RawLogLine "LOAD_GATE_WORKLOAD district=$($district.Record.id) required=$($districtWorkload -join ',')"
            }
        }
    }
    if (-not $failure -and -not (Test-Path -LiteralPath $HostsPath -PathType Leaf)) { Set-Failure "missing_hosts_manifest:$HostsPath" }
    $manifest = $null
    if (-not $failure) {
        try { $manifest = Get-Content -LiteralPath $HostsPath -Raw | ConvertFrom-Json } catch { Set-Failure "malformed_hosts_manifest:$($_.Exception.Message)" }
    }
    if (-not $failure -and $manifest.schema_version -ne 'apb.load-hosts.v1') { Set-Failure 'invalid_hosts_schema' }
    if (-not $failure) { $hosts = @($manifest.hosts | Where-Object { $_.enabled -eq $true }) }
    if (-not $failure -and $hosts.Count -eq 0) { Set-Failure 'no_enabled_hosts' }
    if (-not $failure) {
        $seenNames = [System.Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
        $totalCapacity = 0
        foreach ($hostEntry in $hosts) {
            $name = [string]$hostEntry.name
            $address = [string]$hostEntry.address
            $editor = [string]$hostEntry.editor
            $project = [string]$hostEntry.project
            $capacity = 0
            $validCapacity = [int]::TryParse([string]$hostEntry.max_clients, [ref]$capacity) -and $capacity -gt 0
            $transport = Get-OptionalHostString $hostEntry 'launch_transport'
            if ([string]::IsNullOrWhiteSpace($transport)) { $transport = if (Test-LocalHostAddress $address) { 'local' } else { 'powershell-remoting' } }
            if ([string]::IsNullOrWhiteSpace($name) -or -not $seenNames.Add($name) -or [string]::IsNullOrWhiteSpace($address) -or -not $validCapacity -or [string]::IsNullOrWhiteSpace($editor) -or [string]::IsNullOrWhiteSpace($project) -or $transport -notin @('local', 'powershell-remoting')) {
                Set-Failure "invalid_host:$name"
                break
            }
            if ($transport -eq 'local' -and ((-not (Test-LocalHostAddress $address)) -or -not (Test-Path -LiteralPath $editor -PathType Leaf) -or -not (Test-Path -LiteralPath $project -PathType Leaf))) {
                Set-Failure "invalid_local_host:$name"
                break
            }
            $totalCapacity += $capacity
            $hostEntry | Add-Member -NotePropertyName resolved_transport -NotePropertyValue $transport -Force
        }
        if (-not $failure -and $totalCapacity -lt $clientCount) { Set-Failure "insufficient_host_capacity:$totalCapacity/$clientCount" }
    }
    if (-not $failure) {
        $hostUsage = @{}
        foreach ($hostEntry in $hosts) { $hostUsage[[string]$hostEntry.name] = 0 }
        for ($identityIndex = 1; $identityIndex -le $clientCount; $identityIndex++) {
            $selected = $null
            $selectedUtilization = [double]::PositiveInfinity
            foreach ($hostEntry in $hosts) {
                $used = [int]$hostUsage[[string]$hostEntry.name]
                $capacity = [int]$hostEntry.max_clients
                if ($used -lt $capacity) {
                    $utilization = [double]$used / [double]$capacity
                    if ($null -eq $selected -or $utilization -lt $selectedUtilization) {
                        $selected = $hostEntry
                        $selectedUtilization = $utilization
                    }
                }
            }
            if ($null -eq $selected) { Set-Failure 'assignment_overflow'; break }
            $hostUsage[[string]$selected.name]++
            $identity = 'load_{0:D3}' -f $identityIndex
            $scratchRoot = Get-OptionalHostString $selected 'scratch_root'
            if ([string]::IsNullOrWhiteSpace($scratchRoot)) {
                $scratchRoot = if ($selected.resolved_transport -eq 'local') { Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($RawLogPath))) 'load_scratch' } else { 'C:\APBReloadedLoadScratch' }
            }
            $assignments.Add([pscustomobject]@{
                    ordinal = $identityIndex
                    identity = $identity
                    account = "probe_$identity"
                    authentication = 'world_server_client_rpc'
                    host = [string]$selected.name
                    address = [string]$selected.address
                    transport = [string]$selected.resolved_transport
                    editor = [string]$selected.editor
                    project = [string]$selected.project
                    workload_primary = $districtWorkload[($identityIndex - 1) % $districtWorkload.Count]
                    workload_sequence = $districtWorkload
                    scratch = Join-Path $scratchRoot $identity
                })
        }
    }
    Add-RawLogLine "LOAD_GATE_REQUEST map=$Map clients=$clientCount duration_seconds=$duration latency_ms=$latency loss_percent=$loss"
    foreach ($assignment in $assignments) {
        $line = "LOAD_ASSIGNMENT identity=$($assignment.identity) account=$($assignment.account) host=$($assignment.host) transport=$($assignment.transport) primary=$($assignment.workload_primary) workload=$($assignment.workload_sequence -join ',')"
        Write-Host $line
        Add-RawLogLine $line
    }
    if (-not $failure -and $ValidateOnly) {
        Write-Host "LOAD_CONTRACT_READY clients=$clientCount hosts=$($hosts.Count) assignments=$($assignments.Count) launch=skipped"
        Add-RawLogLine "LOAD_CONTRACT_READY clients=$clientCount hosts=$($hosts.Count) assignments=$($assignments.Count) launch=skipped"
        $failure = 'validate_only_no_launch'
    }
    if (-not $failure) {
        Write-Host "LOAD_GATE_LAUNCH clients=$clientCount duration=$duration workload=$($districtWorkload -join ',')"
        Add-RawLogLine "LOAD_GATE_LAUNCH clients=$clientCount duration=$duration workload=$($districtWorkload -join ',')"
        foreach ($assignment in $assignments) {
            $launch = Start-LoadAssignment $assignment
            $launched.Add($launch)
            Add-RawLogLine "LOAD_GATE_LAUNCHED identity=$($launch.identity) host=$($launch.host) process_id=$($launch.process_id)"
        }
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        $previousCpu = @{}
        $previousSample = [Diagnostics.Stopwatch]::StartNew()
        while ($stopwatch.Elapsed.TotalSeconds -lt $duration) {
            Start-Sleep -Seconds 1
            $elapsedSeconds = [math]::Max(0.001, $previousSample.Elapsed.TotalSeconds)
            $previousSample.Restart()
            foreach ($launch in $launched) {
                if (-not (Test-LoadProcessAlive $launch)) { $launch.alive = $false; $failure = "client_exited:$($launch.identity)"; break }
                if ($launch.transport -eq 'local') {
                    $running = Get-Process -Id $launch.process_id -ErrorAction SilentlyContinue
                    if ($running) {
                        $oldCpu = if ($previousCpu.ContainsKey($launch.process_id)) { $previousCpu[$launch.process_id] } else { 0.0 }
                        if ($oldCpu -gt 0) { $cpuValues.Add([math]::Max(0.0, (([double]$running.CPU - $oldCpu) / $elapsedSeconds) * 100.0 / [Environment]::ProcessorCount)) }
                        $previousCpu[$launch.process_id] = [double]$running.CPU
                        $ramValues.Add([math]::Round([double]$running.WorkingSet64 / 1MB, 3))
                    }
                }
            }
            if ($failure) { break }
        }
        foreach ($assignment in $assignments) { Read-AssignmentProbeLog $assignment }
        Read-ServerPerfLog
        if (-not $failure -and $authenticatedIdentities.Count -ne $clientCount) { $failure = "missing_authenticated_identities:$($authenticatedIdentities.Count)/$clientCount" }
        if (-not $failure -and $workloadIdentities.Count -ne $clientCount) { $failure = "missing_workload_receipts:$($workloadIdentities.Count)/$clientCount" }
        if (-not $failure -and -not $runtimeTelemetryObserved) { $failure = 'missing_runtime_telemetry_marker' }
        if (-not $failure -and $fpsValues.Count -eq 0) { $failure = 'missing_fps_metrics' }
        if (-not $failure -and $tickValues.Count -eq 0) { $failure = 'missing_tick_metrics' }
        if (-not $failure -and -not [string]::IsNullOrWhiteSpace($ServerPerfLog)) {
            # Task-17 30Hz authoritative-step SLA. missed/corrections are hard cadence guards
            # (zero tolerance); MaxStepMs p99 is the per-step compute budget (1/30s = 33.3ms).
            if (-not $serverPerfObserved) { $failure = 'missing_server_perf_telemetry' }
            elseif ($missedSteps -gt 0) { $failure = "authoritative_step_misses:$missedSteps" }
            elseif ($corrections -gt 0) { $failure = "authoritative_corrections:$corrections" }
            else {
                $stepBudgetMs = 1000.0 / 30.0
                $worstStepP99 = if ($maxStepValues.Count) { (Get-ApbPercentiles @($maxStepValues)).p99 } else { 0.0 }
                Add-RawLogLine ("LOAD_GATE_SERVER_BUDGET windows={0} max_step_p99_ms={1} budget_ms={2} missed={3} corrections={4}" -f $maxStepValues.Count, $worstStepP99, [math]::Round($stepBudgetMs, 3), $missedSteps, $corrections)
                if ($worstStepP99 -gt $stepBudgetMs) { $failure = "authoritative_step_over_budget:${worstStepP99}ms/$([math]::Round($stepBudgetMs, 3))ms" }
            }
        }
    }
} catch {
    if ([string]::IsNullOrWhiteSpace($failure)) {
        $failure = "exception:$($_.Exception.Message)"
        Write-Host "LOAD_GATE_CONFIG_FAIL reason=$failure line=$($_.InvocationInfo.ScriptLineNumber)"
        Add-RawLogLine "LOAD_GATE_CONFIG_FAIL reason=$failure"
    }
} finally {
    $stoppedIds = @()
    foreach ($launch in $launched) {
        if (Stop-LoadProcess $launch) {
            $stoppedIds += $launch.process_id
            Add-RawLogLine "LOAD_GATE_CLEANUP stopped_identity=$($launch.identity) process_id=$($launch.process_id)"
        }
    }
    if (-not $ValidateOnly) {
        foreach ($assignment in $assignments) { Read-AssignmentProbeLog $assignment }
        Read-ServerPerfLog
    }
    if (-not $launchObserved -and $launched.Count -gt 0) { $launchObserved = $true }
    $marker = if ([string]::IsNullOrWhiteSpace($failure)) { 'LOAD_GATE_OK' } else { 'LOAD_GATE_FAIL' }
    Add-RawLogLine "$marker reason=$failure"
    $result = New-Result $marker
    $result.cleanup.process_ids_stopped = $stoppedIds
    Write-ApbJson -Path $OutputPath -Value $result
    $validation = Test-ApbPerfTelemetry -Telemetry $result -RequireLoadContract
    if (-not $validation.Valid) {
        Write-Host "LOAD_GATE_FAIL reason=telemetry_schema_invalid:$($validation.Error) output=$OutputPath raw=$RawLogPath"
        exit 1
    }
    Write-Host "$marker reason=$failure output=$OutputPath raw=$RawLogPath"
    if ($ValidateOnly -and $failure -eq 'validate_only_no_launch') { exit 0 }
    if ($marker -eq 'LOAD_GATE_OK') { exit 0 }
    exit 1
}
