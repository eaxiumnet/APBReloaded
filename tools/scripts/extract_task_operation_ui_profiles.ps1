param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\TaskOperationUIProfile.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\task_operation_ui_profiles.json"
)

# Extract the retail APB TASK-OPERATION UI-PROFILE tracked-value labels from TaskOperationUIProfile.INT
# (UTF-16LE mirror of the cooked SDD table "TaskOperationUIProfile"). Each mission-operation UI profile
# (its id shares the TaskOperation id space 1:1 — e.g. "AntiGraffiti", "ArmedGuard", "Escape120") carries a
# fixed 4-slot array of HUD "tracked value" labels:
#   TaskOperationUIProfile_<id>_TrackedValueDescription[0..3]
# These are the short labels the mission HUD shows beside each tracked counter for a stage of that operation
# type (slot 0 the primary, e.g. AntiGraffiti -> "Cover Graffiti:", ArmedGuard -> "Guard Targets:"). Most
# profiles only fill slot 0; the placeholders "None"/"Simple" leave all four empty.
#
# Companion to the already-ported task_operations catalog (MissionOperationCatalog): task_operations holds the
# single UIDescription per operation; THIS holds the up-to-4 per-tracked-value HUD sub-labels. FLATTENED to one
# row per profile with a fixed desc0..desc3 quartet so the flat JSON-catalog helpers apply unchanged. Values are
# kept VERBATIM (<col:...> markup preserved); U+21B5 -> newline; other C0 control stripped. A profile is kept
# only if ANY of its four slots has text (drops "None"/"Simple" and other all-empty placeholders).

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^TaskOperationUIProfile_(?<id>.+?)_TrackedValueDescription\[(?<idx>\d+)\]=(?<val>.*)$'

$order = New-Object System.Collections.Generic.List[string]
$data  = @{}

foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rx.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $idx = [int]$m.Groups['idx'].Value
    $val = Clean $m.Groups['val'].Value
    if (-not $data.ContainsKey($id)) {
        $data[$id] = [ordered]@{ desc0 = ""; desc1 = ""; desc2 = ""; desc3 = "" }
        $order.Add($id) | Out-Null
    }
    if ($idx -ge 0 -and $idx -le 3) { $data[$id]["desc$idx"] = $val }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    if ($id -eq 'None') { continue }                 # DNT placeholder
    $d = $data[$id]
    $any = $d.desc0.Length + $d.desc1.Length + $d.desc2.Length + $d.desc3.Length
    if ($any -eq 0) { continue }                     # keep only profiles that render at least one label
    $rows.Add([pscustomobject][ordered]@{
        id     = $id
        desc0  = $d.desc0
        desc1  = $d.desc1
        desc2  = $d.desc2
        desc3  = $d.desc3
        order  = $ord
        source = "TaskOperationUIProfile.INT"
    }) | Out-Null
    $ord++
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
Write-Output ("Wrote " + $rows.Count + " task-operation UI profiles -> " + $Out)
