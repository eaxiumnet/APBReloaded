param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\StreetName.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\street_names.json"
)

# Extract the APB street-name catalog from the retail StreetName.INT (UTF-16LE mirror of the
# cooked SDD table "StreetName"). Each row is StreetName_<key>_DisplayedStreetName=<name>.
# These are the location labels shown on the minimap / world map and in mission waypoint
# callouts ("meet at Shianxi Boulevard"). Keys encode the district (Financial / Waterfront)
# and whether the label is a single named street or an intersection (keys containing "_X_").
# NOTE: one retail key has a typo ("Financia_X_..." missing the trailing 'l'); it is still a
# Financial-district intersection and is classified as such here (name verbatim from the INT).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^StreetName_(?<key>.+?)_DisplayedStreetName=(?<val>.*)$'

$rows  = New-Object System.Collections.Generic.List[object]
$order = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $key  = $m.Groups['key'].Value
    $name = Clean $m.Groups['val'].Value
    if ($name.Length -eq 0) { continue }

    $isIntersection = $key -match '_X_'
    if ($key -like 'Waterfront*')                              { $district = 'Waterfront' }
    elseif ($key -like 'Financial*' -or $key -like 'Financia_X*') { $district = 'Financial' }
    else                                                        { $district = 'Unknown' }

    $rows.Add([pscustomobject][ordered]@{
        id       = $key
        name     = $name
        district = $district
        kind     = if ($isIntersection) { 'intersection' } else { 'street' }
        order    = $order
        source   = "StreetName.INT"
    }) | Out-Null
    $order++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (accents like a-acute, &, <, >), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " street names -> " + $Out)
