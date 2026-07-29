[CmdletBinding()]
param(
    [string]$Map = 'Social',
    [string]$Clients = '1',
    [Alias('Duration')][string]$DurationSeconds = '60',
    [Alias('Latency')][string]$LatencyMs = '0',
    [Alias('Loss', 'LossPercent')][string]$PacketLossPercent = '0',
    [string]$ScalabilityProfile = 'High',
    [string]$MinFps = '0',
    [string]$OutputPath = '',
    [string]$RawLogPath = '',
    [string]$Project = 'D:\APBReloaded\APBReloaded.uproject',
    [string]$Editor = 'D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe',
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
Import-Module (Join-Path $PSScriptRoot 'PerfTelemetry.psm1') -Force

if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    $OutputPath = Join-Path $root ('work\perf_{0:yyyyMMdd_HHmmss}.json' -f (Get-Date))
}
if ([string]::IsNullOrWhiteSpace($RawLogPath)) {
    $RawLogPath = [IO.Path]::ChangeExtension($OutputPath, '.log')
}
$rawLogParent = Split-Path -Parent $RawLogPath
if (-not [string]::IsNullOrWhiteSpace($rawLogParent)) {
    New-Item -ItemType Directory -Force -Path $rawLogParent | Out-Null
}
Set-Content -LiteralPath $RawLogPath -Encoding utf8 -Value "$(Get-Date -Format o) PERF_GATE_RAW_LOG_START"

$failure = ''
$process = $null
$launchObserved = $false
$mapAsset = 'unresolved:' + $(if ([string]::IsNullOrWhiteSpace($Map)) { 'empty' } else { $Map })
$clientCount = 0
$duration = 0
$latency = 0.0
$loss = 0.0
$minimumFps = 0.0
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
$runtimeTelemetryObserved = $false
$runStartedUtc = (Get-Date).ToUniversalTime().ToString('o')

function Add-RawLogLine {
    param([Parameter(Mandatory)][string]$Line)

    $parent = Split-Path -Parent $RawLogPath
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    Add-Content -LiteralPath $RawLogPath -Encoding utf8 -Value "$(Get-Date -Format o) $Line"
}

function Set-Failure {
    param([Parameter(Mandatory)][string]$Reason)
    if ([string]::IsNullOrWhiteSpace($script:failure)) {
        $script:failure = $Reason
        Write-Host "PERF_GATE_CONFIG_FAIL reason=$Reason"
        Add-RawLogLine "PERF_GATE_CONFIG_FAIL reason=$Reason"
    }
}

function ConvertTo-ApbInteger {
    param([string]$Text, [string]$Name, [int]$Minimum, [int]$Maximum)

    $parsed = 0
    if ([string]::IsNullOrWhiteSpace($Text) -or -not [int]::TryParse($Text, [ref]$parsed)) {
        Set-Failure "malformed_${Name}:$Text"
        return 0
    }
    if ($parsed -lt $Minimum -or $parsed -gt $Maximum) {
        Set-Failure "${Name}_out_of_range:$parsed"
        return $parsed
    }
    return $parsed
}

function ConvertTo-ApbDecimal {
    param([string]$Text, [string]$Name, [double]$Minimum, [double]$Maximum)

    $parsed = 0.0
    $style = [Globalization.NumberStyles]::Float
    $culture = [Globalization.CultureInfo]::InvariantCulture
    if ([string]::IsNullOrWhiteSpace($Text) -or -not [double]::TryParse($Text, $style, $culture, [ref]$parsed) -or -not (Test-ApbFiniteNumber $parsed)) {
        Set-Failure "malformed_${Name}:$Text"
        return 0.0
    }
    if ($parsed -lt $Minimum -or $parsed -gt $Maximum) {
        Set-Failure "${Name}_out_of_range:$parsed"
        return 0.0
    }
    return $parsed
}

function Add-MetricMatches {
    param(
        [Parameter(Mandatory)][string]$Raw,
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][AllowEmptyCollection()][System.Collections.Generic.List[double]]$Destination
    )

    foreach ($match in [regex]::Matches($Raw, "(?im)\b$Name\s*[:=]\s*([0-9]+(?:\.[0-9]+)?)")) {
        $Destination.Add([double]$match.Groups[1].Value)
    }
}

function Collect-EngineTelemetry {
    param([string]$EngineLogPath)

    if ([string]::IsNullOrWhiteSpace($EngineLogPath) -or -not (Test-Path -LiteralPath $EngineLogPath -PathType Leaf)) {
        return
    }
    $raw = Get-Content -LiteralPath $EngineLogPath -Raw
    Add-Content -LiteralPath $RawLogPath -Encoding utf8 -Value $raw
    if ($raw -match '(?im)\b(?:APB_PERF|APB_PERF_METRIC)\b') {
        $script:runtimeTelemetryObserved = $true
    }
    Add-MetricMatches $raw 'FPS' $fpsValues
    Add-MetricMatches $raw 'Frame(?:Time|Ms)' $frameValues
    Add-MetricMatches $raw 'Tick(?:Time|Ms)?' $tickValues
    Add-MetricMatches $raw 'RTT' $rttValues
    Add-MetricMatches $raw 'Replication(?:Rate)?' $replicationValues
    Add-MetricMatches $raw 'VRAM(?:_MB|MB)?' $vramValues
    $script:corrections += @([regex]::Matches($raw, '(?im)\bcorrection(?:s)?\s*[:=]\s*[1-9][0-9]*')).Count
    $script:missedSteps += @([regex]::Matches($raw, '(?im)\bmissed[_ ](?:authoritative[_ ])?steps?\s*[:=]\s*[1-9][0-9]*')).Count
}

function New-Result {
    param([Parameter(Mandatory)][string]$Marker)

    $hardware = Get-ApbHardware
    $frameInput = @($frameValues)
    if ($frameInput.Count -eq 0 -and $fpsValues.Count -gt 0) {
        $frameInput = @($fpsValues | ForEach-Object { 1000.0 / $_ })
    }
    $metrics = New-ApbMetrics -Fps @($fpsValues) -FrameMs $frameInput -TickMs @($tickValues) -RttMs @($rttValues) -ReplicationRate @($replicationValues) -CpuPercent $(if ($cpuValues.Count) { (Get-ApbPercentiles @($cpuValues)).p95 } else { 0.0 }) -RamMb $(if ($ramValues.Count) { (Get-ApbPercentiles @($ramValues)).p95 } else { 0.0 }) -VramMb $(if ($vramValues.Count) { (Get-ApbPercentiles @($vramValues)).p95 } else { 0.0 }) -PacketLossPercent $loss -Corrections $corrections -MissedAuthoritativeSteps $missedSteps
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
            launch_observed = [bool]$launchObserved
            synthetic = [bool](-not $runtimeTelemetryObserved)
            claims_64_player = $false
        }
        failure_reason = $failure
        cleanup = [ordered]@{
            run_started_utc = $runStartedUtc
            process_ids_started = @($process | Where-Object { $null -ne $_ } | ForEach-Object { $_.Id })
            process_ids_stopped = @()
        }
    }
}

try {
    $clientCount = ConvertTo-ApbInteger $Clients 'clients' 1 64
    $duration = ConvertTo-ApbInteger $DurationSeconds 'duration_seconds' 1 86400
    $latency = ConvertTo-ApbDecimal $LatencyMs 'latency_ms' 0 600000
    $loss = ConvertTo-ApbDecimal $PacketLossPercent 'loss_percent' 0 100
    $minimumFps = ConvertTo-ApbDecimal $MinFps 'min_fps' 0 100000
    if ($clientCount -gt 1 -and -not $failure) {
        Set-Failure 'perf_gate_single_client_only_use_run_64_client_gate'
    }

    $district = $null
    if (-not $failure -and [string]::IsNullOrWhiteSpace($Map)) {
        Set-Failure 'unknown_map:empty'
    }
    if (-not $failure) {
        $district = Get-ApbDistrictRecord -DistrictPath (Join-Path $root 'Content\Data\districts.json') -Map $Map
        if ($null -eq $district) {
            Set-Failure "unknown_map:$Map"
        } elseif ($district.MapAssetCount -ne 1) {
            Set-Failure "ambiguous_map_asset:$Map"
        } else {
            $mapAsset = $district.MapAsset
            if (-not [bool]$district.Record.joinable) {
                Set-Failure "map_not_joinable:$Map"
            } elseif ([int]$district.Record.max_players -lt $clientCount) {
                Set-Failure "clients_exceed_map_capacity:$clientCount/$($district.Record.max_players)"
            }
        }
    }
    $configuredMaxFps = Get-ApbConfiguredMaxFps (Join-Path $root 'Config\DefaultEngine.ini')
    if (-not $failure -and $configuredMaxFps -gt 0 -and $minimumFps -gt $configuredMaxFps) {
        Set-Failure "impossible_min_fps:$minimumFps`_exceeds_configured_cap:$configuredMaxFps"
    }
    if (-not $failure -and -not (Test-Path -LiteralPath $Project -PathType Leaf)) {
        Set-Failure "missing_project:$Project"
    }
    if (-not $failure -and -not (Test-Path -LiteralPath $Editor -PathType Leaf)) {
        Set-Failure "missing_editor:$Editor"
    }

    Add-RawLogLine "PERF_GATE_REQUEST map=$Map clients=$clientCount duration_seconds=$duration latency_ms=$latency loss_percent=$loss"
    if (-not $failure -and $DryRun) {
        $failure = 'dry_run_no_launch'
        Write-Host 'PERF_GATE_DRY_RUN preflight=valid launch=skipped'
        Add-RawLogLine 'PERF_GATE_DRY_RUN preflight=valid launch=skipped'
    }
    if (-not $failure) {
        $scratch = Join-Path ([IO.Path]::GetDirectoryName([IO.Path]::GetFullPath($RawLogPath))) ('perf_scratch_{0:yyyyMMdd_HHmmss_ffff}' -f (Get-Date))
        New-Item -ItemType Directory -Force -Path $scratch | Out-Null
        $engineLogFile = Join-Path $scratch 'perf_engine.log'
        $engineLogBefore = Get-Date
        $arguments = @(
            $Project,
            "/Game/Maps/$mapAsset",
            '-game', '-nullrhi', '-nosound', '-unattended', '-nosplash', '-log',
            "-abslog=$engineLogFile",
            '-APBProbe=mp_observe',
            "-APBScratch=$scratch",
            "-NetPktLag=$latency",
            "-NetPktLoss=$loss",
            '-ExecCmds=stat fps;stat unit;stat net'
        )
        Write-Host "PERF_GATE_LAUNCH map=$Map map_asset=$mapAsset clients=$clientCount duration=$duration"
        Add-RawLogLine "PERF_GATE_LAUNCH arguments=$($arguments -join ' ')"
        $process = Start-Process -FilePath $Editor -ArgumentList $arguments -PassThru -WorkingDirectory (Split-Path -Parent $Editor)
        $launchObserved = $true
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        $previousCpu = 0.0
        $previousSample = [Diagnostics.Stopwatch]::StartNew()
        while ($stopwatch.Elapsed.TotalSeconds -lt $duration) {
            Start-Sleep -Milliseconds 1000
            try {
                $running = Get-Process -Id $process.Id -ErrorAction Stop
                $elapsedSeconds = [math]::Max(0.001, $previousSample.Elapsed.TotalSeconds)
                $previousSample.Restart()
                $cpuSeconds = [double]$running.CPU
                if ($previousCpu -gt 0) {
                    $cpuValues.Add([math]::Max(0.0, (($cpuSeconds - $previousCpu) / $elapsedSeconds) * 100.0 / [Environment]::ProcessorCount))
                }
                $previousCpu = $cpuSeconds
                $ramValues.Add([math]::Round([double]$running.WorkingSet64 / 1MB, 3))
            } catch {
                $failure = 'process_exited_before_duration'
                break
            }
        }
        if (Test-Path -LiteralPath $engineLogFile -PathType Leaf) {
            Add-RawLogLine "PERF_GATE_ENGINE_LOG source=abslog path=$engineLogFile"
            Collect-EngineTelemetry $engineLogFile
        } else {
            $engineLog = Get-ChildItem -LiteralPath (Join-Path $root 'Saved\Logs') -Filter '*.log' -File -ErrorAction SilentlyContinue |
                Where-Object { $_.LastWriteTime -ge $engineLogBefore } |
                Sort-Object LastWriteTime -Descending |
                Select-Object -First 1
            if ($engineLog) {
                Add-RawLogLine "PERF_GATE_ENGINE_LOG source=mtime_fallback path=$($engineLog.FullName)"
                Collect-EngineTelemetry $engineLog.FullName
            }
        }
    }
} catch {
    if ([string]::IsNullOrWhiteSpace($failure)) {
        $failure = "exception:$($_.Exception.Message)"
        Write-Host "PERF_GATE_CONFIG_FAIL reason=$failure line=$($_.InvocationInfo.ScriptLineNumber)"
        Add-RawLogLine "PERF_GATE_CONFIG_FAIL reason=$failure"
    }
} finally {
    $stoppedIds = @()
    if ($process) {
        try {
            if (-not $process.HasExited) {
                Stop-Process -Id $process.Id -Force -ErrorAction Stop
                $stoppedIds += $process.Id
                Add-RawLogLine "PERF_GATE_CLEANUP stopped_process_id=$($process.Id)"
            }
        } catch {
            Add-RawLogLine "PERF_GATE_CLEANUP stop_failed_process_id=$($process.Id)"
        }
    }
    if (-not $launchObserved -and [string]::IsNullOrWhiteSpace($failure)) {
        $failure = 'launch_not_observed'
    }
    if ($launchObserved -and -not $runtimeTelemetryObserved -and [string]::IsNullOrWhiteSpace($failure)) {
        $failure = 'missing_runtime_telemetry_marker'
    }
    if ($runtimeTelemetryObserved -and $fpsValues.Count -eq 0 -and [string]::IsNullOrWhiteSpace($failure)) {
        $failure = 'missing_fps_metrics'
    }
    if ($runtimeTelemetryObserved -and $tickValues.Count -eq 0 -and [string]::IsNullOrWhiteSpace($failure)) {
        $failure = 'missing_tick_metrics'
    }
    if ($minimumFps -gt 0 -and $fpsValues.Count -gt 0 -and (Get-ApbPercentiles @($fpsValues)).p50 -lt $minimumFps -and [string]::IsNullOrWhiteSpace($failure)) {
        $failure = "fps_p50_below_threshold:$((Get-ApbPercentiles @($fpsValues)).p50)/$minimumFps"
    }
    $marker = if ([string]::IsNullOrWhiteSpace($failure)) { 'PERF_GATE_OK' } else { 'PERF_GATE_FAIL' }
    Add-RawLogLine "$marker reason=$failure"
    $result = New-Result $marker
    $result.cleanup.process_ids_stopped = $stoppedIds
    Write-ApbJson -Path $OutputPath -Value $result
    $validation = Test-ApbPerfTelemetry -Telemetry $result
    if (-not $validation.Valid) {
        Write-Host "PERF_GATE_FAIL reason=telemetry_schema_invalid:$($validation.Error) output=$OutputPath raw=$RawLogPath"
        exit 1
    }
    Write-Host "$marker reason=$failure output=$OutputPath raw=$RawLogPath"
    if ($marker -eq 'PERF_GATE_OK') { exit 0 }
    exit 1
}
