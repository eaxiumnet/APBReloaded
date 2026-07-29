param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Subtitles_MASC.int",
    [string]$Out    = "D:\APBReloaded\Content\Data\subtitles.json"
)

# Extract the retail APB VOICE-LINE SUBTITLE catalog from Subtitles_MASC.int (UTF-16LE mirror of the cooked SDD
# subtitle table). This is the on-screen caption text spoken by NPCs / contacts / enforcers / criminals during
# missions, greetings, taunts, radio chatter, etc.
#
# IMPORTANT — masc/fem are the SAME table: Subtitles_MASC.int and Subtitles_FEM.int are BYTE-IDENTICAL in this
# build (verified by SHA256), so the male/female split is a non-feature here. We port ONE flat id->text catalog
# from the MASC file rather than a gendered pair; if a future build ever diverges, add a `fem` text column.
#
# Grammar: a single `[Subtitles]` section, then flat `<SubtitleId>=<caption text>` lines (no `@`, no `[<n>]`).
# Ids are structured tokens like `CHA_Greeting_Known_1`, `CHA_Greeting_Known_1_ALT`. 8864 kv lines; 20 have an
# empty value (dropped by the keep-if-non-empty rule). Text is kept VERBATIM; U+21B5 -> newline and C0 control
# stripping are applied defensively.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0) { continue }
    if ($ln[0] -eq ';') { continue }
    if ([regex]::IsMatch($ln, '^\[(.+)\]\s*$')) { continue }   # section header
    $eq = $ln.IndexOf('=')
    if ($eq -lt 0) { continue }
    $id  = $ln.Substring(0, $eq).Trim()
    $val = Clean $ln.Substring($eq + 1)
    if ($id.Length -eq 0) { continue }
    if ($val.Length -eq 0) { continue }                        # keep only lines that render a caption
    $rows.Add([pscustomobject][ordered]@{
        id     = $id
        text   = $val
        order  = $ord
        source = "Subtitles_MASC.int"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, ...), while leaving genuine
# control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " voice-line subtitles -> " + $Out)
