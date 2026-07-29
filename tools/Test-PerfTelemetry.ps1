[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Path,
    [switch]$RequireLoadContract
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'PerfTelemetry.psm1') -Force

try {
    $telemetry = Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json -AsHashtable -DateKind String
    $result = Test-ApbPerfTelemetry -Telemetry $telemetry -RequireLoadContract:$RequireLoadContract
    if ($result.Valid) {
        Write-Host "PERF_SCHEMA_VALID path=$Path"
        exit 0
    }
    Write-Host "PERF_SCHEMA_INVALID error=$($result.Error) path=$Path"
    exit 1
} catch {
    Write-Host "PERF_SCHEMA_INVALID error=parse_or_validator_exception:$($_.Exception.Message) path=$Path"
    exit 1
}
