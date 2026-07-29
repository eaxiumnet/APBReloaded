param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ModifierEffects.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\modifier_effects.json"
)

# Extract the APB character / vehicle / weapon modification effect descriptions from the retail
# ModifierEffects.INT (UTF-16LE mirror of the cooked SDD table "ModifierEffect"). Each modification
# has one OR MORE description lines, keyed:
#   ModifierEffects_<id>_Description        first line
#   ModifierEffects_<id>_Description_<N>    line N (N = 2,3,4,...)
# The keys for one mod are scattered throughout the file, so we group by id and reassemble the
# lines in N order. Every line is kept VERBATIM (including the "<Color:R=.. G=.. B=..>" markup,
# which the mod tooltip renders as coloured text runs); the markup-aware colour parser lives in the
# Domain header, not here. category (Character/Vehicle/Weapon/Usable) is the id's first token.
# Trailing empty lines are trimmed; mods whose lines are ALL empty (TestMod / unused Minigame
# placeholders) are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    # Collapse control chars + the SDD manual line-break glyph U+21B5 to a space, then squeeze.
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029\u21b5]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# Non-greedy id; "_Description" (optionally "_<N>") is the suffix before '='. Trailing digits that
# are glued to the id (Explosives1, Kevlar2, Bandolier3) stay part of the id because the suffix
# always begins with the literal "_Description".
$rx = [regex]'^ModifierEffects_(?<id>.+?)_Description(?:_(?<n>\d+))?=(?<val>.*)$'

$byId  = @{}                                                   # id -> (N -> value)
$order = New-Object System.Collections.Generic.List[string]    # first-appearance order of ids

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $n   = if ($m.Groups['n'].Success) { [int]$m.Groups['n'].Value } else { 1 }
    $val = Clean $m.Groups['val'].Value
    if (-not $byId.ContainsKey($id)) {
        $byId[$id] = @{}
        $order.Add($id) | Out-Null
    }
    $byId[$id][$n] = $val
}

$rows = New-Object System.Collections.Generic.List[object]
$ord = 0
foreach ($id in $order) {
    $map  = $byId[$id]
    $maxN = ($map.Keys | Measure-Object -Maximum).Maximum
    $lines = New-Object System.Collections.Generic.List[string]
    for ($i = 1; $i -le $maxN; $i++) {
        if ($map.ContainsKey($i)) { $lines.Add([string]$map[$i]) | Out-Null }
        else { $lines.Add("") | Out-Null }
    }
    # Trim trailing empty lines.
    while ($lines.Count -gt 0 -and $lines[$lines.Count - 1].Length -eq 0) {
        $lines.RemoveAt($lines.Count - 1) | Out-Null
    }
    if ($lines.Count -eq 0) { continue }   # all lines empty -> unused placeholder / TestMod
    $category = $id.Split('_')[0]          # Character / Vehicle / Weapon / Usable
    $rows.Add([pscustomobject][ordered]@{
        id       = $id
        category = $category
        lines    = @($lines.ToArray())
        order    = $ord
        source   = "ModifierEffects.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, <, >, %, ...), while leaving
# genuine control-char / quote / backslash escapes intact. This is what keeps the "<Color:...>"
# markup readable in the JSON (and the Domain parser sees real '<'/'>').
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " modifier effects -> " + $Out)
