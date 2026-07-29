param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\VehicleItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\vehicle_item_types.json"
)

# Extract the APB VEHICLE item-type catalog from the retail VehicleItemTypes.INT (UTF-16LE mirror of the
# cooked SDD table "VehicleItemTypes"). This is the id -> player-facing DESCRIPTION dictionary the Armas
# marketplace / vehicle-select / inventory UI shows as a vehicle's flavour + role blurb (e.g. the Jericho
# 4x4, the tuned Classic Muscle variants). It is the description leg of the vehicle-info pairing:
#   - vehicles_catalog.json     (apbdb)             -> stats / handling
#   - vehicle_item_types.json   (THIS)              -> id -> rich description
#
# One key per vehicle id:
#   VehicleItemTypes_<id>_Description=<player-facing text>
# Of 580 ids ~569 carry a non-empty description; rows with an empty description are dropped, leaving the
# vehicles that actually render a blurb. Prose kept VERBATIM (embedded double-quotes survive as JSON \",
# apostrophes/&/quotes round-trip); U+21B5 -> newline; C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^VehicleItemTypes_(?<id>.+?)_Description=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $desc = Clean $m.Groups['val'].Value
    if ($desc.Length -eq 0) { continue }   # keep only vehicles that render a description
    $rows.Add([pscustomobject][ordered]@{
        id          = $m.Groups['id'].Value
        description = $desc
        order       = $ord
        source      = "VehicleItemTypes.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, ...), while leaving genuine
# control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " vehicle item types -> " + $Out)
