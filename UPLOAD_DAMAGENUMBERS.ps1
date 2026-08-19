$ErrorActionPreference = "Stop"

function Pause-End {
    Write-Host ""
    Read-Host "Press Enter to close"
}

try {
    $scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
    Set-Location $scriptDir

    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host " DamageNumbers GitHub uploader " -ForegroundColor Cyan
    Write-Host "==========================================" -ForegroundColor Cyan
    Write-Host ""

    $git = Get-Command git -ErrorAction SilentlyContinue
    if (-not $git) {
        throw "Git is not installed. Install Git for Windows first, or use the TurretControl installer script you already used."
    }

    $repoUrl = Read-Host "Paste NEW empty GitHub repository URL for DamageNumbers"
    if ([string]::IsNullOrWhiteSpace($repoUrl)) {
        throw "Repository URL is required."
    }

    if (-not $repoUrl.EndsWith(".git")) {
        $repoUrl = $repoUrl.TrimEnd("/") + ".git"
    }

    Write-Host "[1/7] Initializing repository..." -ForegroundColor Cyan
    if (-not (Test-Path ".git")) {
        git init
        if ($LASTEXITCODE -ne 0) { throw "git init failed." }
    }

    Write-Host "[2/7] Setting branch main..." -ForegroundColor Cyan
    git branch -M main
    if ($LASTEXITCODE -ne 0) { throw "Could not set branch main." }

    Write-Host "[3/7] Setting remote..." -ForegroundColor Cyan
    $remotes = @(git remote)
    if ($remotes -contains "origin") {
        git remote set-url origin $repoUrl
    } else {
        git remote add origin $repoUrl
    }
    if ($LASTEXITCODE -ne 0) { throw "Could not configure remote origin." }

    Write-Host "[4/7] Adding files..." -ForegroundColor Cyan
    git add .
    if ($LASTEXITCODE -ne 0) { throw "git add failed." }

    Write-Host "[5/7] Creating commit if needed..." -ForegroundColor Cyan
    git diff --cached --quiet
    if ($LASTEXITCODE -ne 0) {
        git commit -m "Initial DamageNumbers v1.0"
        if ($LASTEXITCODE -ne 0) { throw "git commit failed." }
    } else {
        Write-Host "No new changes to commit." -ForegroundColor DarkGray
    }

    Write-Host "[6/7] Syncing remote if main exists..." -ForegroundColor Cyan
    cmd /c "git ls-remote --exit-code --heads origin main >nul 2>nul"
    if ($LASTEXITCODE -eq 0) {
        git pull --rebase origin main
        if ($LASTEXITCODE -ne 0) {
            throw "git pull --rebase failed. Create a truly empty GitHub repo and run again."
        }
    }

    Write-Host "[7/7] Pushing to GitHub..." -ForegroundColor Cyan
    git push -u origin main
    if ($LASTEXITCODE -ne 0) {
        throw "git push failed. Sign in to GitHub if prompted, then run again."
    }

    $repoWeb = $repoUrl
    if ($repoWeb.EndsWith(".git")) {
        $repoWeb = $repoWeb.Substring(0, $repoWeb.Length - 4)
    }

    Write-Host ""
    Write-Host "SUCCESS! DamageNumbers uploaded." -ForegroundColor Green
    Write-Host "Opening GitHub Actions..." -ForegroundColor Green
    Start-Process ($repoWeb + "/actions")
    Pause-End
}
catch {
    Write-Host ""
    Write-Host "ERROR:" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Write-Host ""
    Write-Host "Send me a screenshot of this window." -ForegroundColor Yellow
    Pause-End
    exit 1
}
