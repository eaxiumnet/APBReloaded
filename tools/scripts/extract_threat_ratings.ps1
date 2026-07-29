<#
    extract_threat_ratings.ps1

    Extracts the retail APB matchmaking THREAT RATING tiers from ThreatLevels.INT
    (UTF-16LE mirror of the cooked SDD table ThreatLevel) into a flat JSON array
    consumable by the pure-C++ Domain (apb::ThreatRatingCatalog).

    This is the matchmaking skill bracket shown next to a player's name
    (In Training / Green / Bronze / Silver / Gold) plus the AllowedDistrictThreats
    rule that gates which district-instance threat brackets that rating may join.
    It is DISTINCT from notoriety/prestige "heat" (see Content/Data/threat_table.json).

    Line format in the INT:
        ThreatLevels_<id>_DisplayedName=<display name>
        ThreatLevels_<id>_AllowedDistrictThreats=<comma / "or" separated list>

    Output row: { id, displayed_name, allowed_district_threats, rank, source }
    Rows are emitted in file order (rank = 0-based encounter index). Entries with an
    empty DisplayedName are skipped.
#>
param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ThreatLevels.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\threat_ratings.json"
)

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

$rxName    = [regex]'^ThreatLevels_(?<id>.+)_DisplayedName=(?<val>.*)$'
$rxAllowed = [regex]'^ThreatLevels_(?<id>.+)_AllowedDistrictThreats=(?<val>.*)$'

$order   = New-Object System.Collections.Generic.List[string]
$name    = @{}
$allowed = @{}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
    $m = $rxName.Match($line)
    if ($m.Success) {
        $id = $m.Groups['id'].Value
        if (-not $order.Contains($id)) { $order.Add($id) }
        $name[$id] = Clean($m.Groups['val'].Value)
        continue
    }
    $m = $rxAllowed.Match($line)
    if ($m.Success) {
        $id = $m.Groups['id'].Value
        if (-not $order.Contains($id)) { $order.Add($id) }
        $allowed[$id] = Clean($m.Groups['val'].Value)
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$rank = 0
foreach ($id in $order) {
    $dn = if ($name.ContainsKey($id)) { $name[$id] } else { "" }
    if ($dn -eq "") { continue }   # skip entries with no display name (e.g. None)
    $ad = if ($allowed.ContainsKey($id)) { $allowed[$id] } else { "" }
    $rows.Add([pscustomobject][ordered]@{
        id                       = $id
        displayed_name           = $dn
        allowed_district_threats = $ad
        rank                     = $rank
        source                   = "APBGame/Localization/INT/ThreatLevels.INT (retail; mirror of SDD table ThreatLevel)"
    })
    $rank++
}

$arr = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6
# Decode PowerShell's \uXXXX escaping of apostrophes/&/<>, keeping the JSON-structural
# escapes (0x22 quote, 0x5c backslash, and true control chars < 0x20) intact so the naive
# Domain JSON parser never sees a mangled "u0027"/"u0026".
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Host ("Wrote " + $rows.Count + " threat ratings -> " + $Out)
