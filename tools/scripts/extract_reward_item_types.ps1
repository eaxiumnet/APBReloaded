param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\RewardPackageItemTypes.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\reward_item_types.json"
)

# Extract the APB reward-package ITEM-TYPE catalog from the retail RewardPackageItemTypes.INT (UTF-16LE
# mirror of the cooked SDD table "RewardPackageItemTypes"). A reward-package item type is a per-component
# entry of a reward package (vehicle customization kits, weapon/mod components, ...) carrying both a
# rewards-UI DISPLAY description AND the confirmation mail subject/body sent when that component is
# granted. This is the per-ITEM layer beneath the reward-package display blurbs (RewardPackages.INT) and
# the reward-mail catalogs (WeightedRewards.INT / RedeemableRewards.INT).
#
# Three keys per item id:
#   RewardPackageItemTypes_<id>_Description=<rewards-UI blurb>
#   RewardPackageItemTypes_<id>_MailSubject=<mail subject>   (empty for many component ids)
#   RewardPackageItemTypes_<id>_MailBody=<mail body>         (empty for many component ids)
# Rows where all three fields are empty are dropped, leaving the ~139 rows that carry real text.
# Prose kept VERBATIM (apostrophes, quotes): the manual line-break glyph U+21B5 (paragraph breaks in the
# instructional bodies/descriptions) becomes a real newline; other C0 control chars are stripped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^RewardPackageItemTypes_(?<id>.+?)_(?<field>Description|MailSubject|MailBody)=(?<val>.*)$'

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
        $data[$id] = [ordered]@{ description = ""; mail_subject = ""; mail_body = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Description' { $data[$id].description  = $val }
        'MailSubject' { $data[$id].mail_subject = $val }
        'MailBody'    { $data[$id].mail_body    = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.description.Length -eq 0 -and $d.mail_subject.Length -eq 0 -and $d.mail_body.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id           = $id
        description  = $d.description
        mail_subject = $d.mail_subject
        mail_body    = $d.mail_body
        order        = $ord
        source       = "RewardPackageItemTypes.INT"
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
Write-Output ("Wrote " + $rows.Count + " reward-package item types -> " + $Out)
