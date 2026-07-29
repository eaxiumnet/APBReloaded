# Extract per-stage mission briefs from the retail TaskObjectives.INT localization file (the
# shipped mirror of the SDD table "TaskObjective"). Emits Content/Data/task_objectives.json for
# the Domain MissionBriefCatalog — the authoritative owner/dispatch stage briefings shown to each
# side during a mission (e.g. "Get into our newly claimed turf and spray our colors up!").
#
# Keys in the INT look like:
#   TaskObjectives_<TemplateId>_Stage<NN>_OwnerBrief=<text>
#   TaskObjectives_<TemplateId>_Stage<NN>_DispatchBrief=<text>
# TemplateId is everything between the "TaskObjectives_" prefix and the "_Stage<NN>" segment and
# matches the MissionTemplate id space 1:1 (e.g. "AE_BCS0_Ter1_B"), so briefs join onto the
# mission_templates.json / mission scripts by id. OwnerBrief is shown to the mission owner
# (attacker); DispatchBrief is shown to the dispatched opposition (defender).
#
# Usage:
#   .\extract_task_objectives.ps1
#   .\extract_task_objectives.ps1 -Source "<path to TaskObjectives.INT>" -Out "<path to json>"

param(
  [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\TaskObjectives.INT",
  [string]$Out    = "D:\APBReloaded\Content\Data\task_objectives.json"
)

if (-not (Test-Path -LiteralPath $Source)) { throw "TaskObjectives.INT not found at $Source" }

$rx = [regex]'^TaskObjectives_(?<id>.+)_Stage(?<stage>\d+)_(?<role>Owner|Dispatch)Brief=(?<val>.*)$'

function Clean([string]$s) {
  # UE3 INT flattens multi-line SDD text into one physical line; newlines are encoded as bare
  # control chars AND the U+21B5 "return" glyph. Collapse any run of these to a single space.
  if ($null -eq $s) { return "" }
  return ([regex]::Replace($s, '[\x00-\x1f\u2028\u2029\u21b5]+', ' ')).Trim()
}

# Ordered by first appearance so the emitted JSON mirrors the INT roster order.
$order = New-Object System.Collections.Generic.List[string]
$owner = @{}
$dispatch = @{}
$templateOf = @{}
$stageOf = @{}

foreach ($line in [System.IO.File]::ReadLines($Source)) {
  $m = $rx.Match($line)
  if (-not $m.Success) { continue }
  $tid = $m.Groups['id'].Value
  if ($tid -eq 'None' -or $tid -eq '') { continue }
  $stageNum = [int]$m.Groups['stage'].Value
  $key = ("{0}_Stage{1:D2}" -f $tid, $stageNum)
  if (-not $templateOf.ContainsKey($key)) {
    $order.Add($key); $templateOf[$key] = $tid; $stageOf[$key] = $stageNum
  }
  if ($m.Groups['role'].Value -eq 'Owner') { $owner[$key] = Clean($m.Groups['val'].Value) }
  else { $dispatch[$key] = Clean($m.Groups['val'].Value) }
}

$src = "APBGame/Localization/INT/TaskObjectives.INT (retail; mirror of SDD table TaskObjective)"
$rows = foreach ($key in $order) {
  [ordered]@{
    id             = $key
    template_id    = $templateOf[$key]
    stage          = $stageOf[$key]
    owner_brief    = if ($owner.ContainsKey($key)) { $owner[$key] } else { "" }
    dispatch_brief = if ($dispatch.ContainsKey($key)) { $dispatch[$key] } else { "" }
    source         = $src
  }
}

# ConvertTo-Json collapses a single-element collection to a bare object; force an array.
$json = ConvertTo-Json -InputObject @($rows) -Depth 6

# Windows PowerShell's ConvertTo-Json \uXXXX-escapes apostrophes, ampersands, and angle brackets
# (the briefs carry <Col: StageText> markup). The Domain's naive JStr does NOT decode \uXXXX (it
# would keep a literal "u0027"/"u003c"), so decode every \uXXXX back to its literal char EXCEPT
# the quote (0x22) and backslash (0x5c). Control chars were already stripped by Clean().
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
  param($mm)
  $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
  if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
  return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json)
$templates = ($templateOf.Values | Sort-Object -Unique).Count
Write-Host ("WROTE {0} ({1} stage-brief rows across {2} templates)" -f $Out, $order.Count, $templates)
