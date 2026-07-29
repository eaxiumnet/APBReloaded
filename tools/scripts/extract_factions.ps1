param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Factions.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\factions.json"
)

# Extract the APB faction-selection screen content (display names + faction-info lore)
# from the retail Factions.INT (UTF-16LE mirror of the cooked SDD table "Faction").
# Keys per faction id: _DisplayName, _FactionInfoDisplayName, _FactionInfoDescription.
# The lore uses U+21B5 (down-left arrow) as a paragraph-break marker; a double
# U+21B5 == a blank-line break. We convert each U+21B5 to a newline so the JSON
# preserves paragraph structure (the Domain FactionInfoCatalog unescapes \n back).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

# Strip control chars EXCEPT newline (\x0a); collapse runs of the rest to a single space.
function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x09\x0b-\x1f\u2028\u2029]+", " ")
    # collapse spaces/tabs but keep newlines; trim each paragraph
    $paras = $s -split "`n"
    for ($i=0; $i -lt $paras.Count; $i++) {
        $paras[$i] = ([regex]::Replace($paras[$i], "[ \t]+", " ")).Trim()
    }
    return ($paras -join "`n").Trim()
}

$rxDisp  = [regex]'^Factions_(?<id>[^_]+)_DisplayName=(?<val>.*)$'
$rxTitle = [regex]'^Factions_(?<id>[^_]+)_FactionInfoDisplayName=(?<val>.*)$'
$rxDesc  = [regex]'^Factions_(?<id>[^_]+)_FactionInfoDescription=(?<val>.*)$'

$disp  = @{}
$title = @{}
$desc  = @{}
$order = New-Object System.Collections.Generic.List[string]

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    # convert paragraph-break marker to newline before matching
    $m = $rxDisp.Match($ln)
    if ($m.Success) { $id=$m.Groups['id'].Value; $disp[$id] = (Clean ($m.Groups['val'].Value.Replace([char]0x21B5, "`n"))); if (-not $order.Contains($id)) { $order.Add($id) }; continue }
    $m = $rxTitle.Match($ln)
    if ($m.Success) { $id=$m.Groups['id'].Value; $title[$id] = (Clean ($m.Groups['val'].Value.Replace([char]0x21B5, "`n"))); if (-not $order.Contains($id)) { $order.Add($id) }; continue }
    $m = $rxDesc.Match($ln)
    if ($m.Success) { $id=$m.Groups['id'].Value; $desc[$id] = (Clean ($m.Groups['val'].Value.Replace([char]0x21B5, "`n"))); if (-not $order.Contains($id)) { $order.Add($id) }; continue }
}

$rows = New-Object System.Collections.Generic.List[object]
$rank = 0
foreach ($id in $order) {
    $d  = if ($disp.ContainsKey($id))  { $disp[$id] }  else { "" }
    $t  = if ($title.ContainsKey($id)) { $title[$id] } else { "" }
    $ds = if ($desc.ContainsKey($id))  { $desc[$id] }  else { "" }
    # Skip DNT ("Both") and empty-description entries.
    if ($ds.Length -eq 0) { continue }
    if ($ds -match 'DO NOT TRANSLATE' -or $t -match 'DO NOT TRANSLATE') { continue }
    $rows.Add([pscustomobject][ordered]@{
        id               = $id
        display_name     = $d
        info_title       = $t
        info_description = $ds
        rank             = $rank
        source           = "Factions.INT"
    }) | Out-Null
    $rank++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " factions -> " + $Out)
