param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\HUDMarkerVisualText.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\hud_marker_text.json"
)

# Extract the retail APB HUD MARKER TEXT catalog from HUDMarkerVisualText.INT (UTF-16LE mirror of the
# cooked SDD table "HUDMarkerVisualText"). Each marker id (a mission objective / spawn / item marker) carries
# up to six role-dependent label strings that the HUD shows depending on the local player's relationship to
# the marker and the mission phase:
#   HUDMarkerVisualText_<id>_OwnerAttack       (your side, attacking)
#   HUDMarkerVisualText_<id>_OwnerDefend       (your side, defending)
#   HUDMarkerVisualText_<id>_OppositionAttack  (enemy side, attacking)
#   HUDMarkerVisualText_<id>_OppositionDefend  (enemy side, defending)
#   HUDMarkerVisualText_<id>_Neutral           (no allegiance)
#   HUDMarkerVisualText_<id>_Misc              (misc / fallback)
#
# Values embed <Color:R=g G=g B=g> markup (resolved to text colour by the HUD) and are kept VERBATIM.
# U+21B5 -> newline (RTW's in-string line break); other C0 control stripped. Individual fields are often
# empty (a marker only labels the roles that apply); an id is kept if ANY of its six fields has text.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^HUDMarkerVisualText_(?<id>.+?)_(?<field>OwnerAttack|OwnerDefend|OppositionAttack|OppositionDefend|Neutral|Misc)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ owner_attack = ""; owner_defend = ""; opposition_attack = ""; opposition_defend = ""; neutral = ""; misc = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'OwnerAttack'       { $data[$id].owner_attack       = $val }
        'OwnerDefend'       { $data[$id].owner_defend       = $val }
        'OppositionAttack'  { $data[$id].opposition_attack  = $val }
        'OppositionDefend'  { $data[$id].opposition_defend  = $val }
        'Neutral'           { $data[$id].neutral            = $val }
        'Misc'              { $data[$id].misc               = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    if ($id -eq 'None') { continue }                 # DNT placeholder
    $d = $data[$id]
    $any = $d.owner_attack.Length + $d.owner_defend.Length + $d.opposition_attack.Length + $d.opposition_defend.Length + $d.neutral.Length + $d.misc.Length
    if ($any -eq 0) { continue }                     # keep only markers that render at least one label
    $rows.Add([pscustomobject][ordered]@{
        id                = $id
        owner_attack      = $d.owner_attack
        owner_defend      = $d.owner_defend
        opposition_attack = $d.opposition_attack
        opposition_defend = $d.opposition_defend
        neutral           = $d.neutral
        misc              = $d.misc
        order             = $ord
        source            = "HUDMarkerVisualText.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, ...), while leaving genuine
# control-char / quote / backslash escapes intact (keeps <Color:...> markup literal).
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " HUD marker texts -> " + $Out)
