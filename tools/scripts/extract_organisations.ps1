param(
    [string]$Source = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT\Organisations.INT",
    [string]$Out    = "D:\APBReloaded\Content\Data\organisations.json"
)

# Extract the APB organisation catalog (contact orgs + weapon vendors + store fronts)
# from the retail Organisations.INT (UTF-16LE mirror of the cooked SDD table
# "Organisation"). Only key per org is Organisations_<id>_Name=<display>.
#
# The org->faction affiliation and the org "kind" are NOT stored in the INT (they live
# in the SDD Organisation table's Faction column, which is cooked away). They are
# canonical, unambiguous APB facts, so we classify each id here and document the source
# of the classification in work/m15_organisations_note.md. The Name string itself is
# taken verbatim from the INT.

if (-not (Test-Path $Source)) { Write-Error "Source not found: $Source"; exit 1 }

function Clean([string]$s) {
    if ($null -eq $s) { return "" }
    $s = [regex]::Replace($s, "[\x00-\x1f\u2028\u2029]+", " ")
    return ([regex]::Replace($s, "[ \t]+", " ")).Trim()
}

# Canonical APB affiliations (SDD Organisation.Faction). Named gangs + Criminal*/Enforcer*
# defaults/seasonals resolve to a faction; store/vendor/tutorial/None are neutral ("None").
$factionOf = @{
    'None'              = 'None'
    'CriminalDefault'   = 'Criminal'
    'GKings'            = 'Criminal'
    'BloodRoses'        = 'Criminal'
    'RedRain'           = 'Criminal'
    'Anarchists'        = 'Criminal'
    'CriminalSeasonal'  = 'Criminal'
    'EnforcerDefault'   = 'Enforcer'
    'Praetorian'        = 'Enforcer'
    'PrentissTigers'    = 'Enforcer'
    'SPPD'              = 'Enforcer'
    'RIOT'              = 'Enforcer'
    'EnforcerSeasonal'  = 'Enforcer'
    'JokerDistribution' = 'None'
    'JokerAffiliates'   = 'None'
    'Armas'             = 'None'
    'ArmasJB'           = 'None'
    'ArmasNTJB'         = 'None'
    'Tutorial'          = 'None'
    'BothSeasonal'      = 'None'
}
# Org "kind": what the org represents in the UI (contact grouping vs store filter).
$kindOf = @{
    'None'              = 'none'
    'CriminalDefault'   = 'default'
    'EnforcerDefault'   = 'default'
    'GKings'            = 'gang'
    'BloodRoses'        = 'gang'
    'RedRain'           = 'gang'
    'Anarchists'        = 'gang'
    'Praetorian'        = 'gang'
    'PrentissTigers'    = 'gang'
    'SPPD'              = 'gang'
    'RIOT'              = 'gang'
    'CriminalSeasonal'  = 'seasonal'
    'EnforcerSeasonal'  = 'seasonal'
    'BothSeasonal'      = 'seasonal'
    'JokerDistribution' = 'vendor'
    'JokerAffiliates'   = 'vendor'
    'Armas'             = 'store'
    'ArmasJB'           = 'store'
    'ArmasNTJB'         = 'store'
    'Tutorial'          = 'tutorial'
}

$rxName = [regex]'^Organisations_(?<id>[^=]+?)_Name=(?<val>.*)$'

$rows = New-Object System.Collections.Generic.List[object]
$rank = 0
foreach ($ln in [System.IO.File]::ReadLines($Source)) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';' -or $ln[0] -eq '[') { continue }
    $m = $rxName.Match($ln)
    if (-not $m.Success) { continue }
    $id  = $m.Groups['id'].Value
    $nm  = Clean $m.Groups['val'].Value
    if ($nm.Length -eq 0) { continue }   # skip the empty "None" row
    $fac = if ($factionOf.ContainsKey($id)) { $factionOf[$id] } else { 'None' }
    $knd = if ($kindOf.ContainsKey($id))    { $kindOf[$id] }    else { 'none' }
    $rows.Add([pscustomobject][ordered]@{
        id      = $id
        name    = $nm
        faction = $fac
        kind    = $knd
        rank    = $rank
        source  = "Organisations.INT"
    }) | Out-Null
    $rank++
}

# ConvertTo-Json on a List[object] throws "Argument types do not match" under
# Windows PowerShell 5.1; wrap as a single-element array of the materialised array.
$arr  = ,@($rows.ToArray())
$json = ConvertTo-Json -InputObject $arr[0] -Depth 6

# Restore printable chars ConvertTo-Json \u-escaped (apostrophes, &, <, >), while
# leaving genuine control-char / quote / backslash escapes intact.
$json = [regex]::Replace($json, '\\u([0-9a-fA-F]{4})', {
    param($mm)
    $code = [Convert]::ToInt32($mm.Groups[1].Value, 16)
    if ($code -lt 0x20 -or $code -eq 0x22 -or $code -eq 0x5c) { return $mm.Value }
    return [string][char]$code
})

[System.IO.File]::WriteAllText($Out, $json, (New-Object System.Text.UTF8Encoding($false)))
Write-Output ("Wrote " + $rows.Count + " organisations -> " + $Out)
