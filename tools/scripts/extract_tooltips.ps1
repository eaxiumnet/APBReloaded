param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Tooltips.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\tooltips.json"
)

# Extract the retail APB UI TOOLTIP catalog from Tooltips.INT (UTF-16LE mirror of the cooked SDD tooltip
# table). Unlike the other localization tables (which use a flat `<Table>_<id>_<Suffix>=<value>` grammar),
# Tooltips.INT is SECTION-SCOPED: each `[<Scene>]` section groups the hover tooltips for one UI scene, and
# within it every key is `<Scene>@<Widget>=<tooltip text>` (the key repeats the scene name before '@', and it
# always equals the section header). Example:
#   [Login_Scene]
#   Login_Scene@UILabelButton_TOS=Create a new APB Account.
# 54 scenes, 412 keys total. Values are short plain-text hover hints (no <col:> markup, no embedded newlines in
# this build); kept VERBATIM. U+21B5 -> newline and C0 control stripping are applied defensively. A row is kept
# only if its tooltip text is non-empty (drops the 3 empty placeholder widgets).
#
# FLATTENED to one row per (scene, widget) so the flat JSON-catalog helpers apply unchanged; the Domain catalog
# (APBTooltips.h) re-groups by scene.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rows = New-Object System.Collections.Generic.List[object]
$section = $null
$ord = 0

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0) { continue }
    if ($ln[0] -eq ';') { continue }
    $mSec = [regex]::Match($ln, '^\[(.+)\]\s*$')
    if ($mSec.Success) { $section = $mSec.Groups[1].Value; continue }
    $mKv = [regex]::Match($ln, '^(?<key>[^=]+)=(?<val>.*)$')
    if (-not $mKv.Success) { continue }
    $key = $mKv.Groups['key'].Value
    $at  = $key.IndexOf('@')
    if ($at -lt 0) { continue }                       # not a Scene@Widget key
    # Scene from the section header (authoritative); widget is the part after '@'.
    $scene  = if ($section) { $section } else { $key.Substring(0, $at) }
    $widget = $key.Substring($at + 1)
    $val    = Clean $mKv.Groups['val'].Value
    if ($val.Length -eq 0) { continue }               # keep only widgets that render a tooltip
    $rows.Add([pscustomobject][ordered]@{
        scene  = $scene
        widget = $widget
        text   = $val
        order  = $ord
        source = "Tooltips.INT"
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
Write-Output ("Wrote " + $rows.Count + " UI tooltips -> " + $Out)
