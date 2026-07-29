[CmdletBinding()]
param(
    [string]$ProjectRoot
)

$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}
. (Join-Path $PSScriptRoot "scripts\APBPortContract.ps1")

function Assert-PortContract {
    param(
        [Parameter(Mandatory = $true)][bool]$Condition,
        [Parameter(Mandatory = $true)][string]$Message
    )

    if (-not $Condition) {
        throw "APB_PORT_CONSISTENCY_FAIL: $Message"
    }
    Write-Host "PASS: $Message"
}

function Get-IniPort {
    param(
        [Parameter(Mandatory = $true)][string]$Ini,
        [Parameter(Mandatory = $true)][string]$Key
    )

    $match = [regex]::Match($Ini, "(?m)^$Key=(?<value>\d+)\r?$")
    if (-not $match.Success) {
        throw "APB_PORT_CONSISTENCY_FAIL: [APBServer] missing $Key"
    }
    return [int]$match.Groups["value"].Value
}

$ports = Get-APBPortContract -ProjectRoot $ProjectRoot
Assert-PortContract ($ports.World -eq 17778) "APBPorts.h World=17778"
Assert-PortContract ($ports.Relay -eq 17800) "APBPorts.h Relay=17800"
Assert-PortContract ($ports.DistrictBase -eq 17810) "APBPorts.h DistrictBase=17810"

$header = Get-Content -LiteralPath (Join-Path $ProjectRoot "Source\APBReloaded\Systems\APBPorts.h") -Raw
Assert-PortContract ($header -match "numeric_id\s*>\s*0\s*\?\s*\(DistrictBase\s*\+\s*numeric_id\)\s*:\s*0") "APBPorts.h DistrictPort uses numeric_id and rejects zero"

$ini = Get-Content -LiteralPath (Join-Path $ProjectRoot "Config\DefaultGame.ini") -Raw
Assert-PortContract ((Get-IniPort -Ini $ini -Key "WorldPort") -eq $ports.World) "DefaultGame.ini WorldPort matches APBPorts.h"
Assert-PortContract ((Get-IniPort -Ini $ini -Key "RelayPort") -eq $ports.Relay) "DefaultGame.ini RelayPort matches APBPorts.h"
Assert-PortContract ((Get-IniPort -Ini $ini -Key "DistrictPortBase") -eq $ports.DistrictBase) "DefaultGame.ini DistrictPortBase matches APBPorts.h"

$worldScript = Get-Content -LiteralPath (Join-Path $ProjectRoot "tools\scripts\start_world.ps1") -Raw
$districtScript = Get-Content -LiteralPath (Join-Path $ProjectRoot "tools\scripts\start_district.ps1") -Raw
$m6GateScript = Get-Content -LiteralPath (Join-Path $ProjectRoot "tools\run_m6_world_gate.ps1") -Raw
$verificationGateScript = Get-Content -LiteralPath (Join-Path $ProjectRoot "tools\run_verification_gates.ps1") -Raw
Assert-PortContract ($worldScript -match "Get-APBPortContract") "start_world.ps1 resolves the header contract"
Assert-PortContract ($districtScript -match "Get-APBPortContract" -and $districtScript -match "Get-APBDistrictPort") "start_district.ps1 resolves the header contract and numeric_id port"
Assert-PortContract ($worldScript -notmatch '\[int\]\s*\$WorldPort\s*=\s*17778' -and $worldScript -notmatch '\[int\]\s*\$RelayPort\s*=\s*17800') "start_world.ps1 has no port literal defaults"
Assert-PortContract ($districtScript -notmatch '\[int\]\s*\$RelayPort\s*=\s*17800' -and $districtScript -notmatch '\[int\]\s*\$DistrictPortBase\s*=\s*17810') "start_district.ps1 has no port literal defaults"
Assert-PortContract ($m6GateScript -match 'Get-APBPortContract' -and $m6GateScript -notmatch '\[int\]\s*\$Port\s*=\s*17778') "run_m6_world_gate.ps1 resolves the header World port"
Assert-PortContract ($verificationGateScript -match 'Get-APBPortContract' -and $verificationGateScript -notmatch '\$WSPort\s*=\s*17778') "run_verification_gates.ps1 resolves the header World port"

$catalog = Get-Content -LiteralPath (Join-Path $ProjectRoot "Content\Data\districts.json") -Raw | ConvertFrom-Json
$expectedDistricts = @(
    [pscustomobject]@{ Id = "Financial"; NumericId = 1; Port = 17811 },
    [pscustomobject]@{ Id = "FinancialChaos"; NumericId = 2; Port = 17812 },
    [pscustomobject]@{ Id = "PGAsylum"; NumericId = 4; Port = 17814 },
    [pscustomobject]@{ Id = "PGBeacon"; NumericId = 5; Port = 17815 },
    [pscustomobject]@{ Id = "PGCrate"; NumericId = 6; Port = 17816 },
    [pscustomobject]@{ Id = "Social"; NumericId = 9; Port = 17819 },
    [pscustomobject]@{ Id = "Waterfront"; NumericId = 11; Port = 17821 },
    [pscustomobject]@{ Id = "FinancialRiot"; NumericId = 12; Port = 17822 }
)
Assert-PortContract ($catalog.Count -eq $expectedDistricts.Count) "district catalog has the expected eight entries"
foreach ($expected in $expectedDistricts) {
    $entry = @($catalog | Where-Object { $_.id -eq $expected.Id })
    Assert-PortContract ($entry.Count -eq 1) "district catalog contains $($expected.Id)"
    Assert-PortContract ([int]$entry[0].numeric_id -eq $expected.NumericId) "$($expected.Id) numeric_id=$($expected.NumericId)"
    Assert-PortContract ((Get-APBDistrictPort -Ports $ports -NumericId ([int]$entry[0].numeric_id)) -eq $expected.Port) "$($expected.Id) port=$($expected.Port)"
}

foreach ($docPath in @("work\ARCHITECTURE.md", "work\m7_spec.md")) {
    $doc = Get-Content -LiteralPath (Join-Path $ProjectRoot $docPath) -Raw
    Assert-PortContract ($doc -match "World.*17778" -and $doc -match "Relay.*17800" -and $doc -match "(DistrictBase.*17810|17810.*DistrictBase)") "$docPath publishes the APBPorts allocation"
    foreach ($expected in $expectedDistricts) {
        Assert-PortContract ($doc -match "\|\s*$($expected.Id)\s*\|\s*$($expected.NumericId)\s*\|\s*$($expected.Port)\s*\|") "$docPath publishes $($expected.Id)=$($expected.Port)"
    }
}

Write-Host "APB_PORT_CONSISTENCY_PASS world=$($ports.World) relay=$($ports.Relay) district_base=$($ports.DistrictBase) districts=$($expectedDistricts.Count)"
