param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\WeightedRewards.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\weighted_rewards.json"
)

# Extract the APB weighted-reward MAIL catalog from the retail WeightedRewards.INT (UTF-16LE mirror of
# the cooked SDD table "WeightedRewards"). A weighted reward is a standalone reward whose grant sends the
# player an in-game mail: contact/organisation biography lore, weapon/consumable/deployable reward mails,
# minigame + legendary drops, seasonal grants. This is the MAIL-BODY half of the reward system, the
# counterpart to the reward-package DISPLAY descriptions (RewardPackages.INT / reward_packages.json).
#
# Four keys per reward id:
#   WeightedRewards_<id>_RewardMailSubject=<mail subject line>
#   WeightedRewards_<id>_RewardMailBody=<mail body>
#   WeightedRewards_<id>_OutOfSeasonSubject=<alt subject outside a seasonal window>  (empty in retail)
#   WeightedRewards_<id>_OutOfSeasonBody=<alt body outside a seasonal window>        (empty in retail)
# The E_* / C_* families are Enforcer/Criminal reward pools that are mostly empty placeholders; rows
# where all four fields are empty are dropped, leaving the ~189 rows that carry real mail text.
# Prose kept VERBATIM (apostrophes, quotes): the manual line-break glyph U+21B5 (paragraph breaks in the
# lore bodies) becomes a real newline; other C0 control chars are stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^WeightedRewards_(?<id>.+?)_(?<field>RewardMailSubject|RewardMailBody|OutOfSeasonSubject|OutOfSeasonBody)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ reward_mail_subject = ""; reward_mail_body = ""; out_of_season_subject = ""; out_of_season_body = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'RewardMailSubject'  { $data[$id].reward_mail_subject   = $val }
        'RewardMailBody'     { $data[$id].reward_mail_body      = $val }
        'OutOfSeasonSubject' { $data[$id].out_of_season_subject = $val }
        'OutOfSeasonBody'    { $data[$id].out_of_season_body    = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.reward_mail_subject.Length -eq 0 -and $d.reward_mail_body.Length -eq 0 -and
        $d.out_of_season_subject.Length -eq 0 -and $d.out_of_season_body.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id                    = $id
        reward_mail_subject   = $d.reward_mail_subject
        reward_mail_body      = $d.reward_mail_body
        out_of_season_subject = $d.out_of_season_subject
        out_of_season_body    = $d.out_of_season_body
        order                 = $ord
        source                = "WeightedRewards.INT"
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
Write-Output ("Wrote " + $rows.Count + " weighted-reward mails -> " + $Out)
