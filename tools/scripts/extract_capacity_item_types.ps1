param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\CapacityItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\capacity_item_types.json"
)

# Extract the APB capacity-item-type catalog from the retail CapacityItemTypes.INT (UTF-16LE
# mirror of the cooked SDD table "CapacityItemType"). Each capacity item has a single
# _Description field describing the capacity expansion it grants (e.g. "Increases Clothing
# Capacity by 1.", "Increases Vehicle Capacity to Maximum.").
# Ids are hierarchical: "Capacity_<Category>_<Amount>[_Alt]" (e.g. Capacity_Clothing_1,
# Capacity_Songs_Max, Capacity_Weapon_5_Alt).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^CapacityItemTypes_(?<id>.+?)_Description=(?<val>.*)$'

$desc  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    $desc[$id] = $val
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $d = $desc[$id]
    if ($d.Length -eq 0) { continue }

    # Parse category + amount from id: "Capacity_<Category>_<Amount>[_Alt]".
    $category = ""
    $amount   = ""
    $isAlt    = $false
    $isMax    = $false
    if ($id -match '^Capacity_([^_]+)_([^_]+)(?:_(Alt))?$') {
        $category = $matches[1]
        $amount   = $matches[2]
        $isAlt    = ($matches.Count -gt 3 -and $matches[3] -eq 'Alt')
        $isMax    = ($amount -eq 'Max')
    }

    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        description = $d
        category    = $category
        amount      = $amount
        is_max      = $isMax
        is_alt      = $isAlt
        order       = $ord
        source      = "CapacityItemTypes.INT"
    }) | Out-Null
    $ord++
}

$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " capacity item types -> " + $Out)
