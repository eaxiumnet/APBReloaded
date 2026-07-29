# Extract real contact roster + per-contact level counts from the retail
# ContactLevels.INT localization file (the shipped mirror of the SDD table
# "ContactLevel"). Emits Content/Data/contact_levels.json for the Domain
# ProgressionCatalog to consume instead of one invented ladder for every contact.
#
# Keys in the INT look like:
#   ContactLevels_<Contact>_Level<NN>_RewardMailSubject=<text>
#   ContactLevels_<Contact>_Level<NN>_RewardMailBody=<text>
# Per-contact max_level = highest <NN> observed. Non-placeholder subjects (anything
# other than the "DNT - DO NOT TRANSLATE" localization stub) are kept as evidence.
#
# Usage:
#   .\extract_contact_levels.ps1
#   .\extract_contact_levels.ps1 -Source "<path to ContactLevels.INT>" -Out "<path to json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\ContactLevels.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\contact_levels.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "ContactLevels.INT not found at $Source" }

$rx = [regex]'^ContactLevels_(?<contact>[A-Za-z0-9_]+?)_Level(?<lvl>\d+)_RewardMail(?<kind>Subject|Body)=(?<val>.*)$'
$placeholder = 'DNT - DO NOT TRANSLATE'

# Ordered by first appearance so the emitted JSON mirrors the INT's roster order.
$order = New-Object System.Collections.Generic.List[string]
$maxLevel = @{}
$subjects = @{}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rx.Match($line)
  if (-not $m.Success) { continue }
  $contact = $m.Groups['contact'].Value
  $lvl = [int]$m.Groups['lvl'].Value
  if (-not $maxLevel.ContainsKey($contact)) {
    $order.Add($contact); $maxLevel[$contact] = 0; $subjects[$contact] = New-Object System.Collections.Generic.List[string]
  }
  if ($lvl -gt $maxLevel[$contact]) { $maxLevel[$contact] = $lvl }
  if ($m.Groups['kind'].Value -eq 'Subject') {
    $val = $m.Groups['val'].Value.Trim()
    if ($val -and $val -ne $placeholder -and -not $subjects[$contact].Contains($val)) { $subjects[$contact].Add($val) }
  }
}

# Emit a PURE top-level array (matches contacts_lore.json convention so the Domain
# JsonSplitObjects parser splits one object per contact). Provenance rides on each
# object's "source" field, exactly like the apbdb-seeded catalogs.
$src = "APBGame/Localization/INT/ContactLevels.INT (retail; mirror of SDD table ContactLevel)"
$contacts = foreach ($c in $order) {
  [ordered]@{
    contact_id = $c
    max_level = $maxLevel[$c]
    reward_mail_subjects = @($subjects[$c])
    source = $src
  }
}

# ConvertTo-Json collapses a single-element collection to a bare object; force an array.
$json = ConvertTo-Json -InputObject @($contacts) -Depth 6

# Windows PowerShell's ConvertTo-Json \uXXXX-escapes apostrophes, ampersands, and angle
# brackets. The Domain's naive JsonGetString/JStr does NOT decode \uXXXX (it would keep a
# literal "u0027"), so decode every \uXXXX back to its literal char EXCEPT the quote (0x22)
# and backslash (0x5c), which must stay escaped for the parser's string scan. Control chars
# (< 0x20) are left escaped too so the emitted JSON stays valid.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
  param($mm)
  $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
  if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
  return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json)
Write-Host "WROTE $Out ($($order.Count) contacts)"
foreach ($c in $order) { Write-Host ("  {0} -> max_level {1}" -f $c, $maxLevel[$c]) }
