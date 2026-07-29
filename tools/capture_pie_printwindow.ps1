param(
  [string]$OutDir = "D:\APBReloaded\work\logs",
  [string]$Tag = "pie"
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class PieWin {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool MoveWindow(IntPtr h, int x, int y, int w, int hh, bool repaint);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
  public struct RECT { public int Left, Top, Right, Bottom; }
  public class Win { public IntPtr H; public string Title; public string Cls; public int W; public int Hh; public bool Vis; public int X; public int Y; }
  public static List<Win> ForPid(uint want) {
    var res = new List<Win>();
    EnumWindows((h,p) => {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid==want) {
        RECT r; GetWindowRect(h, out r);
        var t = new StringBuilder(256); GetWindowText(h, t, 256);
        var c = new StringBuilder(256); GetClassName(h, c, 256);
        res.Add(new Win{ H=h, Title=t.ToString(), Cls=c.ToString(), W=r.Right-r.Left, Hh=r.Bottom-r.Top, Vis=IsWindowVisible(h), X=r.Left, Y=r.Top });
      }
      return true;
    }, IntPtr.Zero);
    return res;
  }
  public static RECT Rect(IntPtr h){ RECT r; GetWindowRect(h, out r); return r; }
}
"@

$p = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Sort-Object StartTime | Select-Object -First 1
if (-not $p) { Write-Output "NO_EDITOR"; exit 4 }
Write-Output "PID=$($p.Id)"

$wins = [PieWin]::ForPid([uint32]$p.Id)
foreach ($win in $wins) {
  Write-Output ("WIN h=0x{0:X} vis={1} pos=({2},{3}) {4}x{5} cls='{6}' title='{7}'" -f [int64]$win.H, $win.Vis, $win.X, $win.Y, $win.W, $win.Hh, $win.Cls, $win.Title)
}

# PIE game window = VISIBLE UnrealWindow with EMPTY title and real area (editor windows have titles).
$cand = $wins | Where-Object { $_.Vis -and $_.Cls -eq 'UnrealWindow' -and [string]::IsNullOrEmpty($_.Title) -and ($_.W * $_.Hh) -gt 200000 } |
        Sort-Object { $_.W * $_.Hh } -Descending | Select-Object -First 1
if (-not $cand) { Write-Output "NO_PIE_WINDOW_CANDIDATE"; exit 3 }
Write-Output ("TARGET h=0x{0:X} pos=({1},{2}) {3}x{4}" -f [int64]$cand.H, $cand.X, $cand.Y, $cand.W, $cand.Hh)

# Move fully onto the primary monitor via MoveWindow (different path than SetWindowPos), foreground, settle.
[void][PieWin]::ShowWindow($cand.H, 9)   # SW_RESTORE
[void][PieWin]::MoveWindow($cand.H, 0, 0, $cand.W, $cand.Hh, $true)
[void][PieWin]::BringWindowToTop($cand.H)
[void][PieWin]::SetForegroundWindow($cand.H)
Start-Sleep -Milliseconds 1500

$r = [PieWin]::Rect($cand.H)
$w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
Write-Output "GRAB_RECT pos=($($r.Left),$($r.Top)) ${w}x${hh}"

# Method 1: CopyFromScreen (works now that the window is on-screen + foreground).
$out1 = Join-Path $OutDir ("hud_{0}_screen.png" -f $Tag)
$bmp1 = New-Object System.Drawing.Bitmap $w, $hh
$g1 = [System.Drawing.Graphics]::FromImage($bmp1)
$g1.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
$bmp1.Save($out1, [System.Drawing.Imaging.ImageFormat]::Png)
$g1.Dispose(); $bmp1.Dispose()
Write-Output "SAVED_SCREEN $out1"

# Method 2: PrintWindow PW_RENDERFULLCONTENT (redundant; may be black on DX but cheap to try).
$out2 = Join-Path $OutDir ("hud_{0}_printwindow.png" -f $Tag)
$bmp2 = New-Object System.Drawing.Bitmap $w, $hh
$g2 = [System.Drawing.Graphics]::FromImage($bmp2)
$hdc = $g2.GetHdc()
$okpw = [PieWin]::PrintWindow($cand.H, $hdc, 2)
$g2.ReleaseHdc($hdc)
$bmp2.Save($out2, [System.Drawing.Imaging.ImageFormat]::Png)
$g2.Dispose(); $bmp2.Dispose()
Write-Output "SAVED_PRINTWINDOW ok=$okpw $out2"
