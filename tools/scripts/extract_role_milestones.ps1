param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\RoleMilestones.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\role_milestones.json"
)

# Extract the APB role-MILESTONE catalog from the retail RoleMilestones.INT (UTF-16LE mirror of the
# cooked SDD table "RoleMilestones"). A milestone is one rank/step of a player role: as you progress
# a role (see player_roles.json / PlayerRoles.INT) you clear milestones, each of which has a display
# Title and, for the ones that grant loot, a reward-mail Subject + Body.
#
# Three keys per milestone id:
#   RoleMilestones_<id>_Title=<display title>
#   RoleMilestones_<id>_RewardMailSubject=<mail subject>       (often empty)
#   RoleMilestones_<id>_RewardMailBody=<mail body>             (often empty; may contain U+21B5)
# The milestone id carries a trailing "_<NN>" rank (e.g. "15th_Anniversary_Celebrations_01"); the
# base part binds to a player_roles id. Rows where all three fields are empty are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    # Collapse control chars + the manual line-break glyph U+21B5 to a space, then squeeze.
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029\u21b5]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

$rx = [regex]'^RoleMilestones_(?<id>.+?)_(?<field>Title|RewardMailSubject|RewardMailBody)=(?<val>.*)$'

# Group the three fields per id, preserving first-seen order.
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
        $data[$id] = [ordered]@{ title = ""; reward_mail_subject = ""; reward_mail_body = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Title'             { $data[$id].title = $val }
        'RewardMailSubject' { $data[$id].reward_mail_subject = $val }
        'RewardMailBody'    { $data[$id].reward_mail_body = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.title.Length -eq 0 -and $d.reward_mail_subject.Length -eq 0 -and $d.reward_mail_body.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id                  = $id
        title               = $d.title
        reward_mail_subject = $d.reward_mail_subject
        reward_mail_body    = $d.reward_mail_body
        order               = $ord
        source              = "RoleMilestones.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >, ...), while leaving
# genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " role milestones -> " + $Out)
