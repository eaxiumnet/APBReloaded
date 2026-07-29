function Add-OracleError([string]$Code, [string]$RowId, [string]$Detail) {
    $errors.Add("$Code row=$RowId detail=$Detail")
}

function Resolve-InputPath([string]$Path) {
    if ([System.IO.Path]::IsPathRooted($Path)) {
        return $Path
    }
    return Join-Path $RepoRoot $Path
}

function Get-Json([string]$RelativePath, [string]$Label) {
    $fullPath = Resolve-InputPath $RelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        throw "$Label missing: $fullPath"
    }
    try {
        return Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json -Depth 100
    }
    catch {
        throw "$Label parse failed: $fullPath :: $($_.Exception.Message)"
    }
}

function Get-PropertyValue([object]$Object, [string]$Name) {
    $property = $Object.PSObject.Properties[$Name]
    if ($null -eq $property) {
        return $null
    }
    return $property.Value
}

function Test-RelativePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) {
        return $false
    }
    if ([System.IO.Path]::IsPathRooted($Path) -or $Path -match "(^|[\\/])\.\.([\\/]|$)") {
        return $false
    }
    return $true
}

function Get-FileHashValue([string]$RelativePath, [string]$RowId, [string]$Side) {
    if (-not (Test-RelativePath $RelativePath)) {
        Add-OracleError "ROW_PATH_INVALID" $RowId "$Side=$RelativePath"
        return $null
    }

    $fullPath = Join-Path $RepoRoot $RelativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        Add-OracleError "ROW_STAGED_FILE_MISSING" $RowId "$Side=$RelativePath"
        return $null
    }
    return (Get-FileHash -LiteralPath $fullPath -Algorithm SHA256).Hash.ToUpperInvariant()
}

function Test-PlaceholderDestination([string]$Destination) {
    $normalized = $Destination.Replace("\\", "/").ToLowerInvariant()
    return $normalized.Contains("/engine/basicshapes/cube") -or
        $normalized.Contains("/engine/basicshapes/") -or
        $normalized.Contains("placeholder")
}

function Get-RecordCount([object]$Json, [string]$RecordPath) {
    if (-not [string]::IsNullOrWhiteSpace($RecordPath)) {
        $records = Get-PropertyValue $Json $RecordPath
        if ($null -eq $records) {
            return 0
        }
        if ($records -is [System.Array]) {
            return $records.Count
        }
        return @($records.PSObject.Properties).Count
    }
    if ($Json -is [System.Array]) {
        return $Json.Count
    }
    return @($Json.PSObject.Properties).Count
}

function Test-JsonCatalog([object]$Row, [string]$RowId) {
    $threshold = Get-PropertyValue $Row "pass_threshold"
    $path = Get-PropertyValue $Row "staged_path"
    $fullPath = Join-Path $RepoRoot $path
    try {
        $json = Get-Content -LiteralPath $fullPath -Raw | ConvertFrom-Json -Depth 100
    }
    catch {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "json_parse=$path"
        return
    }

    $minimumBytes = Get-PropertyValue $threshold "minimum_bytes"
    if ($minimumBytes -isnot [int] -and $minimumBytes -isnot [long]) {
        Add-OracleError "ROW_THRESHOLD_INVALID" $RowId "minimum_bytes"
    }
    elseif ((Get-Item -LiteralPath $fullPath).Length -lt $minimumBytes) {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "minimum_bytes=$minimumBytes"
    }

    $minimumRecords = Get-PropertyValue $threshold "minimum_records"
    if ($minimumRecords -isnot [int] -and $minimumRecords -isnot [long]) {
        Add-OracleError "ROW_THRESHOLD_INVALID" $RowId "minimum_records"
    }
    elseif ((Get-RecordCount $json (Get-PropertyValue $threshold "record_path")) -lt $minimumRecords) {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "minimum_records=$minimumRecords"
    }
}

function Test-AssetExact([object]$Row, [string]$RowId) {
    $threshold = Get-PropertyValue $Row "pass_threshold"
    $destinationPath = Get-PropertyValue $Row "destination_path"
    $fullPath = Join-Path $RepoRoot $destinationPath
    $minimumBytes = Get-PropertyValue $threshold "minimum_bytes"
    $extension = Get-PropertyValue $threshold "required_extension"

    if ($minimumBytes -isnot [int] -and $minimumBytes -isnot [long]) {
        Add-OracleError "ROW_THRESHOLD_INVALID" $RowId "minimum_bytes"
    }
    elseif ((Get-Item -LiteralPath $fullPath).Length -lt $minimumBytes) {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "minimum_bytes=$minimumBytes"
    }

    if ([string]::IsNullOrWhiteSpace($extension)) {
        Add-OracleError "ROW_THRESHOLD_INVALID" $RowId "required_extension"
    }
    elseif (-not $destinationPath.EndsWith($extension, [System.StringComparison]::OrdinalIgnoreCase)) {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "required_extension=$extension"
    }
}

function Test-UiAnchorTokens([object]$Row, [string]$RowId) {
    $referencePath = Join-Path $RepoRoot (Get-PropertyValue $Row "staged_path")
    $implementationPath = Get-PropertyValue $Row "implementation_path"
    $implementationHash = Get-FileHashValue $implementationPath $RowId "implementation"
    if ($null -eq $implementationHash) {
        return
    }

    try {
        $reference = Get-Content -LiteralPath $referencePath -Raw | ConvertFrom-Json -Depth 100
        $implementation = Get-Content -LiteralPath (Join-Path $RepoRoot $implementationPath) -Raw
    }
    catch {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "ui_anchor_input"
        return
    }

    $tokens = @($reference.login.required_tokens) + @($reference.character_select.required_tokens)
    foreach ($token in $tokens) {
        if (-not $implementation.Contains($token)) {
            Add-OracleError "ROW_REFERENCE_UE_MISMATCH" $RowId "missing_anchor=$token"
        }
    }

    $requiredTokens = Get-PropertyValue (Get-PropertyValue $Row "pass_threshold") "required_tokens"
    if ($requiredTokens -ne $tokens.Count) {
        Add-OracleError "ROW_THRESHOLD_INVALID" $RowId "required_tokens=$requiredTokens actual=$($tokens.Count)"
    }
}

function Test-ScreenshotSpec([object]$Row, [string]$RowId) {
    $specPath = Get-PropertyValue $Row "capture_spec_path"
    $specHash = Get-FileHashValue $specPath $RowId "capture_spec"
    if ($null -eq $specHash) {
        return
    }

    try {
        $spec = Get-Content -LiteralPath (Join-Path $RepoRoot $specPath) -Raw | ConvertFrom-Json -Depth 100
    }
    catch {
        Add-OracleError "ROW_COMPARISON_FAILED" $RowId "capture_spec_parse"
        return
    }

    $threshold = Get-PropertyValue $Row "pass_threshold"
    $valid = $spec.status -eq "deferred_require_binary" -and
        $spec.reference_manifest_row -eq "menu.art.login_scene" -and
        $spec.capture_manifest_row -eq $RowId -and
        $spec.stage -eq "Login" -and
        $spec.resolution.width -eq 1600 -and
        $spec.resolution.height -eq 900 -and
        $spec.camera.location.Count -eq 3 -and
        $spec.camera.rotation.Count -eq 3 -and
        $spec.camera.field_of_view -eq 90 -and
        $spec.comparison -eq "perceptual_image_diff" -and
        $spec.pass_threshold.sample_stride -eq $threshold.sample_stride -and
        $spec.pass_threshold.pixel_tolerance -eq $threshold.pixel_tolerance -and
        $spec.pass_threshold.max_mean_absolute_error -eq $threshold.max_mean_absolute_error -and
        $spec.pass_threshold.max_changed_fraction -eq $threshold.max_changed_fraction

    if (-not $valid) {
        Add-OracleError "ROW_REFERENCE_UE_MISMATCH" $RowId "capture_spec_contract"
    }
}
