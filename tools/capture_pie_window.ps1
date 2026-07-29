param(
  [string]$Out = "D:\APBReloaded\work\logs\hud_mission_capture.png"
)
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path (Split-Path $Out) | Out-Null
Remove-Item $Out -ErrorAction SilentlyContinue
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System;
using System.Text;
using System.Collections.Generic;
using System.Runtime.InteropServices;
public static class Win32 {
  public delegate bool EnumProc(IntPtr h, IntPtr p);
  [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb, IntPtr p);
  [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h, out uint pid);
  [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
  [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h, StringBuilder s, int n);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool BringWindowToTop(IntPtr h);
  [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h, int n);
  [DllImport("user32.dll")] public static extern bool SetWindowPos(IntPtr h, IntPtr after, int x, int y, int cx, int cy, uint flags);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, UIntPtr extra);
  public struct RECT { public int Left, Top, Right, Bottom; }
  public static IntPtr TOPMOST = new IntPtr(-1);
  public static IntPtr NOTOPMOST = new IntPtr(-2);
  public static void AltUnlock() { keybd_event(0x12,0,0,UIntPtr.Zero); keybd_event(0x12,0,2,UIntPtr.Zero); }
  public static List<IntPtr> ForPid(uint want) {
    var res = new List<IntPtr>();
    EnumWindows((h,p) => { uint pid; GetWindowThreadProcessId(h, out pid); if (pid==want && IsWindowVisible(h)) res.Add(h); return true; }, IntPtr.Zero);
    return res;
  }
}
"@

# Find the running UnrealEditor process (do NOT launch or kill it).
$proc = Get-Process UnrealEditor -ErrorAction SilentlyContinue | Sort-Object StartTime | Select-Object -First 1
if (-not $proc) { Write-Output "NO_EDITOR_PROC"; exit 4 }
Write-Output "EDITOR pid=$($proc.Id)"

$wins = [Win32]::ForPid([uint32]$proc.Id)
$best = [IntPtr]::Zero; $bestArea = 0
foreach ($h in $wins) {
  $r = New-Object Win32+RECT
  [void][Win32]::GetWindowRect($h, [ref]$r)
  $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
  $sb = New-Object System.Text.StringBuilder 256
  [void][Win32]::GetWindowText($h, $sb, 256)
  $title = $sb.ToString()
  Write-Output ("WIN h=0x{0:X} {1}x{2} title='{3}'" -f [int64]$h, $w, $hh, $title)
  # The PIE game viewport window title contains the map/preview; skip the main editor frame if a bigger PIE window exists.
  $area = $w * $hh
  if ($area -gt $bestArea) { $bestArea = $area; $best = $h }
}
if ($best -eq [IntPtr]::Zero) { Write-Output "NO_WINDOW"; exit 3 }

[void][Win32]::ShowWindow($best, 9)   # SW_RESTORE (un-minimize)
[Win32]::AltUnlock()
[void][Win32]::SetWindowPos($best, [Win32]::TOPMOST, 0, 0, 0, 0, 0x0043)
[void][Win32]::BringWindowToTop($best)
[void][Win32]::SetForegroundWindow($best)
Start-Sleep -Seconds 3

$r = New-Object Win32+RECT
[void][Win32]::GetWindowRect($best, [ref]$r)
$w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
$bmp.Save($Out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
[void][Win32]::SetWindowPos($best, [Win32]::NOTOPMOST, 0, 0, 0, 0, 0x0043)
Write-Output "CAPTURED $Out ${w}x${hh}"
