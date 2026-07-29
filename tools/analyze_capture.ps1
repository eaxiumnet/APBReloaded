param(
  [string]$Img = "D:\APBReloaded\work\logs\frontend_login_capture.png"
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing
$bmp = New-Object System.Drawing.Bitmap $Img
$W = $bmp.Width; $H = $bmp.Height
Write-Output "IMAGE ${W}x${H}"

# --- Content bounding box: find pixels that are NOT near-black background ---
# (background bed is dark; panel + amber frame + text are brighter)
$minX = $W; $minY = $H; $maxX = 0; $maxY = 0
$step = 2
$brightCount = 0
for ($y = 0; $y -lt $H; $y += $step) {
  for ($x = 0; $x -lt $W; $x += $step) {
    $c = $bmp.GetPixel($x, $y)
    $lum = 0.299*$c.R + 0.587*$c.G + 0.114*$c.B
    if ($lum -gt 40) {
      $brightCount++
      if ($x -lt $minX) { $minX = $x }
      if ($y -lt $minY) { $minY = $y }
      if ($x -gt $maxX) { $maxX = $x }
      if ($y -gt $maxY) { $maxY = $y }
    }
  }
}
Write-Output "CONTENT_BBOX left=$minX top=$minY right=$maxX bottom=$maxY (of ${W}x${H})"
Write-Output "CONTENT_MARGINS L=$minX R=$($W-$maxX) T=$minY B=$($H-$maxY)"
Write-Output "BRIGHT_PIXELS sampled=$brightCount"

# --- Detect amber pixels: high R, mid-high G, low B (gold/amber family) ---
# Report the AVERAGE amber-ish pixel so we can compare to #FFC254 (255,194,84) vs washed pale.
$ar=0.0;$ag=0.0;$ab=0.0;$an=0
for ($y = 0; $y -lt $H; $y += 2) {
  for ($x = 0; $x -lt $W; $x += 2) {
    $c = $bmp.GetPixel($x, $y)
    if ($c.R -gt 150 -and $c.G -gt 90 -and $c.G -lt 230 -and $c.B -lt 150 -and ($c.R - $c.B) -gt 70) {
      $ar+=$c.R;$ag+=$c.G;$ab+=$c.B;$an++
    }
  }
}
if ($an -gt 0) {
  Write-Output ("AMBER_AVG R={0:N0} G={1:N0} B={2:N0} n={3}  (target #FFC254 = 255,194,84)" -f ($ar/$an),($ag/$an),($ab/$an),$an)
} else { Write-Output "AMBER_AVG none-found" }

# --- Sample the mid-grey panel: pixels that are near-neutral grey (R~=G~=B), mid luminance ---
$gr=0.0;$gg=0.0;$gb=0.0;$gn=0
for ($y = 0; $y -lt $H; $y += 2) {
  for ($x = 0; $x -lt $W; $x += 2) {
    $c = $bmp.GetPixel($x, $y)
    $mx = [Math]::Max($c.R,[Math]::Max($c.G,$c.B))
    $mn = [Math]::Min($c.R,[Math]::Min($c.G,$c.B))
    if (($mx - $mn) -le 12 -and $mn -ge 40 -and $mx -le 150) {
      $gr+=$c.R;$gg+=$c.G;$gb+=$c.B;$gn++
    }
  }
}
if ($gn -gt 0) {
  Write-Output ("PANEL_GREY_AVG R={0:N0} G={1:N0} B={2:N0} n={3}  (target #4F4F4F = 79,79,79 ; washed-bug ~150,150,150)" -f ($gr/$gn),($gg/$gn),($gb/$gn),$gn)
} else { Write-Output "PANEL_GREY_AVG none-found" }

# --- Horizontal luminance profile (10 columns) to see where the panel sits ---
$cols = 10
$profile = ""
for ($i = 0; $i -lt $cols; $i++) {
  $x = [int](($i + 0.5) * $W / $cols)
  $sum = 0.0; $n = 0
  for ($y = 0; $y -lt $H; $y += 4) { $c = $bmp.GetPixel($x,$y); $sum += (0.299*$c.R+0.587*$c.G+0.114*$c.B); $n++ }
  $profile += ("{0:N0} " -f ($sum/$n))
}
Write-Output "HLUMA_PROFILE(L->R): $profile"
$bmp.Dispose()
