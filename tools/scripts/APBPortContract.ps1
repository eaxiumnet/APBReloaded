Set-StrictMode -Version Latest

function Get-APBPortContract {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][ValidateNotNullOrEmpty()][string]$ProjectRoot
    )

    $headerPath = Join-Path $ProjectRoot "Source\APBReloaded\Systems\APBPorts.h"
    if (-not (Test-Path -LiteralPath $headerPath)) {
        throw "APBPorts.h not found at $headerPath"
    }

    $header = Get-Content -LiteralPath $headerPath -Raw
    $values = @{}
    foreach ($name in @("World", "Relay", "DistrictBase")) {
        $match = [regex]::Match($header, "inline\s+constexpr\s+int32_t\s+$name\s*=\s*(?<value>\d+)\s*;")
        if (-not $match.Success) {
            throw "APBPorts.h does not define apb::ports::$name as an integer constexpr"
        }
        $values[$name] = [int]$match.Groups["value"].Value
    }

    return [pscustomobject]@{
        World = $values["World"]
        Relay = $values["Relay"]
        DistrictBase = $values["DistrictBase"]
    }
}

function Get-APBDistrictPort {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory = $true)][pscustomobject]$Ports,
        [Parameter(Mandatory = $true)][int]$NumericId
    )

    if ($NumericId -le 0) {
        throw "APBPorts::DistrictPort requires numeric_id > 0; got $NumericId"
    }
    return $Ports.DistrictBase + $NumericId
}
