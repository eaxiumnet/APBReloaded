param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\HUDCombatMessages.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\hud_combat_messages.json"
)

# Extract the APB on-screen combat score-feed catalog from the retail HUDCombatMessages.INT
# (UTF-16LE mirror of the cooked SDD table "HUDCombatMessage"). Each feed entry is a two-line
# floating message shown when a scoring event happens:
#   <id>_Line0  top line   (a token like <CharacterNameA> / <MedalName> / <GameplayObject>, or a
#                           label like "Arson", "Teamkill"; may be empty)
#   <id>_Line2  bottom line (the message: "Enemy Killed", "Objective Complete", "Demerit!", ...)
# Tokens (<CharacterNameA>, <MedalName>, <GameplayObject>, <Score>, ($<Score>)) are kept verbatim.
# Entries whose Line0 AND Line2 are both empty (unused Easter item placeholders) are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# Greedy id; the Line0/Line2 suffix is always the final token before '='.
$rx = [regex]'^HUDCombatMessages_(?<id>.+)_(?<suf>Line0|Line2)=(?<val>.*)$'

$line0 = @{}
$line2 = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    switch ($m.Groups['suf'].Value) {
        'Line0' { $line0[$id] = $val }
        'Line2' { $line2[$id] = $val }
    }
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $l0 = if ($line0.ContainsKey($id)) { $line0[$id] } else { "" }
    $l2 = if ($line2.ContainsKey($id)) { $line2[$id] } else { "" }
    if ($l0.Length -eq 0 -and $l2.Length -eq 0) { continue }   # both empty -> unused placeholder
    $rows.Add([pscustomobject][ordered]@{
        id     = $id
        line0  = $l0
        line2  = $l2
        order  = $ord
        source = "HUDCombatMessages.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >, $), while leaving
# genuine control-char / quote / backslash escapes intact. NOTE: many Line0 fields legitimately
# contain angle-bracket tokens like "<MedalName>"; the restore keeps those readable.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " HUD combat messages -> " + $Out)
