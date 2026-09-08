<#
.SYNOPSIS
    One-click release publishing script for TPFanCtrl2 following modern GitOps best practices.
.DESCRIPTION
    Creates an annotated Git Tag for the target version and pushes it to GitHub.
    This triggers the complete cloud CI/CD release workflow:
      - Automatic version extraction and injection into binaries (SSOT)
      - Compilation and automated testing
      - Application and test package archiving with SHA256 checksum calculation
      - Automated WinGet package manifest generation and validation
      - GitHub Release creation with automated categorized changelog
.EXAMPLE
    .\scripts\publish-release.ps1 -Version 2.8.2 -Message "Dual-fan and temperature monitoring improvements"
#>
param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Version,

    [Parameter(Position = 1)]
    [string]$Message = ""
)

$ErrorActionPreference = "Stop"

# Normalize version string: strip leading 'v' if present
if ($Version.StartsWith("v", [System.StringComparison]::OrdinalIgnoreCase)) {
    $Version = $Version.Substring(1)
}

if (-not ($Version -match '^\d+\.\d+\.\d+.*$')) {
    Write-Error "Invalid semantic version: '$Version'. Expected format: X.Y.Z (e.g. 2.8.2)"
    exit 1
}

$tag = "v$Version"
if (-not $Message) {
    $Message = "Release $tag"
}

# Ensure git working tree is clean
$gitStatus = git status --porcelain
if ($gitStatus) {
    Write-Warning "Working tree has unstaged or uncommitted changes:"
    git status -s
    $confirm = Read-Host "Do you want to continue publishing anyway? (y/N)"
    if ($confirm -ne "y" -and $confirm -ne "Y") {
        Write-Host "Aborted."
        exit 1
    }
}

# Check if tag already exists
$tagExists = git tag -l $tag
if ($tagExists) {
    Write-Error "Git tag '$tag' already exists locally or remotely. Choose a new version number."
    exit 1
}

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host " Publishing TPFanCtrl2 $tag" -ForegroundColor Green
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "Creating annotated Git tag '$tag'..."
git tag -a $tag -m $Message

Write-Host "Pushing '$tag' to remote 'origin'..."
git push origin $tag

Write-Host ""
Write-Host " Tag '$tag' successfully pushed!" -ForegroundColor Green
Write-Host "GitHub Actions Release workflow has been automatically triggered." -ForegroundColor Cyan
Write-Host "Track workflow progress at: https://github.com/Tinnci/TPFanCtrl2/actions"
Write-Host "Once complete, release will appear at: https://github.com/Tinnci/TPFanCtrl2/releases/tag/$tag"
