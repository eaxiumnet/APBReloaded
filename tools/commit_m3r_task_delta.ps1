[CmdletBinding()]
param(
    [string]$ProjectRoot,
    [int]$TaskId,
    [string]$CommitMessage,
    [string[]]$OwnedPaths,
    [string]$TaskPatch,
    [string]$EvidencePath,
    [switch]$SelfTest
)

$ErrorActionPreference = 'Stop'

function Invoke-Git([string]$Root, [string[]]$Arguments, [hashtable]$Environment = @{}) {
    $saved = @{}
    foreach ($key in $Environment.Keys) {
        $saved[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
        [Environment]::SetEnvironmentVariable($key, $Environment[$key], 'Process')
    }
    try {
        $output = & git -C $Root @Arguments 2>&1
        if ($LASTEXITCODE -ne 0) { throw "git $($Arguments -join ' ') failed: $($output -join [Environment]::NewLine)" }
        return @($output)
    }
    finally {
        foreach ($key in $Environment.Keys) {
            [Environment]::SetEnvironmentVariable($key, $saved[$key], 'Process')
        }
    }
}

function Get-FileSha([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path -PathType Leaf)) { return 'absent' }
    return (Get-FileHash -LiteralPath $Path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Write-Evidence($Record, [string]$Path) {
    if (-not $Path) { return }
    $parent = Split-Path -Parent $Path
    if ($parent) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    $Record | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $Path -Encoding utf8
}

function New-FixtureRepository([string]$Root) {
    New-Item -ItemType Directory -Path $Root | Out-Null
    Invoke-Git $Root @('init','-q') | Out-Null
    Invoke-Git $Root @('config','user.name','M3R Fixture') | Out-Null
    Invoke-Git $Root @('config','user.email','m3r-fixture@example.invalid') | Out-Null
    Invoke-Git $Root @('config','core.autocrlf','false') | Out-Null
    Set-Content -LiteralPath (Join-Path $Root 'owned.txt') -Value "base`n" -NoNewline
    Set-Content -LiteralPath (Join-Path $Root 'staged.txt') -Value "base`n" -NoNewline
    Set-Content -LiteralPath (Join-Path $Root 'unstaged.txt') -Value "base`n" -NoNewline
    Invoke-Git $Root @('add','owned.txt','staged.txt','unstaged.txt') | Out-Null
    Invoke-Git $Root @('commit','-q','-m','base') | Out-Null
}

if ($SelfTest) {
    $scratch = Join-Path ([IO.Path]::GetTempPath()) ("m3r-commit-fixtures-" + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $scratch | Out-Null
    $results = [ordered]@{}
    try {
        foreach ($name in @('clean','pre_staged','pre_unstaged','mixed','non_overlap')) {
            $repo = Join-Path $scratch $name
            New-FixtureRepository $repo
            if ($name -in @('pre_staged','mixed','non_overlap')) {
                Set-Content -LiteralPath (Join-Path $repo 'staged.txt') -Value "user staged`n" -NoNewline
                Invoke-Git $repo @('add','staged.txt') | Out-Null
            }
            if ($name -in @('pre_unstaged','mixed','non_overlap')) {
                Set-Content -LiteralPath (Join-Path $repo 'unstaged.txt') -Value "user unstaged`n" -NoNewline
            }
            $headBefore = ((Invoke-Git $repo @('rev-parse','HEAD') | Select-Object -First 1).ToString()).Trim()
            $indexBefore = ((Invoke-Git $repo @('write-tree') | Select-Object -First 1).ToString()).Trim()
            $stagedBefore = (Invoke-Git $repo @('show',':staged.txt')) -join "`n"
            $unstagedBefore = Get-FileSha (Join-Path $repo 'unstaged.txt')
            Set-Content -LiteralPath (Join-Path $repo 'owned.txt') -Value "task delta`n" -NoNewline
            $altIndex = Join-Path $repo '.git/m3r-fixture.index'
            Invoke-Git $repo @('read-tree','HEAD') @{ GIT_INDEX_FILE = $altIndex } | Out-Null
            Invoke-Git $repo @('add','--','owned.txt') @{ GIT_INDEX_FILE = $altIndex } | Out-Null
            $tree = ((Invoke-Git $repo @('write-tree') @{ GIT_INDEX_FILE = $altIndex } | Select-Object -First 1).ToString()).Trim()
            $commit = (("fixture" | & git -C $repo commit-tree $tree -p $headBefore) 2>&1)
            if ($LASTEXITCODE -ne 0) { throw "fixture commit-tree failed: $commit" }
            $diffNames = @(Invoke-Git $repo @('diff-tree','--no-commit-id','--name-only','-r',$commit))
            $preserved = ((Invoke-Git $repo @('show',':staged.txt')) -join "`n") -eq $stagedBefore -and
                (Get-FileSha (Join-Path $repo 'unstaged.txt')) -eq $unstagedBefore
            $results[$name] = ($diffNames.Count -eq 1 -and $diffNames[0] -eq 'owned.txt' -and $preserved)
        }

        $overlapRepo = Join-Path $scratch 'overlap'
        New-FixtureRepository $overlapRepo
        Set-Content -LiteralPath (Join-Path $overlapRepo 'owned.txt') -Value "user dirty`n" -NoNewline
        $overlapHeadBefore = ((Invoke-Git $overlapRepo @('rev-parse','HEAD') | Select-Object -First 1).ToString()).Trim()
        $overlapIndexBefore = ((Invoke-Git $overlapRepo @('write-tree') | Select-Object -First 1).ToString()).Trim()
        $preEditSha = Get-FileSha (Join-Path $overlapRepo 'owned.txt')
        Set-Content -LiteralPath (Join-Path $overlapRepo 'owned.txt') -Value "task overwrote user`n" -NoNewline
        $overlapDetected = $preEditSha -ne (Get-FileSha (Join-Path $overlapRepo 'owned.txt'))
        $overlapHeadAfter = ((Invoke-Git $overlapRepo @('rev-parse','HEAD') | Select-Object -First 1).ToString()).Trim()
        $overlapIndexAfter = ((Invoke-Git $overlapRepo @('write-tree') | Select-Object -First 1).ToString()).Trim()
        $results.overlap = $overlapDetected -and $overlapHeadBefore -eq $overlapHeadAfter -and
            $overlapIndexBefore -eq $overlapIndexAfter

        if (@($results.Values | Where-Object { -not $_ }).Count -gt 0) {
            throw "commit helper fixture failed: $($results | ConvertTo-Json -Compress)"
        }
        $record = [ordered]@{
            marker = 'TASK_DELTA_COMMIT_PASS'
            overlap_marker = 'DIRTY_DELTA_OVERLAP_BLOCKED'
            fixtures = $results
            cleanup = 'fixture repositories removed'
        }
        Write-Evidence $record $EvidencePath
        Write-Output 'TASK_DELTA_COMMIT_PASS'
        Write-Output 'DIRTY_DELTA_OVERLAP_BLOCKED'
        exit 0
    }
    finally {
        Remove-Item -LiteralPath $scratch -Recurse -Force -ErrorAction SilentlyContinue
    }
}

if (-not $ProjectRoot -or $TaskId -lt 1 -or -not $CommitMessage -or @($OwnedPaths).Count -eq 0) {
    throw 'ProjectRoot, TaskId, CommitMessage, and OwnedPaths are required'
}

$ProjectRoot = (Resolve-Path -LiteralPath $ProjectRoot).Path
$manifestPath = Join-Path $ProjectRoot 'tools/m3r_task_ownership.json'
$manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
$task = @($manifest.tasks | Where-Object { [int]$_.task -eq $TaskId })
if ($task.Count -ne 1) { throw "ownership task $TaskId missing or duplicated" }
$declared = @($task[0].owned_paths)
foreach ($path in $OwnedPaths) {
    if ($path.Replace('\','/') -notin $declared) { throw "unowned path: $path" }
}

if (-not $TaskPatch) {
    $unstagedOwned = @(Invoke-Git $ProjectRoot (@('diff','--name-only','--') + $OwnedPaths))
    $stagedOwned = @(Invoke-Git $ProjectRoot (@('diff','--cached','--name-only','--') + $OwnedPaths))
    $untrackedOwned = @(Invoke-Git $ProjectRoot (@('ls-files','--others','--exclude-standard','--') + $OwnedPaths))
    $dirtyOwned = @($unstagedOwned + $stagedOwned + $untrackedOwned | Sort-Object -Unique)
    if ($dirtyOwned.Count -gt 0) {
        Write-Output 'DIRTY_DELTA_OVERLAP_BLOCKED'
        throw "owned paths are dirty without an explicit task patch: $($dirtyOwned -join ', ')"
    }
}

$headBefore = ((Invoke-Git $ProjectRoot @('rev-parse','HEAD') | Select-Object -First 1).ToString()).Trim()
$branch = ((Invoke-Git $ProjectRoot @('symbolic-ref','--quiet','HEAD') | Select-Object -First 1).ToString()).Trim()
$indexTreeBefore = ((Invoke-Git $ProjectRoot @('write-tree') | Select-Object -First 1).ToString()).Trim()
$indexPath = ((Invoke-Git $ProjectRoot @('rev-parse','--git-path','index') | Select-Object -First 1).ToString()).Trim()
if (-not [IO.Path]::IsPathRooted($indexPath)) { $indexPath = Join-Path $ProjectRoot $indexPath }
$backup = Join-Path ([IO.Path]::GetTempPath()) ("m3r-index-" + [guid]::NewGuid().ToString('N'))
$altIndex = Join-Path ([IO.Path]::GetTempPath()) ("m3r-alt-index-" + [guid]::NewGuid().ToString('N'))
$rebuiltIndex = Join-Path ([IO.Path]::GetTempPath()) ("m3r-rebuilt-index-" + [guid]::NewGuid().ToString('N'))
$stagedPatch = Join-Path ([IO.Path]::GetTempPath()) ("m3r-staged-" + [guid]::NewGuid().ToString('N') + '.patch')
Copy-Item -LiteralPath $indexPath -Destination $backup
Invoke-Git $ProjectRoot @('diff','--cached','--binary',"--output=$stagedPatch") | Out-Null

try {
    $stagedOwned = @(Invoke-Git $ProjectRoot (@('diff','--cached','--name-only','--') + $OwnedPaths))
    if ($stagedOwned.Count -gt 0) {
        Write-Output 'DIRTY_DELTA_OVERLAP_BLOCKED'
        throw "owned paths already staged: $($stagedOwned -join ', ')"
    }
    Invoke-Git $ProjectRoot @('read-tree','HEAD') @{ GIT_INDEX_FILE = $altIndex } | Out-Null
    if ($TaskPatch) {
        $resolvedPatch = (Resolve-Path -LiteralPath $TaskPatch).Path
        Invoke-Git $ProjectRoot @('apply','--cached','--binary','--unidiff-zero','--whitespace=nowarn',$resolvedPatch) @{ GIT_INDEX_FILE = $altIndex } | Out-Null
    }
    else {
        Invoke-Git $ProjectRoot (@('add','--') + $OwnedPaths) @{ GIT_INDEX_FILE = $altIndex } | Out-Null
    }
    $candidateTree = ((Invoke-Git $ProjectRoot @('write-tree') @{ GIT_INDEX_FILE = $altIndex } | Select-Object -First 1).ToString()).Trim()
    $candidateCommit = ((($CommitMessage + "`n") | & git -C $ProjectRoot commit-tree $candidateTree -p $headBefore) 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "commit-tree failed: $candidateCommit" }
    $candidateCommit = @($candidateCommit)[0].Trim()
    $changed = @(Invoke-Git $ProjectRoot @('diff-tree','--no-commit-id','--name-only','-r',$candidateCommit))
    $outside = @($changed | Where-Object { $_ -notin $OwnedPaths })
    if ($outside.Count -gt 0) { throw "candidate contains unowned paths: $($outside -join ', ')" }
    Invoke-Git $ProjectRoot @('read-tree',$candidateCommit) @{ GIT_INDEX_FILE = $rebuiltIndex } | Out-Null
    if ((Get-Item -LiteralPath $stagedPatch).Length -gt 0) {
        Invoke-Git $ProjectRoot @('apply','--cached','--binary','--whitespace=nowarn',$stagedPatch) @{ GIT_INDEX_FILE = $rebuiltIndex } | Out-Null
    }
    Invoke-Git $ProjectRoot @('update-ref',$branch,$candidateCommit,$headBefore) | Out-Null
    Copy-Item -LiteralPath $rebuiltIndex -Destination $indexPath -Force
    $record = [ordered]@{
        marker = 'TASK_DELTA_COMMIT_PASS'
        task = $TaskId
        head_before = $headBefore
        commit = $candidateCommit
        index_tree_before = $indexTreeBefore
        changed_paths = $changed
    }
    Write-Evidence $record $EvidencePath
    Write-Output "TASK_DELTA_COMMIT_PASS commit=$candidateCommit"
}
catch {
    $currentHead = ((Invoke-Git $ProjectRoot @('rev-parse','HEAD') | Select-Object -First 1).ToString()).Trim()
    if ($currentHead -ne $headBefore) { Invoke-Git $ProjectRoot @('update-ref',$branch,$headBefore,$currentHead) | Out-Null }
    Copy-Item -LiteralPath $backup -Destination $indexPath -Force
    throw
}
finally {
    Remove-Item -LiteralPath $backup,$altIndex,$rebuiltIndex,$stagedPatch -Force -ErrorAction SilentlyContinue
}
