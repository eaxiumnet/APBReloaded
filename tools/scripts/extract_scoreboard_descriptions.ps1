param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ScoreboardDescriptions.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\scoreboard_descriptions.json"
)

# Extract the APB post-mission / end-of-round scoreboard column catalog from the retail
# ScoreboardDescriptions.INT (UTF-16LE mirror of the cooked SDD table "ScoreboardDescription").
# Every entry is a single field:
#   ScoreboardDescriptions_Column_<id>_DisplayText=<tooltip text>
# where <id> is the scoreboard column key (Arrests, Kills, Deaths, Score, Threat, ...). The text
# is the tooltip / hover description shown for that column on the mission + chaos scoreboards.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^ScoreboardDescriptions_Column_(?<id>.+?)_DisplayText=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$seen = @{}
$ord = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    if ($val.Length -eq 0) { continue }          # need a description to be a real row
    if ($seen.ContainsKey($id)) { continue }      # first occurrence wins
    $seen[$id] = $true
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        display_text = $val
        order        = $ord
        source       = "ScoreboardDescriptions.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >), while leaving
# genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " scoreboard descriptions -> " + $Out)
