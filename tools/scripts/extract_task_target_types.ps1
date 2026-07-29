param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\TaskTargetTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\task_target_types.json"
)

# Extract the APB mission-target-type catalog from the retail TaskTargetTypes.INT (UTF-16LE
# mirror of the cooked SDD table "TaskTargetType"). Each target type has a single _DisplayName
# field — the label shown when the player approaches a mission objective ("Graffiti Point",
# "ATM", "Checkpoint", "Pedestrian", etc.).
# Rows with empty or DNT display names are dropped. Checkpoint/Epidemic variant ids that all
# map to "Checkpoint" are kept (the mission system resolves by id, not display name).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^TaskTargetTypes_(?<id>.+?)_DisplayName=(?<val>.*)$'

$name  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    $name[$id] = $val
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $n = $name[$id]
    # Drop empty display names and DNT-only rows.
    if ($n.Length -eq 0 -or $n -match '^DNT') { continue }
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        display_name = $n
        order        = $ord
        source       = "TaskTargetTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " task target types -> " + $Out)
