[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string]$Alias,
    [switch]$Preflight,
    [string]$RegistryPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$registryPath = if ([string]::IsNullOrWhiteSpace($RegistryPath)) {
    Join-Path $projectRoot "tools\source_registry.json"
}
else {
    [System.IO.Path]::GetFullPath($RegistryPath)
}
if (-not (Test-Path -LiteralPath $registryPath -PathType Leaf)) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=registry_missing path=$registryPath"
    exit 1
}
$registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json -Depth 32
$rootProperty = $registry.roots.PSObject.Properties[$Alias]

if ($null -eq $rootProperty) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=unknown_alias registry=$registryPath"
    exit 1
}

$entry = $rootProperty.Value
$isCanonical = $entry.canonical -eq $true
$statusProperty = $entry.PSObject.Properties["status"]
$isQuarantinedCandidate = $null -ne $statusProperty -and $statusProperty.Value -eq "quarantined_candidate"
if ($isQuarantinedCandidate) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=quarantined_archive"
    exit 1
}
if (-not $isCanonical) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=noncanonical_root"
    exit 1
}
if ($entry.authoritative -ne $true) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=nonauthoritative_root"
    exit 1
}

$resolvedPath = [System.IO.Path]::GetFullPath([string]$entry.path)
if ($Preflight -and -not (Test-Path -LiteralPath $resolvedPath)) {
    Write-Error "SOURCE_ROOT_FAIL alias=$Alias reason=canonical_root_missing path=$resolvedPath"
    exit 1
}

Write-Output $resolvedPath
