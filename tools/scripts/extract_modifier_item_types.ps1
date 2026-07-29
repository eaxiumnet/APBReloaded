param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ModifierItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\modifier_item_types.json"
)

# Extract the APB modification ITEM catalog from the retail ModifierItemTypes.INT (UTF-16LE mirror
# of the cooked SDD table "ModifierItemType"). These are the purchasable / equippable mod items
# (what you buy on Armas and slot on a character / vehicle / weapon); each one carries a short
# "type label" and a flavour description, and its id maps to the stat effect in ModifierEffects.INT
# (see modifier_effects.json) by stripping the "FnMod_"/"FNMod_" prefix and any trailing "_Tutorial".
#
# Each item is a single key:
#   ModifierItemTypes_<id>_Description=<TypeLabel> -<U+21B5><flavour description>
# The value is a two-part string separated by the SDD manual line-break glyph U+21B5: the part
# before it is the type label (e.g. "Health Modification", usually ending in " -"), the part after
# is the flavour description. Some entries have no U+21B5 (flavour only, no label) and a few have
# extra U+21B5 (multi-line flavour, collapsed to spaces). Empty-value rows are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

$SEP = [char]0x21B5

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    # Collapse control chars + the manual line-break glyph U+21B5 to a space, then squeeze.
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029\u21b5]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# Derive the category from the id: strip a leading FnMod_/FNMod_/Mod_ (case-insensitive), then take
# the first token. The empty-slot placeholders (Mod_None/Mod_Vacant/None) are "Special".
function CategoryOf([string]$id) {
    if ($id -eq 'None' -or $id -match '^Mod_(None|Vacant)$') { return 'Special' }
    $core = [regex]::Replace($id, '^(FnMod_|FNMod_|Mod_)', '')
    return $core.Split('_')[0]
}

$rx = [regex]'^ModifierItemTypes_(?<id>.+?)_Description=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $raw = $m.Groups['val'].Value

    $label = ""
    $desc  = ""
    $idx = $raw.IndexOf($SEP)
    if ($idx -ge 0) {
        # Type label is everything before the first U+21B5, minus a trailing " -".
        $label = $raw.Substring(0, $idx)
        $label = [regex]::Replace($label, "[\x00-\x1f\u2028\u2029\u21b5]+", " ").Trim()
        $label = $label.TrimEnd(' ', '-').Trim()
        $desc  = Clean $raw.Substring($idx + 1)
    } else {
        # No separator: whole value is flavour (no type label).
        $desc = Clean $raw
    }

    if ($label.Length -eq 0 -and $desc.Length -eq 0) { continue }  # empty row -> dropped

    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        category    = CategoryOf $id
        type_label  = $label
        description = $desc
        order       = $ord
        source      = "ModifierItemTypes.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >, ...), while leaving
# genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " modifier item types -> " + $Out)
