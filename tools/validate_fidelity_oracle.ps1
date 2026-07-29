param(
    [string]$ManifestPath = "Content/Data/fidelity/fidelity_oracle_manifest.json",
    [string]$PrecedencePath = "Content/Data/fidelity/source_precedence_manifest.json",
    [string]$LedgerPath = "tools/import_ledger.json",
    [string]$RepoRoot = (Split-Path -Parent $PSScriptRoot),
    [switch]$AllowDeferred
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$requiredRowFields = @(
    "id", "domain", "source", "status", "staged_path", "hash", "destination",
    "destination_path", "destination_hash", "expected_behavior", "comparison_type",
    "pass_threshold", "ledger_key", "ledger_status"
)
$allowedSources = @("2011", "retail", "apbdb")
$allowedStatuses = @("validated", "pending_manual", "deferred_require_binary")
$allowedComparisonTypes = @("asset_exact", "documented_fallback", "json_catalog", "manual_import", "perceptual_image_diff", "ui_anchor_tokens")
$errors = [System.Collections.Generic.List[string]]::new()
. (Join-Path $PSScriptRoot "fidelity_oracle_helpers.ps1")

try {
    $manifest = Get-Json $ManifestPath "Fidelity manifest"
    $precedence = Get-Json $PrecedencePath "Source precedence manifest"
    $ledger = Get-Json $LedgerPath "Import ledger"
}
catch {
    Write-Output "FIDELITY_ORACLE_FATAL $($_.Exception.Message)"
    exit 1
}

$precedenceByDomain = @{}
foreach ($rule in @($precedence.rules)) {
    if ([string]::IsNullOrWhiteSpace($rule.domain) -or @($rule.precedence).Count -eq 0) {
        $errors.Add("PRECEDENCE_RULE_INVALID row=manifest detail=domain_or_precedence")
        continue
    }
    $precedenceByDomain[$rule.domain] = @($rule.precedence)
}

$ledgerByKey = @{}
foreach ($entry in @($ledger.entries)) {
    $ledgerByKey[$entry.asset_key] = $entry
}

$rowIds = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
foreach ($row in @($manifest.rows)) {
    $rowId = Get-PropertyValue $row "id"
    if ([string]::IsNullOrWhiteSpace($rowId)) {
        $errors.Add("ROW_REQUIRED_FIELD_MISSING row=<unknown> detail=id")
        continue
    }

    foreach ($field in $requiredRowFields) {
        if ($null -eq $row.PSObject.Properties[$field]) {
            Add-OracleError "ROW_REQUIRED_FIELD_MISSING" $rowId $field
        }
    }
    if (-not $rowIds.Add($rowId)) {
        Add-OracleError "ROW_DUPLICATE_ID" $rowId "id"
    }

    $source = Get-PropertyValue $row "source"
    $status = Get-PropertyValue $row "status"
    $domain = Get-PropertyValue $row "domain"
    $stagedPath = Get-PropertyValue $row "staged_path"
    $stagedHash = Get-PropertyValue $row "hash"
    $destination = Get-PropertyValue $row "destination"
    $destinationPath = Get-PropertyValue $row "destination_path"
    $destinationHash = Get-PropertyValue $row "destination_hash"
    $ledgerKey = Get-PropertyValue $row "ledger_key"
    $ledgerStatus = Get-PropertyValue $row "ledger_status"
    $comparisonType = Get-PropertyValue $row "comparison_type"

    if ($allowedSources -notcontains $source) {
        Add-OracleError "ROW_SOURCE_INVALID" $rowId "source=$source"
    }
    if ($allowedStatuses -notcontains $status) {
        Add-OracleError "ROW_STATUS_INVALID" $rowId "status=$status"
    }
    if ($allowedComparisonTypes -notcontains $comparisonType) {
        Add-OracleError "ROW_COMPARISON_TYPE_INVALID" $rowId "comparison_type=$comparisonType"
    }
    if ($null -eq $precedenceByDomain[$domain]) {
        Add-OracleError "ROW_PRECEDENCE_MISSING" $rowId "domain=$domain"
    }
    elseif ($precedenceByDomain[$domain][0] -ne $source) {
        Add-OracleError "ROW_SOURCE_PRECEDENCE_MISMATCH" $rowId "expected=$($precedenceByDomain[$domain][0]) actual=$source"
    }
    if (Test-PlaceholderDestination $destination) {
        Add-OracleError "ROW_PLACEHOLDER_DESTINATION" $rowId "destination=$destination"
    }

    $isPending = $status -eq "pending_manual" -or $status -eq "deferred_require_binary"
    if ($isPending -and [string]::IsNullOrWhiteSpace((Get-PropertyValue $row "pending_reason"))) {
        Add-OracleError "ROW_PENDING_REASON_MISSING" $rowId "status=$status"
    }
    if ($status -eq "validated" -and ([string]::IsNullOrWhiteSpace($stagedPath) -or [string]::IsNullOrWhiteSpace($stagedHash) -or [string]::IsNullOrWhiteSpace($destinationPath) -or [string]::IsNullOrWhiteSpace($destinationHash))) {
        Add-OracleError "ROW_VALIDATED_ARTIFACT_MISSING" $rowId "staged_or_destination"
    }
    if (-not $AllowDeferred -and $isPending) {
        $code = if ($status -eq "pending_manual") { "ROW_PENDING" } else { "ROW_DEFERRED_REQUIRE_BINARY" }
        Add-OracleError $code $rowId $status
    }

    if ($null -ne $stagedPath -or $null -ne $stagedHash) {
        if ([string]::IsNullOrWhiteSpace($stagedPath) -or [string]::IsNullOrWhiteSpace($stagedHash)) {
            Add-OracleError "ROW_SOURCE_ARTIFACT_INCOMPLETE" $rowId "staged_path_or_hash"
        }
        elseif ($stagedHash -notmatch "^[A-Fa-f0-9]{64}$") {
            Add-OracleError "ROW_HASH_INVALID" $rowId "hash"
        }
        else {
            $actualHash = Get-FileHashValue $stagedPath $rowId "staged"
            if ($null -ne $actualHash -and $actualHash -ne $stagedHash.ToUpperInvariant()) {
                Add-OracleError "ROW_HASH_MISMATCH" $rowId "expected=$stagedHash actual=$actualHash"
            }
        }
    }

    if ($null -ne $destinationPath -or $null -ne $destinationHash) {
        if ([string]::IsNullOrWhiteSpace($destinationPath) -or [string]::IsNullOrWhiteSpace($destinationHash)) {
            Add-OracleError "ROW_DESTINATION_ARTIFACT_INCOMPLETE" $rowId "destination_path_or_hash"
        }
        elseif ($destinationHash -notmatch "^[A-Fa-f0-9]{64}$") {
            Add-OracleError "ROW_DESTINATION_HASH_INVALID" $rowId "destination_hash"
        }
        else {
            $actualDestinationHash = Get-FileHashValue $destinationPath $rowId "destination"
            if ($null -ne $actualDestinationHash -and $actualDestinationHash -ne $destinationHash.ToUpperInvariant()) {
                Add-OracleError "ROW_REFERENCE_UE_MISMATCH" $rowId "destination_hash expected=$destinationHash actual=$actualDestinationHash"
            }
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($ledgerKey)) {
        $ledgerEntry = $ledgerByKey[$ledgerKey]
        if ($ledgerStatus -eq "missing") {
            if ($null -ne $ledgerEntry) {
                Add-OracleError "ROW_LEDGER_STATUS_MISMATCH" $rowId "expected=missing actual=$($ledgerEntry.status)"
            }
        }
        elseif ($null -eq $ledgerEntry) {
            Add-OracleError "ROW_LEDGER_ENTRY_MISSING" $rowId "ledger_key=$ledgerKey"
        }
        elseif ($ledgerEntry.status -ne $ledgerStatus) {
            Add-OracleError "ROW_LEDGER_STATUS_MISMATCH" $rowId "expected=$ledgerStatus actual=$($ledgerEntry.status)"
        }
    }

    if ($null -ne $stagedPath -and (Test-Path -LiteralPath (Join-Path $RepoRoot $stagedPath) -PathType Leaf)) {
        if ($comparisonType -eq "json_catalog") {
            Test-JsonCatalog $row $rowId
        }
        elseif ($comparisonType -eq "asset_exact") {
            Test-AssetExact $row $rowId
        }
        elseif ($comparisonType -eq "ui_anchor_tokens") {
            Test-UiAnchorTokens $row $rowId
        }
        elseif ($comparisonType -eq "perceptual_image_diff") {
            Test-ScreenshotSpec $row $rowId
        }
    }
}

if ($errors.Count -gt 0) {
    foreach ($errorLine in $errors) {
        Write-Output "FIDELITY_ORACLE_FAIL $errorLine"
    }
    exit 1
}

Write-Output "FIDELITY_ORACLE_PASS rows=$(@($manifest.rows).Count) allow_deferred=$($AllowDeferred.IsPresent)"
