param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\PopupDialogs.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\popup_dialogs.json"
)

# Extract the retail APB POPUP DIALOG catalog from PopupDialogs.INT (UTF-16LE mirror of the cooked SDD
# table "PopupDialogs"). These are the in-game advisory / help popups shown to the player during play
# (ammo-low advice, arrest rules, vehicle controls, group/mission notices, ...). One key per id:
#   PopupDialogs_<id>_PopupBody=<popup text>
#
# The id carries a category prefix (first '_'-token): AD (Advice/tutorial dialogs), etc. Some bodies embed
# UI markup tokens such as <Key:Handbrake> that are resolved to the player's bound key at runtime; these are
# kept VERBATIM for 1:1 rendering. U+21B5 -> newline; other C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^PopupDialogs_(?<id>.+?)_PopupBody=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    if ($id -eq 'None') { continue }                 # DNT placeholder (empty body)
    $val = Clean $m.Groups['val'].Value
    if ($val.Length -eq 0) { continue }              # keep only popups that render text
    $rows.Add([pscustomobject][ordered]@{
        id     = $id
        body   = $val
        order  = $ord
        source = "PopupDialogs.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, ...), while leaving genuine
# control-char / quote / backslash escapes intact (keeps <Key:...> markup literal).
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " popup dialogs -> " + $Out)
