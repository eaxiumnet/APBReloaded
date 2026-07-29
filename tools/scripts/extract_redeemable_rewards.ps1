param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\RedeemableRewards.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\redeemable_rewards.json"
)

# Extract the APB redeemable-reward MAIL catalog from the retail RedeemableRewards.INT (UTF-16LE mirror of
# the cooked SDD table "RedeemableRewards"). A redeemable reward is a player-CHOICE reward: the player
# picks one item from a set (Retail/Leased weapon presets, clothing, titles, weapon skins, vehicles,
# emotes, bundles, etc.) and the grant sends a confirmation mail. This is the player-choice half of the
# reward-mail system, alongside the weighted-reward mails (WeightedRewards.INT / weighted_rewards.json)
# and the reward-package DISPLAY descriptions (RewardPackages.INT / reward_packages.json).
#
# Two keys per reward id:
#   RedeemableRewards_<id>_MailSubject=<mail subject line>
#   RedeemableRewards_<id>_MailBody=<mail body>            (empty for many Leased/preset ids)
# Rows where both fields are empty are dropped, leaving the ~1471 rows that carry real mail text.
# Prose kept VERBATIM (apostrophes, quotes): the manual line-break glyph U+21B5 (paragraph breaks in the
# instructional bodies) becomes a real newline; other C0 control chars are stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^RedeemableRewards_(?<id>.+?)_(?<field>MailSubject|MailBody)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ mail_subject = ""; mail_body = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'MailSubject' { $data[$id].mail_subject = $val }
        'MailBody'    { $data[$id].mail_body    = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.mail_subject.Length -eq 0 -and $d.mail_body.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        mail_subject = $d.mail_subject
        mail_body    = $d.mail_body
        order        = $ord
        source       = "RedeemableRewards.INT"
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
Write-Output ("Wrote " + $rows.Count + " redeemable-reward mails -> " + $Out)
