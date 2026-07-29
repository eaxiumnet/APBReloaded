param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\EmoteCommands.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\emote_commands.json"
)

# Extract the APB emote-command catalog from the retail EmoteCommands.INT (UTF-16LE mirror of
# the cooked SDD table "EmoteCommand"). Each emote has two localized fields:
#   SlashCommand   the slash command that triggers the emote, e.g. "/dance"
#   DisplayName    the human-readable name shown in the emote wheel UI, e.g. "Dance"
# This is the authoritative list the emote wheel / chat autocomplete / animation binder reads.
# Ids may contain spaces (e.g. "Body Pop", "Dance 80s"); the regex handles this via non-greedy
# match up to the known suffix.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^EmoteCommands_(?<id>.+?)_(?<suf>SlashCommand|DisplayName)=(?<val>.*)$'

$slash = @{}
$name  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    switch ($m.Groups['suf'].Value) {
        'SlashCommand' { $slash[$id] = $val }
        'DisplayName'  { $name[$id]  = $val }
    }
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $n = if ($name.ContainsKey($id)) { $name[$id] } else { "" }
    if ($n.Length -eq 0) { continue }   # need at least a display name to be a real emote
    $rows.Add([pscustomobject][ordered]@{
        id            = $id
        slash_command = if ($slash.ContainsKey($id)) { $slash[$id] } else { "" }
        display_name  = $n
        order         = $ord
        source        = "EmoteCommands.INT"
    }) | Out-Null
    $ord++
}

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
Write-Output ("Wrote " + $rows.Count + " emote commands -> " + $Out)
