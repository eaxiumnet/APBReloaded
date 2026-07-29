param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\WeaponItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\weapon_item_types.json"
)

# Extract the APB WEAPON item-type catalog from the retail WeaponItemTypes.INT (UTF-16LE mirror of the
# cooked SDD table "WeaponItemTypes"). This is the id -> player-facing DESCRIPTION dictionary the Armas
# marketplace / weapon-select / inventory UI shows as a weapon's flavour + role blurb (e.g. the HVR-243
# sniper, the STAR assault rifle). It is the missing description leg of the weapon-info triple:
#   - weapons_catalog.json      (apbdb) -> stats / ballistics
#   - weapon_display_names.json (InventoryItemTypes) -> id -> display name
#   - weapon_item_types.json    (THIS) -> id -> rich description
#
# One key per weapon id:
#   WeaponItemTypes_<id>_Description=<player-facing text>
# Of 936 ids ~839 carry a non-empty description; rows with an empty description are dropped, leaving the
# weapons that actually render a blurb. Prose kept VERBATIM (embedded double-quotes survive as JSON \",
# apostrophes/&/quotes round-trip); U+21B5 -> newline; C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^WeaponItemTypes_(?<id>.+?)_Description=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $desc = Clean $m.Groups['val'].Value
    if ($desc.Length -eq 0) { continue }   # keep only weapons that render a description
    $rows.Add([pscustomobject][ordered]@{
        id          = $m.Groups['id'].Value
        description = $desc
        order       = $ord
        source      = "WeaponItemTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " weapon item types -> " + $Out)
