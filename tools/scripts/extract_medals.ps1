param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Medals.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\medals.json"
)

# Extract the APB medal/award catalog from the retail Medals.INT (UTF-16LE mirror of the
# cooked SDD table "Medal"). Each medal has Medals_<id>_Title + Medals_<id>_Description.
# These are the post-mission / profile achievements (kill streaks, mission-win awards,
# situational feats) and the negative "Demerit" dishonours. The <id>'s first underscore
# token is the medal category (KillStreak / BigWin / Dishonour / Situational / TimeLimit /
# KillBehind); "Dishonour" medals are demerits (shown negatively).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^Medals_(?<id>.+?)_(?<suf>Title|Description)=(?<val>.*)$'

$title = @{}
$desc  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    if ($m.Groups['suf'].Value -eq 'Title') { $title[$id] = $val } else { $desc[$id] = $val }
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $t = if ($title.ContainsKey($id)) { $title[$id] } else { "" }
    $d = if ($desc.ContainsKey($id))  { $desc[$id] }  else { "" }
    if ($t.Length -eq 0 -and $d.Length -eq 0) { continue }   # skip the empty "None" placeholder
    $cat = $id.Split('_')[0]
    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        title       = $t
        description = $d
        category    = $cat
        order       = $ord
        source      = "Medals.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " medals -> " + $Out)
