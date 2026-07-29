param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\AmmoCategories.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\ammo_categories.json"
)

# Extract the APB ammunition-category catalog from the retail AmmoCategories.INT (UTF-16LE
# mirror of the cooked SDD table "AmmoCategories"). Each category has four localized fields:
#   Name             full name shown in the inventory / mod screen
#   NameAbbreviated  short label shown next to the HUD ammo counter
#   QuantityText     ammo-counter template, e.g. "<Num> bullets" (<Num> is substituted live)
#   Description      flavour / caliber description
# These map weapons to their ammo pool and drive the HUD ammo counter text. The "None" id is
# the retail no-ammo sentinel (all fields "Not Currently Available") and is kept verbatim.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# NameAbbreviated must precede Name in the alternation so the longer suffix wins.
$rx = [regex]'^AmmoCategories_(?<id>.+?)_(?<suf>NameAbbreviated|Name|QuantityText|Description)=(?<val>.*)$'

$name  = @{}
$abbr  = @{}
$qty   = @{}
$desc  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    switch ($m.Groups['suf'].Value) {
        'Name'            { $name[$id] = $val }
        'NameAbbreviated' { $abbr[$id] = $val }
        'QuantityText'    { $qty[$id]  = $val }
        'Description'     { $desc[$id] = $val }
    }
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $n = if ($name.ContainsKey($id)) { $name[$id] } else { "" }
    if ($n.Length -eq 0) { continue }   # need at least a name to be a real row
    $rows.Add([pscustomobject][ordered]@{
        id               = $id
        name             = $n
        name_abbreviated = if ($abbr.ContainsKey($id)) { $abbr[$id] } else { "" }
        quantity_text    = if ($qty.ContainsKey($id))  { $qty[$id] }  else { "" }
        description      = if ($desc.ContainsKey($id)) { $desc[$id] } else { "" }
        order            = $ord
        source           = "AmmoCategories.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >), while leaving
# genuine control-char / quote / backslash escapes intact. NOTE: QuantityText legitimately
# contains "<Num>" angle-bracket tokens; the restore keeps those readable.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " ammo categories -> " + $Out)
