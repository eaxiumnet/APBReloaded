$ErrorActionPreference = 'Stop'
$dir = "C:\Program Files (x86)\Steam\steamapps\common\APB Reloaded\APBGame\Localization\INT"
$masc = Join-Path $dir 'Subtitles_MASC.int'
$fem  = Join-Path $dir 'Subtitles_FEM.int'
Write-Output ("masc_exists={0} fem_exists={1}" -f (Test-Path $masc), (Test-Path $fem))
Write-Output ("masc_len={0} fem_len={1}" -f (Get-Item $masc).Length, (Get-Item $fem).Length)

# byte-identical check
$hmasc = (Get-FileHash $masc -Algorithm SHA256).Hash
$hfem  = (Get-FileHash $fem  -Algorithm SHA256).Hash
Write-Output ("masc_sha={0}" -f $hmasc)
Write-Output ("fem_sha={0}"  -f $hfem)
Write-Output ("byte_identical={0}" -f ($hmasc -eq $hfem))

# parse structure: sections + key grammar
$lines = [System.IO.File]::ReadAllLines($masc)
Write-Output ("total_lines={0}" -f $lines.Count)
$sections = New-Object System.Collections.Generic.List[string]
$kv = 0; $withAt = 0; $withBracket = 0; $empty = 0
$sampleKeys = New-Object System.Collections.Generic.List[string]
foreach ($ln in $lines) {
    if ($ln.Length -eq 0) { continue }
    if ($ln[0] -eq ';') { continue }
    $mSec = [regex]::Match($ln, '^\[(.+)\]\s*$')
    if ($mSec.Success) { $sections.Add($mSec.Groups[1].Value); continue }
    $eq = $ln.IndexOf('=')
    if ($eq -lt 0) { continue }
    $kv++
    $key = $ln.Substring(0, $eq)
    $val = $ln.Substring($eq + 1)
    if ($val.Trim().Length -eq 0) { $empty++ }
    if ($key.Contains('@')) { $withAt++ }
    if ($key.Contains('[')) { $withBracket++ }
    if ($sampleKeys.Count -lt 12) { $sampleKeys.Add($key) }
}
Write-Output ("sections={0} kv={1} keys_with_at={2} keys_with_bracket={3} empty_val={4}" -f $sections.Count, $kv, $withAt, $withBracket, $empty)
Write-Output "section_names (first 12):"
$sections | Select-Object -First 12 | ForEach-Object { Write-Output ("  [{0}]" -f $_) }
Write-Output "sample_keys (first 12):"
$sampleKeys | ForEach-Object { Write-Output ("  {0}" -f $_) }

# suffix analysis: split key on '_' and look at last token distribution
$suffix = @{}
foreach ($ln in $lines) {
    $eq = $ln.IndexOf('=')
    if ($eq -lt 0) { continue }
    if ($ln.Length -eq 0 -or $ln[0] -eq ';') { continue }
    if ([regex]::IsMatch($ln, '^\[(.+)\]\s*$')) { continue }
    $key = $ln.Substring(0, $eq)
    $parts = $key.Split('_')
    $last = $parts[$parts.Count - 1]
    if ($suffix.ContainsKey($last)) { $suffix[$last]++ } else { $suffix[$last] = 1 }
}
Write-Output "top key-suffix tokens:"
$suffix.GetEnumerator() | Sort-Object Value -Descending | Select-Object -First 12 | ForEach-Object { Write-Output ("  {0,-24} {1}" -f $_.Key, $_.Value) }

# sample a few full lines
Write-Output "sample_lines (first 6 kv):"
$shown = 0
foreach ($ln in $lines) {
    if ($ln.Length -eq 0 -or $ln[0] -eq ';') { continue }
    if ([regex]::IsMatch($ln, '^\[(.+)\]\s*$')) { continue }
    if ($ln.IndexOf('=') -lt 0) { continue }
    Write-Output ("  " + $ln)
    $shown++
    if ($shown -ge 6) { break }
}
