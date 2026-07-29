param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\EquipmentTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\equipment_types.json"
)

# Extract the APB equipment-type catalog from the retail EquipmentTypes.INT (UTF-16LE mirror of
# the cooked SDD table "EquipmentType"). Each equipment type has a single _Description field.
# Equipment ids are hierarchical: "Equipment_<Base>[_Mk<2-4>]" (e.g. Equipment_BatteringRam,
# Equipment_BatteringRam_Mk2). The base name + mk tier are parsed from the id.
# Rows with DNT descriptions are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^EquipmentTypes_(?<id>.+?)_Description=(?<val>.*)$'

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
    # Drop DNT-only rows (empty desc kept as None row, but DNT rows skipped).
    if ($d -match '^DNT') { continue }

    # Parse base + mk tier from id: "Equipment_BatteringRam" or "Equipment_BatteringRam_Mk2".
    $base = $id
    $mk   = 0
    if ($id -match '_Mk(\d+)$') { $mk = [int]$matches[1]; $base = $id -replace '_Mk\d+$','' }

    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        description = $d
        base        = $base
        mk          = $mk
        order       = $ord
        source      = "EquipmentTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " equipment types -> " + $Out)
