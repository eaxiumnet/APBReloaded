# Extract the canonical retail mission-title roster from the retail MissionTemplates.INT
# localization file (the shipped mirror of the SDD table "MissionTemplate"). Emits
# Content/Data/mission_templates.json for the Domain MissionTitleCatalog — the authoritative
# per-template display titles ("GANGLAND ANNEXATION", "PIMP MY CRIB", ...).
#
# Keys in the INT look like:
#   MissionTemplates_<TemplateId>_MissionTitle=<TEXT>
# TemplateId is everything between the "MissionTemplates_" prefix and the "_MissionTitle"
# suffix (e.g. "DB_BCS4_Del1", "AE_BCS0_Ter1_B"). The faction/district/activity/tier/variant
# structure is preserved verbatim in the id so downstream code can match mission scripts.
#
# Usage:
#   .\extract_mission_templates.ps1
#   .\extract_mission_templates.ps1 -Source "<path to MissionTemplates.INT>" -Out "<path to json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\MissionTemplates.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\mission_templates.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "MissionTemplates.INT not found at $Source" }

$rx = [regex]'^MissionTemplates_(?<id>.+)_MissionTitle=(?<val>.*)$'

# Ordered by first appearance so the emitted JSON mirrors the INT roster order.
$order = New-Object System.Collections.Generic.List[string]
$title = @{}

function Clean([string]$s) {
  # UE3 INT flattens multi-line SDD text into one physical line; newlines are encoded as bare
  # control chars AND the U+21B5 "return" glyph. Collapse any run of these to a single space.
  if ($null -eq $s) { return "" }
  return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rx.Match($line)
  if (-not $m.Success) { continue }
  $id = $m.Groups['id'].Value
  if ($id -eq 'None' -or $id -eq '') { continue }
  if (-not $title.ContainsKey($id)) { $order.Add($id) }
  $title[$id] = Clean($m.Groups['val'].Value)
}

$src = "APBGame/Localization/INT/MissionTemplates.INT (retail; mirror of SDD table MissionTemplate)"
$rows = foreach ($id in $order) {
  [ordered]@{
    id = $id
    title = if ($title.ContainsKey($id)) { $title[$id] } else { $id }
    source = $src
  }
}

# ConvertTo-Json collapses a single-element collection to a bare object; force an array.
$json = ConvertTo-Json -InputObject @($rows) -Depth 6

# Windows PowerShell's ConvertTo-Json \uXXXX-escapes apostrophes, ampersands, and angle
# brackets. The Domain's naive JsonGetString does NOT decode \uXXXX (it would keep a literal
# "u0027"), so decode every \uXXXX back to its literal char EXCEPT the quote (0x22) and
# backslash (0x5c), which must stay escaped for the parser's string scan. Control chars were
# already stripped by Clean(), so anything >= 0x20 is safe to inline.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
  param($mm)
  $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
  if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
  return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json)
Write-Host ("WROTE {0} ({1} mission titles)" -f $Out, $order.Count)
