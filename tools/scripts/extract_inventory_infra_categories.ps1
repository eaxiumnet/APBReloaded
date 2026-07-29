param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\InventoryItemInfraCategories.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\inventory_infra_categories.json"
)

# Extract the APB inventory-item INFRASTRUCTURE-CATEGORY taxonomy from the retail
# InventoryItemInfraCategories.INT (UTF-16LE mirror of the cooked SDD table
# "InventoryItemInfraCategories"). These are the category buckets the inventory / Armas / store UI uses
# to GROUP and LABEL items (Marketplace Cash, Character, Clothing: Accessories, Clothing: Armor (Vests),
# Weapons, Mods, Vehicles, Symbols, ...). It is the categorisation layer over the master item-name
# dictionary in APBInventoryItemTypes.h (InventoryItemTypes.INT).
#
# Three keys per category id:
#   InventoryItemInfraCategories_<id>_DisplayName=<UI header, e.g. "Clothing: Accessories (Clothing)">
#   InventoryItemInfraCategories_<id>_Description=<plural/long label, e.g. "Accessories (Clothing)">
#   InventoryItemInfraCategories_<id>_SingularName=<singular label, e.g. "Accessory (Clothing)">
# The placeholder "None" id carries all-empty fields; rows with an empty DisplayName are dropped, leaving
# the categories that actually render. Prose kept VERBATIM; U+21B5 -> newline; other C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^InventoryItemInfraCategories_(?<id>.+?)_(?<field>DisplayName|Description|SingularName)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ display_name = ""; description = ""; singular_name = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'DisplayName'  { $data[$id].display_name  = $val }
        'Description'  { $data[$id].description   = $val }
        'SingularName' { $data[$id].singular_name = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.display_name.Length -eq 0) { continue }   # keep only categories that render a header
    $rows.Add([pscustomobject][ordered]@{
        id            = $id
        display_name  = $d.display_name
        description   = $d.description
        singular_name = $d.singular_name
        order         = $ord
        source        = "InventoryItemInfraCategories.INT"
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
Write-Output ("Wrote " + $rows.Count + " inventory infra categories -> " + $Out)
