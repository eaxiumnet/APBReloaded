param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ChatMessageCategories.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\chat_message_categories.json"
)

# Extract the APB chat-channel catalog from the retail ChatMessageCategories.INT (UTF-16LE
# mirror of the cooked SDD table "ChatMessageCategory"). Each channel has five localized fields:
#   SlashCommand            primary slash command, e.g. "/c"
#   SecondarySlashCommand   long-form alias, e.g. "/clan"
#   Tag                     display tag shown in the chat UI, e.g. "Clan"
#   Description             player-facing help text describing the channel
#   SyntaxExample           example usage, e.g. "/c message"
# Rows where Tag is DNT are dropped (None placeholder); DNT sub-fields are kept verbatim
# so the consumer can distinguish "has a slash command" from "system-only channel".

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f  ]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# SecondarySlashCommand must precede SlashCommand in the alternation so the longer suffix wins.
$rx = [regex]'^ChatMessageCategories_(?<id>.+?)_(?<suf>SecondarySlashCommand|SlashCommand|Tag|Description|SyntaxExample)=(?<val>.*)$'

$slash  = @{}
$slash2 = @{}
$tag    = @{}
$desc   = @{}
$syntax = @{}
$order  = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $val = Clean $m.Groups['val'].Value
    switch ($m.Groups['suf'].Value) {
        'SlashCommand'          { $slash[$id]  = $val }
        'SecondarySlashCommand' { $slash2[$id] = $val }
        'Tag'                   { $tag[$id]    = $val }
        'Description'           { $desc[$id]   = $val }
        'SyntaxExample'         { $syntax[$id] = $val }
    }
    if (-not $order.Contains($id)) { $order.Add($id) }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $t = if ($tag.ContainsKey($id)) { $tag[$id] } else { "" }
    # Drop rows where Tag is DNT (None placeholder); keep all others including system channels.
    if ($t.Length -eq 0 -or $t -match '^DNT') { continue }
    $rows.Add([pscustomobject][ordered]@{
        id                       = $id
        slash_command            = if ($slash.ContainsKey($id))  { $slash[$id] }  else { "" }
        secondary_slash_command  = if ($slash2.ContainsKey($id)) { $slash2[$id] } else { "" }
        tag                      = $t
        description              = if ($desc.ContainsKey($id))   { $desc[$id] }   else { "" }
        syntax_example           = if ($syntax.ContainsKey($id)) { $syntax[$id] } else { "" }
        order                    = $ord
        source                   = "ChatMessageCategories.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws under Windows PowerShell 5.1; wrap as array.
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
Write-Output ("Wrote " + $rows.Count + " chat message categories -> " + $Out)
