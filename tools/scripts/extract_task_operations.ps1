# Extract per-operation objective-type UI labels from the retail TaskOperations.INT localization
# file (the shipped mirror of the SDD table "TaskOperation"). Emits Content/Data/task_operations.json
# for the Domain MissionOperationCatalog — the short HUD label shown for each mission operation /
# objective type ("Checkpoint", "Graffiti Target", "Bomb Target", "Escape!", ...).
#
# Keys in the INT look like:
#   TaskOperations_<OpId>_UIDescription=<short text>
# OpId is everything between the "TaskOperations_" prefix and the "_UIDescription" suffix
# (e.g. "AntiGraffiti10NoHoldPoints"). Placeholder ops with an empty UIDescription (e.g. "None",
# "OppositionDefault") carry no HUD label and are skipped, matching the ContactLevels convention
# of keeping only real evidence.
#
# Usage:
#   .\extract_task_operations.ps1
#   .\extract_task_operations.ps1 -Source "<path to TaskOperations.INT>" -Out "<path to json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\TaskOperations.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\task_operations.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "TaskOperations.INT not found at $Source" }

$rx = [regex]'^TaskOperations_(?<id>.+)_UIDescription=(?<val>.*)$'

function Clean([string]$s) {
  # UE3 INT flattens multi-line SDD text into one physical line; newlines are encoded as bare
  # control chars AND the U+21B5 "return" glyph. Collapse any run of these to a single space.
  if ($null -eq $s) { return "" }
  return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

# Ordered by first appearance so the emitted JSON mirrors the INT roster order.
$order = New-Object System.Collections.Generic.List[string]
$label = @{}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rx.Match($line)
  if (-not $m.Success) { continue }
  $id = $m.Groups['id'].Value
  if ($id -eq 'None' -or $id -eq '') { continue }
  $val = Clean($m.Groups['val'].Value)
  if ($val.Length -eq 0) { continue }   # skip placeholder ops with no HUD label
  if (-not $label.ContainsKey($id)) { $order.Add($id) }
  $label[$id] = $val
}

$src = "APBGame/Localization/INT/TaskOperations.INT (retail; mirror of SDD table TaskOperation)"
$rows = foreach ($id in $order) {
  [ordered]@{
    id             = $id
    ui_description = $label[$id]
    source         = $src
  }
}

# ConvertTo-Json collapses a single-element collection to a bare object; force an array.
$json = ConvertTo-Json -InputObject @($rows) -Depth 6

# Windows PowerShell's ConvertTo-Json \uXXXX-escapes apostrophes, ampersands, and angle brackets.
# The Domain's naive JStr does NOT decode \uXXXX (it would keep a literal "u0027"), so decode every
# \uXXXX back to its literal char EXCEPT the quote (0x22) and backslash (0x5c). Control chars were
# already stripped by Clean().
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
  param($mm)
  $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
  if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
  return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json)
$distinct = ($label.Values | Sort-Object -Unique).Count
Write-Host ("WROTE {0} ({1} operations, {2} distinct labels)" -f $Out, $order.Count, $distinct)
