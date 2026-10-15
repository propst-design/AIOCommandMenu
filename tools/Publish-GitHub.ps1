# Push this mod's committed main branch. No automatic commits, tags or force pushes.
param(
    [switch]$DryRun,
    [switch]$NoPause
)

$ErrorActionPreference = 'Stop'
$projectRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$repository = 'https://github.com/propst-design/AIOCommandMenu'
$exitCode = 1

try {
    $gitCommand = Get-Command git -ErrorAction SilentlyContinue
    if ($gitCommand) {
        $gitPath = $gitCommand.Source
    } else {
        $gitPath = Join-Path $env:ProgramFiles 'Git\cmd\git.exe'
        if (!(Test-Path -LiteralPath $gitPath)) {
            throw 'Git was not found. Install Git for Windows, then run this launcher again.'
        }
    }

    $topLevel = & $gitPath -C $projectRoot rev-parse --show-toplevel
    if ($LASTEXITCODE -ne 0 -or !$topLevel -or [IO.Path]::GetFullPath($topLevel) -ne $projectRoot) {
        throw 'The launcher must be inside the AiO Git repository.'
    }
    $branch = & $gitPath -C $projectRoot branch --show-current
    if ($LASTEXITCODE -ne 0 -or $branch -ne 'main') {
        throw 'Switch this repository to main before publishing.'
    }
    $pushUrls = @(& $gitPath -C $projectRoot remote get-url --push --all origin)
    $allowedUrls = @("$repository.git", $repository, 'git@github.com:propst-design/AIOCommandMenu.git')
    if ($LASTEXITCODE -ne 0 -or $pushUrls.Count -ne 1 -or $pushUrls[0] -cnotin $allowedUrls) {
        throw "origin must point to $repository.git. No remote settings were changed."
    }

    $changes = @(& $gitPath -C $projectRoot status --porcelain)
    if ($LASTEXITCODE -ne 0) { throw 'Could not check the working tree.' }
    if ($changes.Count -gt 0) {
        Write-Host 'There are uncommitted files. Only existing commits will be published.' -ForegroundColor Yellow
    }

    Write-Host "Publishing committed main to $repository ..." -ForegroundColor Cyan
    $pushArguments = @('-C', $projectRoot, 'push', '--set-upstream', '--no-follow-tags')
    if ($DryRun) { $pushArguments += '--dry-run' }
    $pushArguments += @('origin', 'refs/heads/main:refs/heads/main')
    & $gitPath @pushArguments
    if ($LASTEXITCODE -ne 0) {
        throw 'GitHub push failed. Check the Git output above for sign-in, network or remote-change errors. No force push was attempted.'
    }

    if ($DryRun) {
        Write-Host 'Dry run succeeded. Nothing was uploaded.' -ForegroundColor Green
    } else {
        Write-Host "Published successfully: $repository" -ForegroundColor Green
    }
    $exitCode = 0
} catch {
    Write-Host "Publish failed: $($_.Exception.Message)" -ForegroundColor Red
}

if (!$NoPause) {
    Write-Host ''
    [void](Read-Host 'Press Enter to close')
}
exit $exitCode
