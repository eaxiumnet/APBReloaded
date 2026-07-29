param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\DisplayPoint.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\display_points.json"
)

# Extract the retail APB DISPLAY POINT catalog from DisplayPoint.INT (UTF-16LE mirror of the cooked SDD
# table "DisplayPoint"). Display points are the progression/unlock entries surfaced in the UI (a rank,
# role, achievement or reward the player can earn) carrying a full name, a short name, a description and
# an "obtained by" blurb explaining how it is earned. Four keys per id:
#   DisplayPoint_<id>_Title=<full display name>
#   DisplayPoint_<id>_ShortTitle=<abbreviated name>
#   DisplayPoint_<id>_Description=<what it is>
#   DisplayPoint_<id>_ObtainedBy=<how it is earned>
#
# The "None" DO-NOT-TRANSLATE placeholder id is dropped. Prose kept VERBATIM; U+21B5 -> newline; other C0
# control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^DisplayPoint_(?<id>.+?)_(?<field>Title|ShortTitle|Description|ObtainedBy)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ title = ""; short_title = ""; description = ""; obtained_by = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Title'       { $data[$id].title       = $val }
        'ShortTitle'  { $data[$id].short_title  = $val }
        'Description' { $data[$id].description  = $val }
        'ObtainedBy'  { $data[$id].obtained_by  = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    if ($id -eq 'None') { continue }                 # DNT placeholder
    $d = $data[$id]
    if ($d.title.Length -eq 0) { continue }          # keep only points that render a name
    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        title       = $d.title
        short_title = $d.short_title
        description = $d.description
        obtained_by = $d.obtained_by
        order       = $ord
        source      = "DisplayPoint.INT"
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
Write-Output ("Wrote " + $rows.Count + " display points -> " + $Out)
