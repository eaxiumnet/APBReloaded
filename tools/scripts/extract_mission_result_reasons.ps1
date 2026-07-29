# Extract mission end-screen result messages from the retail MissionResultReasons.INT localization
# file (the shipped mirror of the SDD table "MissionResultReason"). Emits
# Content/Data/mission_result_reasons.json for the Domain MissionResultReasonCatalog — the
# authentic Win / Lose / Draw messages APB shows when a mission ends, keyed by result-reason id
# (e.g. "TimedOut", "CompletedUnopposed", "OppositionDestroyedOwnerTarget", "WonFinalObjective").
#
# Keys in the INT look like:
#   MissionResultReasons_<ReasonId>_WinMessage=<text>
#   MissionResultReasons_<ReasonId>_LoseMessage=<text>
#   MissionResultReasons_<ReasonId>_DrawMessage=<text>
# A reason has up to three perspective-specific messages; any of them may be empty. Reasons where
# ALL THREE messages are empty (e.g. the placeholder "None") carry no display text and are skipped,
# matching the "keep only real evidence" convention of the other retail catalogs.
#
# Usage:
#   .\extract_mission_result_reasons.ps1
#   .\extract_mission_result_reasons.ps1 -Source "<path to MissionResultReasons.INT>" -Out "<json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\MissionResultReasons.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\mission_result_reasons.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "MissionResultReasons.INT not found at $Source" }

$rx = [regex]'^MissionResultReasons_(?<id>.+)_(?<kind>Win|Lose|Draw)Message=(?<val>.*)$'

function Clean([string]$s) {
  # UE3 INT flattens multi-line SDD text into one physical line; newlines are encoded as bare
  # control chars AND the U+21B5 "return" glyph. Collapse any run of these to a single space.
  if ($null -eq $s) { return "" }
  return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

# Ordered by first appearance so the emitted JSON mirrors the INT roster order.
$order = New-Object System.Collections.Generic.List[string]
$win   = @{}
$lose  = @{}
$draw  = @{}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rx.Match($line)
  if (-not $m.Success) { continue }
  $id = $m.Groups['id'].Value
  if ($id -eq 'None' -or $id -eq '') { continue }
  $kind = $m.Groups['kind'].Value
  $val  = Clean($m.Groups['val'].Value)
  if (-not $order.Contains($id)) { $order.Add($id) }
  switch ($kind) {
    'Win'  { $win[$id]  = $val }
    'Lose' { $lose[$id] = $val }
    'Draw' { $draw[$id] = $val }
  }
}

$src = "APBGame/Localization/INT/MissionResultReasons.INT (retail; mirror of SDD table MissionResultReason)"
$rows = foreach ($id in $order) {
  $w = if ($win.ContainsKey($id))  { $win[$id] }  else { "" }
  $l = if ($lose.ContainsKey($id)) { $lose[$id] } else { "" }
  $d = if ($draw.ContainsKey($id)) { $draw[$id] } else { "" }
  # Skip reasons with no display text at all (all three perspectives empty).
  if (($w.Length -eq 0) -and ($l.Length -eq 0) -and ($d.Length -eq 0)) { continue }
  [ordered]@{
    id           = $id
    win_message  = $w
    lose_message = $l
    draw_message = $d
    source       = $src
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
Write-Host ("WROTE {0} ({1} result reasons)" -f $Out, @($rows).Count)
