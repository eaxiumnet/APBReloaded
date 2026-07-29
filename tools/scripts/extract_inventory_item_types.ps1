param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\InventoryItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\inventory_item_types.json"
)

# Extract the APB master inventory ITEM-TYPE dictionary from the retail InventoryItemTypes.INT (UTF-16LE
# mirror of the cooked SDD table "InventoryItemTypes"). This is the authoritative id -> display-name map
# for every inventory item type in APB (weapons, mods, clothing, symbols, vehicles, rewards, equipment,
# consumables, ...). It is the dictionary the inventory/armas/rewards UI uses to render item names, and
# the foundation for resolving reward "contents" ids to real item names.
#
# Two keys per item id:
#   InventoryItemTypes_<id>_DisplayName=<player-facing name>
#   InventoryItemTypes_<id>_CreatorName=<author: "Reloaded Productions" or a community creator>
# Placeholder ids (None/Vacant slots) carry an empty DisplayName; rows with an empty DisplayName are
# dropped, leaving the item types that actually render a name. CreatorName is preserved verbatim because
# APB credits community-created content (symbols/clothing/themes) to its author for 1:1 fidelity.
# Prose kept VERBATIM (apostrophes, quotes); U+21B5 -> newline; other C0 control chars stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^InventoryItemTypes_(?<id>.+?)_(?<field>DisplayName|CreatorName)=(?<val>.*)$'

$order = New-Object System.Collections.Generic.List[string]
$data  = @{}

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $f   = $m.Groups['field'].Value
    $val = Clean $m.Groups['val'].Value
    if (-not $data.ContainsKey($id)) {
        $data[$id] = [ordered]@{ display_name = ""; creator_name = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'DisplayName' { $data[$id].display_name = $val }
        'CreatorName' { $data[$id].creator_name = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.display_name.Length -eq 0) { continue }   # keep only item types that render a name
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        display_name = $d.display_name
        creator_name = $d.creator_name
        order        = $ord
        source       = "InventoryItemTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " inventory item types -> " + $Out)
