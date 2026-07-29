param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\HUDCeremonyMsg.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\hud_ceremony_msgs.json"
)

# Extract the APB ceremony-message catalog from the retail HUDCeremonyMsg.INT (UTF-16LE mirror
# of the cooked SDD table "HUDCeremonyMsg"). Each ceremony has a single _Title field — the big
# on-screen celebration popup text (KILL STREAK!, MEDAL EARNED!, NOTORIETY LEVEL UP, etc.).
# Ids are hierarchical: AM_* (achievement-manager gameplay events), Minigame_*, ProvingGrounds_*,
# Reward_*, Trade*, Weapon_*, TimeLimitedReward_*.
# Rows with empty titles or DNT titles are dropped; <WeaponName> token kept verbatim.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^HUDCeremonyMsg_(?<id>.+?)_Title=(?<val>.*)$'

$title = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    $title[$id] = $val
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $t = $title[$id]
    # Drop empty titles and DNT-only rows.
    if ($t.Length -eq 0 -or $t -match '^DNT') { continue }
    # Category = first token (AM / Minigame / ProvingGrounds / Reward / Trade / Weapon / TimeLimitedReward).
    $cat = ($id -split '_')[0]
    $rows.Add([pscustomobject][ordered]@{
        id       = $id
        title    = $t
        category = $cat
        order    = $ord
        source   = "HUDCeremonyMsg.INT"
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
Write-Output ("Wrote " + $rows.Count + " ceremony messages -> " + $Out)
