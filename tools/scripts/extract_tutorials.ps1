param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Tutorials.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\tutorials.json"
)

# Extract the retail APB in-game TUTORIAL / "City Guide" catalog from Tutorials.INT (UTF-16LE mirror of the
# cooked SDD table "Tutorials"). This is the new-player onboarding help system shown in the tutorial book /
# City Guide UI. Three keys per tutorial id:
#   Tutorials_<id>_Title=<short heading, e.g. "Welcome to San Paro">
#   Tutorials_<id>_SubTitle=<one-line tagline>
#   Tutorials_<id>_Body=<multi-paragraph HTML body; contains <br>, <b>, ... markup>
#
# The Body carries the game's own lightweight HTML markup (<br>, <b>, <img>, ...) which is kept VERBATIM so
# the UE5 UI can render it exactly like retail (1:1 fidelity). U+21B5 -> newline; other C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^Tutorials_(?<id>.+?)_(?<field>Title|SubTitle|Body)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ title = ""; subtitle = ""; body = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Title'    { $data[$id].title    = $val }
        'SubTitle' { $data[$id].subtitle = $val }
        'Body'     { $data[$id].body     = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    if ($id -eq 'None') { continue }                 # DNT placeholder
    $d = $data[$id]
    if ($d.title.Length -eq 0) { continue }          # keep only tutorials that render a heading
    $rows.Add([pscustomobject][ordered]@{
        id       = $id
        title    = $d.title
        subtitle = $d.subtitle
        body     = $d.body
        order    = $ord
        source   = "Tutorials.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (< > & apostrophes ...), while leaving genuine
# control-char / quote / backslash escapes intact. This keeps the HTML markup literal.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " tutorials -> " + $Out)
