param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\DailyActivityContacts.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\daily_activity_contacts.json"
)

# Extract the retail APB DAILY-ACTIVITY CONTACT catalog from DailyActivityContacts.INT (UTF-16LE mirror of
# the cooked SDD table "DailyActivityContacts"). A daily activity is the small "do X today" objective a
# player receives from a contact each day (e.g. "Blow up 3 enemy vehicles"). Each activity id carries three
# text fields, and MANY activities ship several randomised flavour VARIANTS of that text (the game rotates
# them so the same objective reads differently day to day / faction to faction):
#   DailyActivityContacts_<id>_Title[_<n>]            short punny name shown in the daily list
#   DailyActivityContacts_<id>_HUDDescription[_<n>]   terse HUD line ("Blow up <col: Yellow>3</col> ...")
#   DailyActivityContacts_<id>_LongDescription[_<n>]  contact's flavour brief
# The unnumbered key is variant 1; _2/_3/_4 are the extra variants. We FLATTEN to one row per (id, variant)
# so the flat JSON-catalog helpers apply unchanged; the Domain catalog re-groups by id.
#
# Values embed <col: ...> markup (resolved to a text colour by the HUD) and are kept VERBATIM. Apostrophes /
# embedded quotes round-trip through JSON. U+21B5 -> newline; other C0 control stripped. A (id,variant) row is
# kept if ANY of its three fields has text.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^DailyActivityContacts_(?<id>.+?)_(?<field>Title|HUDDescription|LongDescription)(?:_(?<n>\d+))?=(?<val>.*)$'

$idOrder = New-Object System.Collections.Generic.List[string]  # first-seen id order
$data    = @{}                                                 # key "id|variant" -> [ordered]{title,hud,long}
$keyOrder = New-Object System.Collections.Generic.List[string] # first-seen (id|variant) order

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $f   = $m.Groups['field'].Value
    $n   = if ($m.Groups['n'].Success) { [int]$m.Groups['n'].Value } else { 1 }
    $val = Clean $m.Groups['val'].Value
    if (-not $idOrder.Contains($id)) { $idOrder.Add($id) | Out-Null }
    $key = "$id|$n"
    if (-not $data.ContainsKey($key)) {
        $data[$key] = [ordered]@{ title = ""; hud_description = ""; long_description = "" }
        $keyOrder.Add($key) | Out-Null
    }
    switch ($f) {
        'Title'           { $data[$key].title            = $val }
        'HUDDescription'  { $data[$key].hud_description   = $val }
        'LongDescription' { $data[$key].long_description  = $val }
    }
}

# Emit grouped by base-id file order, variants ascending, so `order` is stable and readable.
$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $idOrder) {
    if ($id -eq 'None') { continue }
    $variants = @($data.Keys | Where-Object { $_ -like "$id|*" } | ForEach-Object { [int]($_ -split '\|')[1] } | Sort-Object)
    foreach ($v in $variants) {
        $d = $data["$id|$v"]
        $any = $d.title.Length + $d.hud_description.Length + $d.long_description.Length
        if ($any -eq 0) { continue }
        $rows.Add([pscustomobject][ordered]@{
            id               = $id
            variant          = $v
            title            = $d.title
            hud_description  = $d.hud_description
            long_description = $d.long_description
            order            = $ord
            source           = "DailyActivityContacts.INT"
        }) | Out-Null
        $ord++
    }
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, ...), while leaving genuine
# control-char / quote / backslash escapes intact (keeps <col:...> markup literal).
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " daily-activity rows (" + $idOrder.Count + " activities) -> " + $Out)
