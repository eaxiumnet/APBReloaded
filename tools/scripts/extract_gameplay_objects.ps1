param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\GameplayObjects.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\gameplay_objects.json"
)

# Extract the APB gameplay-object label catalog from the retail GameplayObjects.INT (UTF-16LE
# mirror of the cooked SDD table "GameplayObject"). Each object has a single _Description field
# — the context-sensitive interaction label shown in the HUD ("Bench", "Electrical Box",
# "Pedestrian", "Criminal", "Armoured Van", etc.).
# Ids are hierarchical: <Category>_<Name> (e.g. Prop_Bench, Vehicle_Ambient_Taxi,
# PlayerCharacter_Criminal). Category = first id token.
# Rows with empty or DNT descriptions are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^GameplayObjects_(?<id>.+?)_Description=(?<val>.*)$'

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
    # Drop empty and DNT-only rows.
    if ($d.Length -eq 0 -or $d -match '^DNT') { continue }
    # Category = first underscore-separated token.
    $cat = ($id -split '_')[0]
    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        description = $d
        category    = $cat
        order       = $ord
        source      = "GameplayObjects.INT"
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
Write-Output ("Wrote " + $rows.Count + " gameplay objects -> " + $Out)
