param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Contacts.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\contacts_catalog.json"
)

# Extract the AUTHORITATIVE retail APB CONTACT catalog from Contacts.INT (UTF-16LE mirror of the cooked
# SDD table "Contacts"). Contacts are the mission-giver NPCs that drive all of APB progression (Double-B,
# Veronika Lee, Britney Bloodrose, ...). Each contact carries a display NAME (Title) and a rich lore BIO
# (Description).
#
# Two keys per contact id:
#   Contacts_<id>_Title=<display name, e.g. "Double-B">
#   Contacts_<id>_Description=<multi-paragraph lore bio>
#
# NOTE ON PROVENANCE: this is the AUTHORITATIVE, UNTRUNCATED source. The existing
# Content/Data/contacts_lore.json is apbdb-scraped and its descriptions are truncated (~500 chars, cut
# mid-sentence). This catalog carries the complete retail bio text. The "None" placeholder id (a
# DO-NOT-TRANSLATE stub) is dropped. Prose kept VERBATIM; U+21B5 -> newline; other C0 control stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^Contacts_(?<id>.+?)_(?<field>Title|Description)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ title = ""; description = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Title'       { $data[$id].title       = $val }
        'Description' { $data[$id].description = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    if ($id -eq 'None') { continue }                 # DNT placeholder
    $d = $data[$id]
    if ($d.title.Length -eq 0) { continue }          # keep only contacts that render a name
    $rows.Add([pscustomobject][ordered]@{
        id          = $id
        title       = $d.title
        description = $d.description
        order       = $ord
        source      = "Contacts.INT"
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
Write-Output ("Wrote " + $rows.Count + " contacts -> " + $Out)
