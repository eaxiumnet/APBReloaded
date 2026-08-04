param(
  [string]$Scratch = "$env:TEMP\apb_m7_relay_listener_gate",
  [string]$Project = "D:\APBReloaded\APBReloaded.uproject",
  [string]$Game = "D:\APBReloaded\Binaries\Win64\APBReloaded.exe",
  [string]$Editor = "D:\UE58\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe",
  [int]$RelayPort = 17800,
  [int]$TimeoutSec = 45
)

$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "scripts\APBGateCleanup.ps1")
$logPath = Join-Path $Scratch "world_relay.log"
$world = $null
$failure = $null

function Fail([string]$Reason) {
  throw [System.InvalidOperationException]::new($Reason)
}

function Stop-WorldProcess([switch]$BestEffort) {
  Stop-APBGateProcesses -Tracked @($world) -Project $Project -EngineBin (Split-Path $Editor) -BestEffort:$BestEffort
}

function Get-RelaySecretHex([string]$MaterialHex) {
  $root = [byte[]]::new([Math]::Floor($MaterialHex.Length / 2))
  for ($i = 0; $i -lt $root.Length; $i++) {
    $root[$i] = [Convert]::ToByte($MaterialHex.Substring($i * 2, 2), 16)
  }
  $purpose = [Text.Encoding]::ASCII.GetBytes("apb/relay/v1")
  $hmac = [System.Security.Cryptography.HMACSHA256]::new($root)
  try { $digest = $hmac.ComputeHash($purpose) } finally { $hmac.Dispose() }
  return -join ($digest | ForEach-Object { $_.ToString("x2") })
}

function Get-RelayFrameAuth([string]$CanonicalJson, [string]$RelaySecretHex) {
  $key = [Text.Encoding]::ASCII.GetBytes($RelaySecretHex)
  $data = [Text.Encoding]::ASCII.GetBytes($CanonicalJson)
  $hmac = [System.Security.Cryptography.HMACSHA256]::new($key)
  try { $digest = $hmac.ComputeHash($data) } finally { $hmac.Dispose() }
  return -join ($digest | ForEach-Object { $_.ToString("x2") })
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

  # M16 zero-trust: world/district processes preflight APB_DEPLOYMENT_SECRET and halt when it
  # is missing. The spine exports it for child gates; standalone leg runs must set it too.
  [Environment]::SetEnvironmentVariable('APB_DEPLOYMENT_SECRET', ('a1' * 32), 'Process')
  $relaySecretHex = Get-RelaySecretHex -MaterialHex ('a1' * 32)

  Remove-Item -LiteralPath $Scratch -Recurse -Force -ErrorAction SilentlyContinue
  New-Item -ItemType Directory -Force -Path $Scratch | Out-Null
  Remove-Item -LiteralPath $logPath -Force -ErrorAction SilentlyContinue
  # M7 teardown hardening: kill any leftover editors from a prior run BEFORE launching,
  # so this gate never starts with a live ghost. Sweep-and-verify throws if any persist.
  Stop-WorldProcess

  $frontendMap = "/Game/Maps/Lvl_APB_Frontend"
  $worldArgs = @(
    $Project, "$frontendMap`?listen?game=/Script/APBReloaded.APBWorldGameMode",
    "-game", "-WorldServer", "-Port=17778", "-RelayPort=$RelayPort",
    "-nullrhi", "-nosound", "-unattended", "-AbsLog=$logPath"
  )
  $world = Start-Process -FilePath $Editor -ArgumentList $worldArgs -PassThru -WorkingDirectory (Split-Path $Editor) -WindowStyle Hidden

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
  $sentMsText = $nowMs.ToString([System.Globalization.CultureInfo]::InvariantCulture)
  $canonical = '{"version":1,"request_id":"gate-register-1","sent_ms":' + $sentMsText +
    ',"auth":"","verb":"register","district":"Financial","numeric_id":1,"port":17811}' + "`n"
  $frameAuth = Get-RelayFrameAuth -CanonicalJson $canonical -RelaySecretHex $relaySecretHex
  $register = $canonical.Replace('"auth":""', '"auth":"' + $frameAuth + '"')
  $ack = Send-RelayLine $register $true | ConvertFrom-Json
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
  Stop-WorldProcess -BestEffort
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
