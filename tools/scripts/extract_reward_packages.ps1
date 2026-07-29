param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\RewardPackages.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\reward_packages.json"
)

# Extract the APB reward-package display catalog from the retail RewardPackages.INT (UTF-16LE mirror of
# the cooked SDD table "RewardPackages"). A reward package is the named bundle granted by achievements,
# role milestones, missions, challenges, seasonal events, Armas purchases, etc. This INT holds only the
# human-readable DISPLAY text for each package id (the actual item payload lives in the cooked SDD and is
# resolved separately) -- it is what the rewards UI / reward mail shows the player.
#
# Two keys per package id:
#   RewardPackages_<id>_Description=<player-facing description>
#   RewardPackages_<id>_OutOfSeasonDescription=<alt text shown outside a seasonal window>  (empty in
#     the current retail build, kept for schema completeness / event rotation)
# The id prefix names the family (Weapon/Symbol/Crim/Enf/Clothing/Title/Vehicle/WeaponSkin/Christmas/
# Ach/Armas/Emote/Halloween/RewardPoints/Challenges/Capacity/Easter/Tutorial/Decal/Leased/...).
# Descriptions are prose (apostrophes, quotes, "APB$"): kept VERBATIM. The manual line-break glyph
# U+21B5 becomes a real newline (descriptions render multi-line); other control chars are stripped.
# Rows where both fields are empty (e.g. RewardPackages_None) are dropped.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    # U+21B5 -> newline (forced line break in prose), strip CR, drop other C0 control chars but KEEP
    # newline/tab; do NOT touch apostrophes / quotes / the "APB$" text.
    $s = $s.Replace([char]0x21B5, "`n")
    $s = $s -replace "`r", ""
    $s = [regex]::Replace($s, "[\x00-\x08\x0b\x0c\x0e-\x1f\u2028\u2029]", "")
    return $s.Trim()
}

$rx = [regex]'^RewardPackages_(?<id>.+?)_(?<field>Description|OutOfSeasonDescription)=(?<val>.*)$'

# Group the two fields per id, preserving first-seen order.
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
        $data[$id] = [ordered]@{ description = ""; out_of_season_description = "" }
        $order.Add($id) | Out-Null
    }
    switch ($f) {
        'Description'             { $data[$id].description               = $val }
        'OutOfSeasonDescription'  { $data[$id].out_of_season_description = $val }
    }
}

$rows = New-Object System.Collections.Generic.List[object]
$ord  = 0
foreach ($id in $order) {
    $d = $data[$id]
    if ($d.description.Length -eq 0 -and $d.out_of_season_description.Length -eq 0) { continue }
    $rows.Add([pscustomobject][ordered]@{
        id                        = $id
        description               = $d.description
        out_of_season_description = $d.out_of_season_description
        order                     = $ord
        source                    = "RewardPackages.INT"
    }) | Out-Null
    $ord++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under Windows PowerShell
# 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (angle brackets, apostrophes, &, ...), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " reward packages -> " + $Out)
