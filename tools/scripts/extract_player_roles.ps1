# Extract the full retail role roster (display names + descriptions) from the retail
# PlayerRoles.INT localization file (the shipped mirror of the SDD table "PlayerRoles").
# Emits Content/Data/player_roles.json for the Domain ProgressionCatalog to merge on top
# of the partial apbdb-seeded roles.json (retail-canonical, complete roster).
#
# Keys in the INT look like:
#   PlayerRoles_<RoleId>_DisplayName=<text>
#   PlayerRoles_<RoleId>_Description=<text>
# RoleId is everything between the "PlayerRoles_" prefix and the "_DisplayName"/"_Description"
# suffix (so "..._2016_PC" stays part of the id). "None" is skipped.
#
# Usage:
#   .\extract_player_roles.ps1
#   .\extract_player_roles.ps1 -Source "<path to PlayerRoles.INT>" -Out "<path to json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\PlayerRoles.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\player_roles.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "PlayerRoles.INT not found at $Source" }

$rxName = [regex]'^PlayerRoles_(?<id>.+)_DisplayName=(?<val>.*)$'
$rxDesc = [regex]'^PlayerRoles_(?<id>.+)_Description=(?<val>.*)$'

# Ordered by first appearance so the emitted JSON mirrors the INT's roster order.
$order = New-Object System.Collections.Generic.List[string]
$name = @{}
$desc = @{}

function Clean([string]$s) {
  # UE3 INT flattens multi-line SDD text into one physical line; newlines are encoded as bare
  # control chars AND the U+21B5 "return" glyph (↵). Collapse any run of these to a single
  # space so the JSON stays clean single-line.
  if ($null -eq $s) { return "" }
  return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rxName.Match($line)
  if ($m.Success) {
    $id = $m.Groups['id'].Value
    if ($id -eq 'None') { continue }
    if (-not $name.ContainsKey($id)) { $order.Add($id) }
    $name[$id] = Clean($m.Groups['val'].Value)
    continue
  }
  $m = $rxDesc.Match($line)
  if ($m.Success) {
    $id = $m.Groups['id'].Value
    if ($id -eq 'None') { continue }
    $desc[$id] = Clean($m.Groups['val'].Value)
  }
}

$src = "APBGame/Localization/INT/PlayerRoles.INT (retail; mirror of SDD table PlayerRoles)"
$roles = foreach ($id in $order) {
  [ordered]@{
    id = $id
    name = if ($name.ContainsKey($id)) { $name[$id] } else { $id }
    description = if ($desc.ContainsKey($id)) { $desc[$id] } else { "" }
    source = $src
  }
}

# ConvertTo-Json collapses a single-element collection to a bare object; force an array.
$json = ConvertTo-Json -InputObject @($roles) -Depth 6

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
Write-Host "WROTE $Out ($($order.Count) roles)"
$activity = $order | Where-Object { $_ -like 'Role2_*' }
Write-Host ("activity/weapon roles (Role2_*): {0}" -f $activity.Count)
