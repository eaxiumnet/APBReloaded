[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [string]$RegistryPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
}

$errors = [System.Collections.Generic.List[string]]::new()
$registryPath = if ([string]::IsNullOrWhiteSpace($RegistryPath)) {
    Join-Path $ProjectRoot "tools\source_registry.json"
}
else {
    [System.IO.Path]::GetFullPath($RegistryPath)
}
try {
    $registry = Get-Content -LiteralPath $registryPath -Raw | ConvertFrom-Json
}
catch {
    Write-Output "SOURCE_REGISTRY_FAIL reason=parse detail=$($_.Exception.Message)"
    exit 1
}

function Get-Value {
    param([object]$InputObject, [string]$Name)
    if ($null -eq $InputObject) { return $null }
    $property = $InputObject.PSObject.Properties[$Name]
    if ($null -eq $property) { return $null }
    return $property.Value
}

function Test-PackageVersion {
    param([string]$Path, [int]$FileVersion, [int]$Licensee)
    try {
        $stream = [System.IO.File]::OpenRead($Path)
        try {
            $bytes = [byte[]]::new(8)
            $read = $stream.Read($bytes, 0, $bytes.Length)
        }
        finally {
            $stream.Dispose()
        }
    }
    catch {
        return $false
    }
    if ($read -ne 8 -or [BitConverter]::ToUInt32($bytes, 0) -ne [uint32]2653586369) {
        return $false
    }
    return [BitConverter]::ToUInt16($bytes, 4) -eq $FileVersion -and
        [BitConverter]::ToUInt16($bytes, 6) -eq $Licensee
}

$canonicalAliases = @("retail_steam", "ref_2011")
foreach ($alias in $canonicalAliases) {
    $entry = Get-Value $registry.roots $alias
    if ($null -eq $entry) {
        $errors.Add("reason=canonical_alias_missing alias=$alias")
        continue
    }
    $version = Get-Value $entry "expected_package_version"
    $fileVersion = Get-Value $version "file_version"
    $licensee = Get-Value $version "licensee"
    $packageSubpath = Get-Value $entry "packages_subpath"
    $probe = Get-Value $entry "package_probe"
    if ($entry.canonical -ne $true -or $entry.authoritative -ne $true -or
        $null -eq $fileVersion -or $null -eq $licensee -or
        [string]::IsNullOrWhiteSpace([string]$packageSubpath) -or
        [string]::IsNullOrWhiteSpace([string]$probe)) {
        $errors.Add("reason=canonical_metadata_missing alias=$alias")
        continue
    }
    if (-not (Test-Path -LiteralPath $entry.path -PathType Container)) {
        $errors.Add("reason=canonical_root_missing alias=$alias path=$($entry.path)")
        continue
    }
    $probePath = Join-Path (Join-Path $entry.path $packageSubpath) $probe
    if (-not (Test-Path -LiteralPath $probePath -PathType Leaf)) {
        $errors.Add("reason=package_probe_missing alias=$alias path=$probePath")
        continue
    }
    if (-not (Test-PackageVersion -Path $probePath -FileVersion ([int]$fileVersion) -Licensee ([int]$licensee))) {
        $errors.Add("reason=package_version_mismatch alias=$alias path=$probePath")
    }
}

$mirror = Get-Value $registry.roots "retail_mirror"
$retail = Get-Value $registry.roots "retail_steam"
if ($null -ne $mirror -and $null -ne $retail -and
    [System.IO.Path]::GetFullPath([string]$mirror.path) -eq [System.IO.Path]::GetFullPath([string]$retail.path)) {
    $errors.Add("reason=retail_mirror_path")
}

$reader = [string]$registry.readers.umodel.path
$identity = Get-Value $registry.readers.umodel "identity"
if (-not (Test-Path -LiteralPath $reader -PathType Leaf)) {
    $errors.Add("reason=reader_missing path=$reader")
}
elseif ($null -eq $identity -or [System.IO.Path]::GetFileName($reader) -cne [string]$identity.file_name) {
    $errors.Add("reason=reader_file_name actual=$reader")
}
elseif ((Get-FileHash -LiteralPath $reader -Algorithm SHA256).Hash.ToLowerInvariant() -ne [string]$identity.sha256) {
    $errors.Add("reason=reader_sha256_mismatch")
}
elseif ((Get-Item -LiteralPath $reader).Length -ne [long]$identity.bytes) {
    $errors.Add("reason=reader_size_mismatch")
}

$migratedCallers = @(
    "tools\convert\parse_privateserver.py",
    "tools\scripts\extract_with_umodel.ps1"
)
if ($migratedCallers.Count -ne 2) {
    $errors.Add("migrated_caller_list_invalid count=$($migratedCallers.Count)")
}
$literalRootPattern = 'C:\\Program Files \(x86\)\\Steam\\steamapps\\common\\APB Reloaded|D:\\APBReloaded\\2011 apb\\APB All Points Bulletin'
foreach ($relativePath in $migratedCallers) {
    $fullPath = Join-Path $ProjectRoot $relativePath
    if (-not (Test-Path -LiteralPath $fullPath -PathType Leaf)) {
        $errors.Add("migrated_caller_missing path=$relativePath")
        continue
    }
    $content = Get-Content -LiteralPath $fullPath -Raw
    if ($content -match $literalRootPattern) {
        $errors.Add("literal_root path=$relativePath")
    }
    if ($content -notmatch 'resolve_source_root') {
        $errors.Add("registry_reference_missing path=$relativePath")
    }
}

if ($errors.Count -eq 0) {
    $resolver = Join-Path $ProjectRoot "tools\scripts\resolve_source_root.ps1"
    foreach ($alias in $canonicalAliases) {
        $resolved = & pwsh -NoProfile -File $resolver -Alias $alias -Preflight -RegistryPath $registryPath 2>&1
        if ($LASTEXITCODE -ne 0) {
            $errors.Add("reason=resolver_preflight_failed alias=$alias detail=$($resolved -join ' ')")
        }
        else {
            Write-Output "SOURCE_ROOT_RESOLVER_PASS alias=$alias path=$($resolved -join '')"
        }
    }
    $parser = Join-Path $ProjectRoot "tools\convert\parse_privateserver.py"
    $parserOutput = & python $parser --help 2>&1
    if ($LASTEXITCODE -ne 0 -or -not ($parserOutput -match '--steam-root')) {
        $errors.Add("reason=parser_resolver_failed")
    }
    else {
        Write-Output "SOURCE_CALLER_RESOLVER_PASS caller=parse_privateserver.py"
    }
    $wrapper = Join-Path $ProjectRoot "tools\scripts\extract_with_umodel.ps1"
    $output = & pwsh -NoProfile -File $wrapper -RegistryPath $registryPath -ListOnly -Package "Character\Contact\Contact_LaRocha.upk" 2>&1
    if ($LASTEXITCODE -ne 0) {
        $errors.Add("reason=reader_probe_exit code=$LASTEXITCODE")
    }
    if (-not ($output -match 'UMODEL_READER_IDENTITY_PASS')) {
        $errors.Add("reason=reader_probe_identity")
    }
    if (-not ($output -match 'UMODEL_CAPABILITY_PASS')) {
        $errors.Add("reason=reader_probe_capability")
    }
}

if ($errors.Count -gt 0) {
    foreach ($errorLine in $errors) {
        Write-Output "SOURCE_REGISTRY_FAIL $errorLine"
    }
    exit 1
}

Write-Output "SOURCE_REGISTRY_PASS roots=$(@($registry.roots.PSObject.Properties).Count) callers=$($migratedCallers.Count) reader=$reader"
