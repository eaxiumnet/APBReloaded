param(
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot)
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$validator = Join-Path $RepoRoot "tools/validate_fidelity_oracle.ps1"
$manifest = Join-Path $RepoRoot "Content/Data/fidelity/fidelity_oracle_manifest.json"
$precedence = Join-Path $RepoRoot "Content/Data/fidelity/source_precedence_manifest.json"

if (-not (Test-Path -LiteralPath $validator -PathType Leaf)) {
    throw "RED: validator not found at $validator"
}

if (-not (Test-Path -LiteralPath $manifest -PathType Leaf)) {
    throw "RED: manifest not found at $manifest"
}

if (-not (Test-Path -LiteralPath $precedence -PathType Leaf)) {
    throw "RED: source-precedence manifest not found at $precedence"
}

$tempRoot = Join-Path ([System.IO.Path]::GetTempPath()) ("apb-fidelity-oracle-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tempRoot | Out-Null

function Invoke-Oracle([string]$CandidateManifest, [bool]$AllowDeferred) {
    $args = @(
        "-NoProfile",
        "-File", $validator,
        "-ManifestPath", $CandidateManifest,
        "-PrecedencePath", $precedence,
        "-RepoRoot", $RepoRoot
    )
    if ($AllowDeferred) {
        $args += "-AllowDeferred"
    }

    $output = & pwsh @args 2>&1 | Out-String
    return [pscustomobject]@{
        ExitCode = $LASTEXITCODE
        Output = $output.Trim()
    }
}

function Assert-OracleFailure([object]$Result, [string]$Expected) {
    if ($Result.ExitCode -eq 0) {
        throw "Expected oracle failure '$Expected', but the command succeeded."
    }
    if ($Result.Output -notmatch [regex]::Escape($Expected)) {
        throw "Expected oracle output '$Expected'; actual: $($Result.Output)"
    }
    Write-Output "FIDELITY_ORACLE_TEST_PASS expected=$Expected"
}

try {
    $valid = Invoke-Oracle -CandidateManifest $manifest -AllowDeferred $true
    if ($valid.ExitCode -ne 0 -or $valid.Output -notmatch "FIDELITY_ORACLE_PASS") {
        throw "Expected deferred-aware baseline validation to pass; actual: $($valid.Output)"
    }
    Write-Output "FIDELITY_ORACLE_TEST_PASS baseline=allow_deferred"

    $strict = Invoke-Oracle -CandidateManifest $manifest -AllowDeferred $false
    Assert-OracleFailure -Result $strict -Expected "ROW_PENDING row=district.social.streamed_asset"

    $missingFieldPath = Join-Path $tempRoot "missing_hash.json"
    $missingField = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $missingField.rows[0].PSObject.Properties.Remove("hash")
    $missingField | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $missingFieldPath -Encoding utf8NoBOM
    $missingFieldResult = Invoke-Oracle -CandidateManifest $missingFieldPath -AllowDeferred $true
    Assert-OracleFailure -Result $missingFieldResult -Expected "ROW_REQUIRED_FIELD_MISSING row=menu.strings.2011 detail=hash"

    $tamperedHashPath = Join-Path $tempRoot "tampered_hash.json"
    $tamperedHash = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $tamperedHash.rows[0].hash = "0000000000000000000000000000000000000000000000000000000000000000"
    $tamperedHash | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $tamperedHashPath -Encoding utf8NoBOM
    $tamperedHashResult = Invoke-Oracle -CandidateManifest $tamperedHashPath -AllowDeferred $true
    Assert-OracleFailure -Result $tamperedHashResult -Expected "ROW_HASH_MISMATCH row=menu.strings.2011"

    $tamperedSourcePath = Join-Path $tempRoot "tampered_source.json"
    $tamperedSource = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $tamperedSource.rows[0].source = "retail"
    $tamperedSource | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $tamperedSourcePath -Encoding utf8NoBOM
    $tamperedSourceResult = Invoke-Oracle -CandidateManifest $tamperedSourcePath -AllowDeferred $true
    Assert-OracleFailure -Result $tamperedSourceResult -Expected "ROW_SOURCE_PRECEDENCE_MISMATCH row=menu.strings.2011"

    $placeholderPath = Join-Path $tempRoot "placeholder_destination.json"
    $placeholder = Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
    $placeholder.rows[0].destination = "/Engine/BasicShapes/Cube"
    $placeholder | ConvertTo-Json -Depth 32 | Set-Content -LiteralPath $placeholderPath -Encoding utf8NoBOM
    $placeholderResult = Invoke-Oracle -CandidateManifest $placeholderPath -AllowDeferred $true
    Assert-OracleFailure -Result $placeholderResult -Expected "ROW_PLACEHOLDER_DESTINATION row=menu.strings.2011"
}
finally {
    if (Test-Path -LiteralPath $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force
    }
    Write-Output "FIDELITY_ORACLE_TEST_CLEANUP removed=$tempRoot"
}
