param(
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Map = "/Game/Maps/Lvl_APB_Frontend",
  [string]$OutDir = "D:\APBReloaded\work\logs",
  [string]$Stage = "Login",
  [int]$ResX = 1600,
  [int]$ResY = 900,
  [int]$ReadyWaitSec = 240
)

$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$out = Join-Path $OutDir ("frontend_{0}_capture.png" -f $Stage.ToLower())
Remove-Item $out -ErrorAction SilentlyContinue
$gameLog = "D:\APBReloaded\Saved\Logs\APBReloaded.log"
Remove-Item $gameLog -ErrorAction SilentlyContinue

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
  public static void AltUnlock() {
    // Press+release ALT to clear the foreground lock so SetForegroundWindow works from a bg proc.
    keybd_event(0x12, 0, 0, UIntPtr.Zero);
    keybd_event(0x12, 0, 2, UIntPtr.Zero);
  }
  public static List<IntPtr> ForPid(uint want) {
    var res = new List<IntPtr>();
    EnumWindows((h,p) => {
      uint pid; GetWindowThreadProcessId(h, out pid);
      if (pid == want && IsWindowVisible(h)) res.Add(h);
      return true;
    }, IntPtr.Zero);
    return res;
  }
}
"@

$args = @($Project, $Map, "-game", "-windowed", "-ResX=$ResX", "-ResY=$ResY", "-nosplash", "-nosound",
  "-APBHoldStage=$Stage", "-ExecCmds=t.IdleWhenNotForeground 0")
$p = Start-Process -FilePath $Editor -ArgumentList $args -PassThru
Write-Output "LAUNCHED pid=$($p.Id) stage=$Stage waiting for HOLD_STAGE ..."

$deadline = (Get-Date).AddSeconds($ReadyWaitSec)
$ready = $false
while ((Get-Date) -lt $deadline) {
  Start-Sleep -Seconds 2
  if (Test-Path $gameLog) {
    $txt = Get-Content $gameLog -Raw -ErrorAction SilentlyContinue
    if ($txt -match "HOLD_STAGE=") { $ready = $true; break }
  }
  if ($p.HasExited) { Write-Output "PROC_EXITED_EARLY code=$($p.ExitCode)"; break }
}
if (-not $ready) { Write-Output "STAGE_NOT_READY"; try { $p.Kill() } catch {}; exit 2 }

Start-Sleep -Seconds 5

$wins = [Win32]::ForPid([uint32]$p.Id)
$best = [IntPtr]::Zero; $bestArea = 0
foreach ($h in $wins) {
  $r = New-Object Win32+RECT
  [void][Win32]::GetWindowRect($h, [ref]$r)
  $w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
  $sb = New-Object System.Text.StringBuilder 256
  [void][Win32]::GetWindowText($h, $sb, 256)
  $title = $sb.ToString()
  Write-Output ("WIN h=0x{0:X} {1}x{2} title='{3}'" -f [int64]$h, $w, $hh, $title)
  if ($title -like "*UnrealEditor.exe*") { continue }
  $area = $w * $hh
  if ($area -gt $bestArea) { $bestArea = $area; $best = $h }
}
if ($best -eq [IntPtr]::Zero) { Write-Output "NO_GAME_WINDOW"; try { $p.Kill() } catch {}; exit 3 }

# Force the game window to the very top and focus it (defeats qwen's terminal occlusion).
[void][Win32]::ShowWindow($best, 9)          # SW_RESTORE
[Win32]::AltUnlock()
[void][Win32]::SetWindowPos($best, [Win32]::TOPMOST, 0, 0, 0, 0, 0x0043)  # NOMOVE|NOSIZE|SHOWWINDOW
[void][Win32]::BringWindowToTop($best)
[void][Win32]::SetForegroundWindow($best)
Start-Sleep -Seconds 3   # let it render a fresh foreground frame

$r = New-Object Win32+RECT
[void][Win32]::GetWindowRect($best, [ref]$r)
$w = $r.Right - $r.Left; $hh = $r.Bottom - $r.Top
$bmp = New-Object System.Drawing.Bitmap $w, $hh
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.CopyFromScreen($r.Left, $r.Top, 0, 0, (New-Object System.Drawing.Size($w, $hh)))
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
# Drop topmost so the window doesn't linger over everything.
[void][Win32]::SetWindowPos($best, [Win32]::NOTOPMOST, 0, 0, 0, 0, 0x0043)
try { $p.Kill() } catch {}
Write-Output "CAPTURED $out ${w}x${hh}"
