param(
  [string]$Scratch = "$env:TEMP\apb_m7_relay_listener_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Game = "D:\APBReloaded\Binaries\Win64\APBReloaded.exe",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$RelayPort = 17800,
  [int]$TimeoutSec = 45
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$logPath = Join-Path $Scratch "world_relay.log"
$world = $null
$failure = $null

function Fail([string]$Reason) {
  throw [System.InvalidOperationException]::new($Reason)
}

function Stop-WorldProcess {
  if ($null -ne $world) {
    try {
      if (-not $world.HasExited) {
        Stop-Process -Id $world.Id -Force -ErrorAction SilentlyContinue
        $world.WaitForExit(10000) | Out-Null
      }
    } catch {}
  }
  Get-Process -Name "UnrealEditor","CrashReportClientEditor" -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -like "D:\UE58\UE_5.8\Engine\Binaries\Win64\*" } |
    Stop-Process -Force -ErrorAction SilentlyContinue
}

function Send-RelayLine([string]$Line, [bool]$ReadResponse) {
  $client = [System.Net.Sockets.TcpClient]::new()
  try {
    $client.Connect("127.0.0.1", $RelayPort)
    $stream = $client.GetStream()
    $stream.ReadTimeout = 5000
    $stream.WriteTimeout = 5000
    $bytes = [Text.Encoding]::UTF8.GetBytes($Line)
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
    if (-not $ReadResponse) {
      Start-Sleep -Milliseconds 300
      return ""
    }

    $response = [System.Text.StringBuilder]::new()
    while ($true) {
      $next = $stream.ReadByte()
      if ($next -lt 0) { break }
      if ($next -eq 10) { break }
      [void]$response.Append([char]$next)
    }
    return $response.ToString()
  } finally {
    $client.Dispose()
  }
}

try {
  if (-not (Test-Path -LiteralPath $Project)) { Fail "project_missing" }
  if (-not (Test-Path -LiteralPath $Editor)) { Fail "editor_binary_missing" }

  $ticketSecretLine = Get-Content (Join-Path $projectRoot "Config\DefaultGame.ini") |
    Where-Object { $_ -match '^TicketSecret=' } | Select-Object -First 1
  if ([string]::IsNullOrWhiteSpace($ticketSecretLine)) { Fail "ticket_secret_missing" }
  $secret = $ticketSecretLine.Substring("TicketSecret=".Length)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue

  $frontendMap = "/Game/Maps/Lvl_APB_Frontend"
  $worldArgs = @(
    $Project, "$frontendMap`?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-Port=17778", "-RelayPort=$RelayPort",
    "-nullrhi", "-nosound", "-unattended", "-log", "-AbsLog=$logPath"
  )
  $world = Start-Process -FilePath $Editor -ArgumentList $worldArgs -PassThru -WorkingDirectory (Split-Path $Editor) -NoNewWindow

  $startup = [Diagnostics.Stopwatch]::StartNew()
  while ($startup.Elapsed.TotalSeconds -lt $TimeoutSec) {
    if ($world.HasExited) { Fail "world_exited_before_listen" }
    if ((Test-Path $logPath) -and ((Get-Content $logPath -Raw -ErrorAction SilentlyContinue) -match "RELAY_LISTEN port=$RelayPort")) {
      break
    }
    Start-Sleep -Milliseconds 250
  }
  if ($startup.Elapsed.TotalSeconds -ge $TimeoutSec) { Fail "listen_timeout" }

  $nowMs = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
  $register = @{ version = 1; request_id = "gate-register-1"; sent_ms = $nowMs; auth = $secret; verb = "register"; district = "Financial"; numeric_id = 1; port = 17811 } |
    ConvertTo-Json -Compress
  $ack = Send-RelayLine "$register`n" $true | ConvertFrom-Json
  if ($ack.verb -ne "register_ack" -or $ack.request_id -ne "gate-register-1" -or -not $ack.ok -or $ack.numeric_id -ne 1) {
    Fail "register_ack_invalid"
  }

  $badAuth = @{ version = 1; request_id = "gate-auth-fail-1"; sent_ms = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds(); auth = "wrong-auth"; verb = "register"; district = "Financial"; numeric_id = 1; port = 17811 } |
    ConvertTo-Json -Compress
  [void](Send-RelayLine "$badAuth`n" $false)

  [void](Send-RelayLine (("X" * 65537) + "`n") $false)

  $checks = [Diagnostics.Stopwatch]::StartNew()
  while ($checks.Elapsed.TotalSeconds -lt $TimeoutSec) {
    if ($world.HasExited) { Fail "world_exited_during_probe" }
    $log = if (Test-Path $logPath) { Get-Content $logPath -Raw -ErrorAction SilentlyContinue } else { "" }
    if ($log -match "RELAY_REGISTER district=Financial numeric_id=1 ok=1" -and
        $log -match "RELAY_REJECT reason=auth_failed" -and
        $log -match "RELAY_REJECT reason=oversize") {
      break
    }
    Start-Sleep -Milliseconds 250
  }
  if ($checks.Elapsed.TotalSeconds -ge $TimeoutSec) { Fail "relay_markers_missing" }
} catch {
  $failure = $_.Exception.Message.Replace("`r", " ").Replace("`n", " ")
} finally {
  Stop-WorldProcess
  Write-Host "===== world_relay.log ====="
  if (Test-Path $logPath) {
    Get-Content $logPath | Where-Object { $_ -match "RELAY_" }
  } else {
    Write-Host "(no world relay log written)"
  }
}

if (-not [string]::IsNullOrWhiteSpace($failure)) {
  Write-Host "RELAY_LISTENER_GATE_FAIL $failure"
  exit 1
}

Write-Host "RELAY_LISTENER_GATE_OK"
exit 0
