param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\HUDMessages.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\hud_messages.json"
)

# Extract the APB HUD-message catalog from the retail HUDMessages.INT (UTF-16LE mirror of the cooked
# SDD table "HUDMessage"). These are the broad on-screen HUD notifications / prompts / error banners
# (mission events, deliveries, "You cannot abandon opposed missions.", arrest/bounty prompts, ...) —
# distinct from the combat score-feed in HUDCombatMessages.INT.
#
# Two keys per message id:
#   HUDMessages_<id>_DisplayText=<on-screen banner text>
#   HUDMessages_<id>_ChatText=<system-chat-log version>   (usually empty)
# Values carry semantic markup that must be preserved VERBATIM:
#   * <col:NAME>...</col>   named colour spans (NAME resolves against the HUD colour palette)
#   * <CharacterNameA> / <VehicleName> / <Amount> / ...  runtime substitution tokens
# The manual line-break glyph U+21B5 is converted to a real newline (the banner renders multi-line);
# other control chars are stripped. Rows where both fields are empty (e.g. HUDMessages_None) dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

$SEP = [char]0x21B5

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    # U+21B5 -> newline (forced HUD line break), strip CR, drop other C0 control chars but KEEP
    # newline/tab; do NOT touch angle-bracket markup or substitution tokens.
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^HUDMessages_(?<id>.+?)_(?<field>DisplayText|ChatText)=(?<val>.*)$'

# Group the two fields per id, preserving first-seen order.
$order = New-Object System.Collections.Generic.List[string]
$data  = @{}

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $f   = $m.Groups['field'].Value
    $val = Clean $m.Groups['val'].Value
    if (-not $data.ContainsKey($id)) {
        $data[$id] = [ordered]@{ display_text = ""; chat_text = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'DisplayText' { $data[$id].display_text = $val }
        'ChatText'    { $data[$id].chat_text    = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.display_text.Length -eq 0 -and $d.chat_text.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        display_text = $d.display_text
        chat_text    = $d.chat_text
        order        = $ord
        source       = "HUDMessages.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (angle brackets, apostrophes, &, ...), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " HUD messages -> " + $Out)
