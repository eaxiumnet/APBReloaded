Set-StrictMode -Version Latest

function Get-ApbProperty {
    param(
        [AllowNull()]$Object,
        [Parameter(Mandatory)][string]$Name
    )

    if ($null -eq $Object) {
        return [pscustomobject]@{ Found = $false; Value = $null }
    }
    if ($Object -is [System.Collections.IDictionary]) {
        if ($Object.Contains($Name)) {
            return [pscustomobject]@{ Found = $true; Value = $Object[$Name] }
        }
        return [pscustomobject]@{ Found = $false; Value = $null }
    }
    $property = $Object.PSObject.Properties[$Name]
    if ($null -ne $property) {
        return [pscustomobject]@{ Found = $true; Value = $property.Value }
    }
    return [pscustomobject]@{ Found = $false; Value = $null }
}

function Test-ApbFiniteNumber {
    param([AllowNull()]$Value)

    if ($null -eq $Value -or $Value -is [bool] -or $Value -is [string]) {
        return $false
    }
    try {
        $number = [double]$Value
        return -not [double]::IsNaN($number) -and -not [double]::IsInfinity($number)
    } catch {
        return $false
    }
}

function Test-ApbBoolean {
    param([AllowNull()]$Value)
    return $Value -is [bool]
}

function Test-ApbString {
    param([AllowNull()]$Value)
    return $Value -is [string] -and -not [string]::IsNullOrWhiteSpace($Value)
}

function Test-ApbInteger {
    param([AllowNull()]$Value)
    if (-not (Test-ApbFiniteNumber $Value)) {
        return $false
    }
    return [math]::Floor([double]$Value) -eq [double]$Value
}

function Get-ApbPercentiles {
    param([AllowEmptyCollection()][double[]]$Values)

    $numbers = @($Values | Where-Object { -not [double]::IsNaN($_) -and -not [double]::IsInfinity($_) } | Sort-Object)
    if ($numbers.Count -eq 0) {
        return [ordered]@{ p50 = 0.0; p95 = 0.0; p99 = 0.0 }
    }

    $percentile = {
        param([double]$Percent)
        $index = [math]::Min($numbers.Count - 1, [math]::Max(0, [math]::Ceiling(($Percent / 100.0) * $numbers.Count) - 1))
        return [math]::Round([double]$numbers[$index], 3)
    }
    return [ordered]@{ p50 = & $percentile 50; p95 = & $percentile 95; p99 = & $percentile 99 }
}

function Get-ApbBuildSha {
    param([Parameter(Mandatory)][string]$Root)

    try {
        $sha = & git -C $Root rev-parse HEAD 2>$null
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($sha)) {
            return $sha.Trim()
        }
    } catch {
        # The telemetry envelope records an explicit unknown value when git is unavailable.
    }
    return "unknown"
}

function Get-ApbHardware {
    $computer = Get-CimInstance Win32_ComputerSystem -ErrorAction SilentlyContinue
    $operatingSystem = Get-CimInstance Win32_OperatingSystem -ErrorAction SilentlyContinue
    $processor = Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1
    $gpu = Get-CimInstance Win32_VideoController -ErrorAction SilentlyContinue | Select-Object -First 1
    return [ordered]@{
        os = if ($operatingSystem) { [string]$operatingSystem.Caption } else { [string]$env:OS }
        cpu = if ($processor) { [string]$processor.Name } else { "unknown" }
        logical_processors = [int][math]::Max(1, [Environment]::ProcessorCount)
        ram_mb = if ($computer) { [math]::Round([double]$computer.TotalPhysicalMemory / 1MB, 1) } else { 0.0 }
        gpu = if ($gpu) { [string]$gpu.Name } else { "unknown" }
        vram_mb = if ($gpu -and $gpu.AdapterRAM) { [math]::Round([double]$gpu.AdapterRAM / 1MB, 1) } else { 0.0 }
    }
}

function Get-ApbDistrictRecord {
    param(
        [Parameter(Mandatory)][string]$DistrictPath,
        [Parameter(Mandatory)][string]$Map
    )

    $document = Get-Content -LiteralPath $DistrictPath -Raw | ConvertFrom-Json
    $records = if ($document -is [System.Array]) {
        @($document)
    } elseif ($null -ne $document.PSObject.Properties['districts']) {
        @($document.districts)
    } else {
        @($document)
    }

    foreach ($record in $records) {
        $candidates = @([string]$record.id, [string]$record.name) + @($record.map | ForEach-Object { [string]$_ })
        if ($candidates | Where-Object { $_ -ieq $Map }) {
            $mapAssets = @($record.map | ForEach-Object { [string]$_ } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
            return [pscustomobject]@{
                Record = $record
                MapAsset = if ($mapAssets.Count -eq 1) { $mapAssets[0] } else { "" }
                MapAssetCount = $mapAssets.Count
            }
        }
    }
    return $null
}

function Get-ApbConfiguredMaxFps {
    param([Parameter(Mandatory)][string]$DefaultEnginePath)

    if (-not (Test-Path -LiteralPath $DefaultEnginePath -PathType Leaf)) {
        return 0.0
    }
    $limits = @(
        Get-Content -LiteralPath $DefaultEnginePath | ForEach-Object {
            if ($_ -match '^\s*(?:t\.MaxFPS|FrameRateLimit)\s*=\s*([0-9]+(?:\.[0-9]+)?)\s*$') {
                [double]$Matches[1]
            }
        }
    )
    if ($limits.Count -eq 0) {
        return 0.0
    }
    return [double]($limits | Measure-Object -Minimum).Minimum
}

function New-ApbMetrics {
    param(
        [AllowEmptyCollection()][double[]]$Fps = @(),
        [AllowEmptyCollection()][double[]]$FrameMs = @(),
        [AllowEmptyCollection()][double[]]$TickMs = @(),
        [AllowEmptyCollection()][double[]]$RttMs = @(),
        [AllowEmptyCollection()][double[]]$ReplicationRate = @(),
        [double]$CpuPercent = 0.0,
        [double]$RamMb = 0.0,
        [double]$VramMb = 0.0,
        [double]$PacketLossPercent = 0.0,
        [int]$Corrections = 0,
        [int]$MissedAuthoritativeSteps = 0
    )

    return [ordered]@{
        fps = Get-ApbPercentiles $Fps
        frame_ms = Get-ApbPercentiles $FrameMs
        tick_ms = Get-ApbPercentiles $TickMs
        cpu_percent = [math]::Round([math]::Max(0.0, $CpuPercent), 3)
        ram_mb = [math]::Round([math]::Max(0.0, $RamMb), 3)
        vram_mb = [math]::Round([math]::Max(0.0, $VramMb), 3)
        rtt_ms = Get-ApbPercentiles $RttMs
        packet_loss_percent = [math]::Round([math]::Max(0.0, $PacketLossPercent), 3)
        replication_rate = [math]::Round([math]::Max(0.0, (Get-ApbPercentiles $ReplicationRate).p50), 3)
        corrections = [int][math]::Max(0, $Corrections)
        missed_authoritative_steps = [int][math]::Max(0, $MissedAuthoritativeSteps)
    }
}

function Write-ApbJson {
    param(
        [Parameter(Mandatory)][string]$Path,
        [Parameter(Mandatory)]$Value
    )

    $parent = Split-Path -Parent $Path
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Force -Path $parent | Out-Null
    }
    $Value | ConvertTo-Json -Depth 16 | Set-Content -LiteralPath $Path -Encoding utf8
}

function Test-ApbPerfTelemetry {
    param(
        [Parameter(Mandatory)]$Telemetry,
        [switch]$RequireLoadContract
    )

    $rootFields = @('schema_version', 'captured_at_utc', 'terminal_marker', 'run', 'hardware', 'metrics', 'evidence')
    foreach ($field in $rootFields) {
        if (-not (Get-ApbProperty $Telemetry $field).Found) {
            return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:$field" }
        }
    }

    $schemaVersion = (Get-ApbProperty $Telemetry 'schema_version').Value
    if ($schemaVersion -ne 'apb.perf.v1') {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_schema_version:schema_version' }
    }
    foreach ($stringField in @('captured_at_utc', 'terminal_marker')) {
        if (-not (Test-ApbString (Get-ApbProperty $Telemetry $stringField).Value)) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_string:$stringField" }
        }
    }
    $marker = [string](Get-ApbProperty $Telemetry 'terminal_marker').Value
    if ($marker -notin @('PERF_GATE_OK', 'PERF_GATE_FAIL', 'LOAD_GATE_OK', 'LOAD_GATE_FAIL')) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_terminal_marker:terminal_marker' }
    }

    $run = (Get-ApbProperty $Telemetry 'run').Value
    foreach ($field in @('map', 'map_asset', 'scalability_profile', 'clients', 'duration_seconds', 'latency_ms', 'packet_loss_percent', 'build_sha')) {
        if (-not (Get-ApbProperty $run $field).Found) {
            return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:run.$field" }
        }
    }
    foreach ($field in @('map', 'map_asset', 'scalability_profile', 'build_sha')) {
        if (-not (Test-ApbString (Get-ApbProperty $run $field).Value)) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_string:run.$field" }
        }
    }
    $clients = (Get-ApbProperty $run 'clients').Value
    if (-not (Test-ApbInteger $clients) -or [int]$clients -lt 0) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_range:run.clients' }
    }
    foreach ($field in @('duration_seconds', 'latency_ms', 'packet_loss_percent')) {
        $value = (Get-ApbProperty $run $field).Value
        if (-not (Test-ApbFiniteNumber $value) -or [double]$value -lt 0) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_range:run.$field" }
        }
    }
    if ([double](Get-ApbProperty $run 'packet_loss_percent').Value -gt 100) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_range:run.packet_loss_percent' }
    }

    $hardware = (Get-ApbProperty $Telemetry 'hardware').Value
    foreach ($field in @('os', 'cpu', 'logical_processors', 'ram_mb', 'gpu', 'vram_mb')) {
        if (-not (Get-ApbProperty $hardware $field).Found) {
            return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:hardware.$field" }
        }
    }
    foreach ($field in @('os', 'cpu', 'gpu')) {
        if (-not (Test-ApbString (Get-ApbProperty $hardware $field).Value)) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_string:hardware.$field" }
        }
    }
    foreach ($field in @('logical_processors', 'ram_mb', 'vram_mb')) {
        $value = (Get-ApbProperty $hardware $field).Value
        if (-not (Test-ApbFiniteNumber $value) -or [double]$value -lt 0) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_range:hardware.$field" }
        }
    }
    if ([int](Get-ApbProperty $hardware 'logical_processors').Value -lt 1) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_range:hardware.logical_processors' }
    }

    $metrics = (Get-ApbProperty $Telemetry 'metrics').Value
    foreach ($field in @('fps', 'frame_ms', 'tick_ms', 'cpu_percent', 'ram_mb', 'vram_mb', 'rtt_ms', 'packet_loss_percent', 'replication_rate', 'corrections', 'missed_authoritative_steps')) {
        if (-not (Get-ApbProperty $metrics $field).Found) {
            return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:metrics.$field" }
        }
    }
    foreach ($percentileField in @('fps', 'frame_ms', 'tick_ms', 'rtt_ms')) {
        $percentiles = (Get-ApbProperty $metrics $percentileField).Value
        foreach ($field in @('p50', 'p95', 'p99')) {
            if (-not (Get-ApbProperty $percentiles $field).Found) {
                return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:metrics.$percentileField.$field" }
            }
            $value = (Get-ApbProperty $percentiles $field).Value
            if (-not (Test-ApbFiniteNumber $value) -or [double]$value -lt 0) {
                return [pscustomobject]@{ Valid = $false; Error = "invalid_range:metrics.$percentileField.$field" }
            }
        }
    }
    foreach ($field in @('cpu_percent', 'ram_mb', 'vram_mb', 'packet_loss_percent', 'replication_rate', 'corrections', 'missed_authoritative_steps')) {
        $value = (Get-ApbProperty $metrics $field).Value
        if (-not (Test-ApbFiniteNumber $value) -or [double]$value -lt 0) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_range:metrics.$field" }
        }
    }
    if ([double](Get-ApbProperty $metrics 'packet_loss_percent').Value -gt 100) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_range:metrics.packet_loss_percent' }
    }
    foreach ($field in @('corrections', 'missed_authoritative_steps')) {
        if (-not (Test-ApbInteger (Get-ApbProperty $metrics $field).Value)) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_integer:metrics.$field" }
        }
    }

    $evidence = (Get-ApbProperty $Telemetry 'evidence').Value
    foreach ($field in @('raw_log', 'launch_observed', 'synthetic', 'claims_64_player')) {
        if (-not (Get-ApbProperty $evidence $field).Found) {
            return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:evidence.$field" }
        }
    }
    if (-not (Test-ApbString (Get-ApbProperty $evidence 'raw_log').Value)) {
        return [pscustomobject]@{ Valid = $false; Error = 'invalid_string:evidence.raw_log' }
    }
    foreach ($field in @('launch_observed', 'synthetic', 'claims_64_player')) {
        if (-not (Test-ApbBoolean (Get-ApbProperty $evidence $field).Value)) {
            return [pscustomobject]@{ Valid = $false; Error = "invalid_boolean:evidence.$field" }
        }
    }
    $launchObserved = [bool](Get-ApbProperty $evidence 'launch_observed').Value
    $synthetic = [bool](Get-ApbProperty $evidence 'synthetic').Value
    if ((-not $launchObserved -or $synthetic) -and $marker -notin @('PERF_GATE_FAIL', 'LOAD_GATE_FAIL')) {
        return [pscustomobject]@{ Valid = $false; Error = 'honesty:non_launched_or_synthetic_capture_requires_fail_terminal' }
    }
    if ($marker -like '*_OK' -and (-not $launchObserved -or $synthetic)) {
        return [pscustomobject]@{ Valid = $false; Error = 'honesty:pass_requires_observed_non_synthetic_launch' }
    }

    $requiresLoad = $RequireLoadContract -or $marker -like 'LOAD_*'
    $loadContract = (Get-ApbProperty $Telemetry 'load_contract').Value
    if ($requiresLoad) {
        if (-not (Get-ApbProperty $Telemetry 'load_contract').Found) {
            return [pscustomobject]@{ Valid = $false; Error = 'missing_required_field:load_contract' }
        }
        foreach ($field in @('hosts_manifest', 'workload', 'identities', 'assignments', 'observed_live_clients', 'required_live_clients')) {
            if (-not (Get-ApbProperty $loadContract $field).Found) {
                return [pscustomobject]@{ Valid = $false; Error = "missing_required_field:load_contract.$field" }
            }
        }
        if (-not (Test-ApbString (Get-ApbProperty $loadContract 'hosts_manifest').Value)) {
            return [pscustomobject]@{ Valid = $false; Error = 'invalid_string:load_contract.hosts_manifest' }
        }
        foreach ($field in @('workload', 'identities', 'assignments')) {
            if ((Get-ApbProperty $loadContract $field).Value -isnot [System.Collections.IEnumerable] -or (Get-ApbProperty $loadContract $field).Value -is [string]) {
                return [pscustomobject]@{ Valid = $false; Error = "invalid_array:load_contract.$field" }
            }
        }
        if (@((Get-ApbProperty $loadContract 'workload').Value).Count -lt 3) {
            return [pscustomobject]@{ Valid = $false; Error = 'invalid_load_contract:workload_requires_movement_combat_vehicle' }
        }
        foreach ($field in @('observed_live_clients', 'required_live_clients')) {
            $value = (Get-ApbProperty $loadContract $field).Value
            if (-not (Test-ApbInteger $value) -or [int]$value -lt 0) {
                return [pscustomobject]@{ Valid = $false; Error = "invalid_range:load_contract.$field" }
            }
        }
        $identities = @((Get-ApbProperty $loadContract 'identities').Value)
        $assignments = @((Get-ApbProperty $loadContract 'assignments').Value)
        if ([int](Get-ApbProperty $loadContract 'required_live_clients').Value -ne [int]$clients) {
            return [pscustomobject]@{ Valid = $false; Error = 'invalid_load_contract:required_live_clients_must_match_clients' }
        }
        if ([int](Get-ApbProperty $loadContract 'observed_live_clients').Value -gt [int]$clients) {
            return [pscustomobject]@{ Valid = $false; Error = 'invalid_load_contract:observed_live_clients_exceeds_clients' }
        }
        if ($marker -eq 'LOAD_GATE_OK') {
            if ($identities.Count -ne [int]$clients) {
                return [pscustomobject]@{ Valid = $false; Error = 'invalid_load_contract:identity_count_must_match_clients' }
            }
            if ($assignments.Count -ne [int]$clients) {
                return [pscustomobject]@{ Valid = $false; Error = 'invalid_load_contract:assignment_count_must_match_clients' }
            }
        }
    }
    if ([bool](Get-ApbProperty $evidence 'claims_64_player').Value) {
        if ([int]$clients -ne 64 -or -not $requiresLoad -or [int](Get-ApbProperty $loadContract 'observed_live_clients').Value -ne 64) {
            return [pscustomobject]@{ Valid = $false; Error = 'honesty:claims_64_player_requires_64_observed_live_clients' }
        }
    }

    return [pscustomobject]@{ Valid = $true; Error = '' }
}

Export-ModuleMember -Function @(
    'Get-ApbBuildSha',
    'Get-ApbConfiguredMaxFps',
    'Get-ApbDistrictRecord',
    'Get-ApbHardware',
    'Get-ApbPercentiles',
    'New-ApbMetrics',
    'Test-ApbFiniteNumber',
    'Test-ApbPerfTelemetry',
    'Write-ApbJson'
)
