param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\UnlockItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\unlock_item_types.json"
)

# Extract the APB UNLOCK item-type catalog from the retail UnlockItemTypes.INT (UTF-16LE mirror of the
# cooked SDD table "UnlockItemTypes"). An "unlock item" is a token/entitlement granted through
# progression, Armas, Joker Store or events that enables something for the character: emotes,
# inventory-capacity increases (clothing/outfit/symbol/song slots), daily-activity unlocks, chat/marker
# features, etc. This is the id -> player-facing DESCRIPTION dictionary the inventory / store / progression
# UI uses to explain what an unlock grants, and a companion to the master item-name dictionary in
# APBInventoryItemTypes.h (InventoryItemTypes.INT).
#
# One key per unlock id:
#   UnlockItemTypes_<id>_Description=<player-facing text, e.g. Unlocks the Angry Emote - "/angry".>
# Of 8655 ids only ~1972 carry a non-empty description; rows with an empty description are dropped,
# leaving the unlocks that actually render text. A small tail (~83 rows) are internal "DNT - ..."
# (Do-Not-Translate) developer notes kept verbatim for a faithful 1:1 mirror of the retail table.
# Prose kept VERBATIM (embedded double-quotes survive as JSON \"); U+21B5 -> newline; C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^UnlockItemTypes_(?<id>.+?)_Description=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $desc = Clean $m.Groups['val'].Value
    if ($desc.Length -eq 0) { continue }   # keep only unlocks that render a description
    $rows.Add([pscustomobject][ordered]@{
        id          = $m.Groups['id'].Value
        description = $desc
        order       = $ord
        source      = "UnlockItemTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " unlock item types -> " + $Out)
