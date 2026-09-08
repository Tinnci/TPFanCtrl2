<#
.SYNOPSIS
    Generates standard Microsoft WinGet package manifests for TPFanCtrl2,
    including the official namazso.PawnIO driver package dependency.
.PARAMETER Version
    Package version string (default: "2.6.0").
.PARAMETER Architecture
    Target architecture ("x86" or "x64", default: "x86").
.PARAMETER InstallerUrl
    Download URL for the release app zip package.
.PARAMETER InstallerSha256
    SHA-256 hash of the release app zip package.
.PARAMETER OutputDirectory
    Directory to write the generated manifests to.
.PARAMETER Validate
    Whether to run `winget validate` on the generated manifest directory.
#>
param(
    [string]$Version = "2.6.0",
    [string]$Architecture = "x86",
    [string]$InstallerUrl = "",
    [string]$InstallerSha256 = "",
    [string]$OutputDirectory = "",
    [switch]$Validate = $true
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $repoRoot "packaging/winget/manifests/t/Tinnci/TPFanCtrl2/$Version"
}

if ([string]::IsNullOrWhiteSpace($InstallerUrl)) {
    $InstallerUrl = "https://github.com/Tinnci/TPFanCtrl2/releases/download/v$Version/TPFanCtrl2-v$Version-windows-$Architecture-app.zip"
}

if ([string]::IsNullOrWhiteSpace($InstallerSha256)) {
    # Check if package zip exists locally in artifacts/dist/packages
    $localZip = Join-Path $repoRoot "artifacts/dist/packages/TPFanCtrl2-v$Version-windows-$Architecture-app.zip"
    if (Test-Path -LiteralPath $localZip) {
        $InstallerSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $localZip).Hash.ToLower()
        Write-Host "Computed SHA256 from local package: $InstallerSha256"
    } else {
        $InstallerSha256 = "0000000000000000000000000000000000000000000000000000000000000000"
        Write-Host "Warning: Local package not found, using placeholder SHA256."
    }
}

New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null

$versionYaml = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.version.1.6.0.schema.json
PackageIdentifier: Tinnci.TPFanCtrl2
PackageVersion: $Version
DefaultLocale: en-US
ManifestType: version
ManifestVersion: 1.6.0
"@

$installerYaml = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.installer.1.6.0.schema.json
PackageIdentifier: Tinnci.TPFanCtrl2
PackageVersion: $Version
MinimumOSVersion: 10.0.17763.0
InstallerType: zip
NestedInstallerType: portable
NestedInstallerFiles:
  - RelativeFilePath: TPFanCtrl2.exe
    PortableCommandAlias: TPFanCtrl2
  - RelativeFilePath: TPFanCtrl2-cli.exe
    PortableCommandAlias: TPFanCtrl2-cli
Commands:
  - TPFanCtrl2
  - TPFanCtrl2-cli
Installers:
  - Architecture: $Architecture
    InstallerUrl: $InstallerUrl
    InstallerSha256: $InstallerSha256
Dependencies:
  PackageDependencies:
    - PackageIdentifier: namazso.PawnIO
      MinimumVersion: 2.2.0
ManifestType: installer
ManifestVersion: 1.6.0
"@

$enLocaleYaml = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.defaultLocale.1.6.0.schema.json
PackageIdentifier: Tinnci.TPFanCtrl2
PackageVersion: $Version
PackageLocale: en-US
Publisher: Tinnci
PublisherUrl: https://github.com/Tinnci
PublisherSupportUrl: https://github.com/Tinnci/TPFanCtrl2/issues
Author: Tinnci
PackageName: TPFanCtrl2
PackageUrl: https://github.com/Tinnci/TPFanCtrl2
License: The Unlicense
LicenseUrl: https://github.com/Tinnci/TPFanCtrl2/blob/main/LICENSE
ShortDescription: ThinkPad fan and thermal control for modern Windows
Description: TPFanCtrl2 provides automatic and manual fan control for ThinkPad laptops with modern driver support and CLI interface.
Tags:
  - thinkpad
  - fan-control
  - hardware
  - thermal
ManifestType: defaultLocale
ManifestVersion: 1.6.0
"@

$zhLocaleYaml = @"
# yaml-language-server: `$schema=https://aka.ms/winget-manifest.locale.1.6.0.schema.json
PackageIdentifier: Tinnci.TPFanCtrl2
PackageVersion: $Version
PackageLocale: zh-CN
Publisher: Tinnci
PublisherUrl: https://github.com/Tinnci
PublisherSupportUrl: https://github.com/Tinnci/TPFanCtrl2/issues
Author: Tinnci
PackageName: TPFanCtrl2
PackageUrl: https://github.com/Tinnci/TPFanCtrl2
License: The Unlicense
LicenseUrl: https://github.com/Tinnci/TPFanCtrl2/blob/main/LICENSE
ShortDescription: 适配现代 Windows 11 的 ThinkPad 风扇与温控程序
Description: TPFanCtrl2 为 ThinkPad 笔记本提供开箱即用的风扇自动与手动控制，支持现代 Windows 11 安全驱动 PawnIO，并提供独立的 CLI 命令行工具。
Tags:
  - thinkpad
  - fan-control
  - hardware
  - thermal
  - 风扇控制
ManifestType: locale
ManifestVersion: 1.6.0
"@

$versionFile = Join-Path $OutputDirectory "Tinnci.TPFanCtrl2.yaml"
$installerFile = Join-Path $OutputDirectory "Tinnci.TPFanCtrl2.installer.yaml"
$enLocaleFile = Join-Path $OutputDirectory "Tinnci.TPFanCtrl2.locale.en-US.yaml"
$zhLocaleFile = Join-Path $OutputDirectory "Tinnci.TPFanCtrl2.locale.zh-CN.yaml"

[System.IO.File]::WriteAllText($versionFile, $versionYaml, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($installerFile, $installerYaml, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($enLocaleFile, $enLocaleYaml, [System.Text.Encoding]::UTF8)
[System.IO.File]::WriteAllText($zhLocaleFile, $zhLocaleYaml, [System.Text.Encoding]::UTF8)

Write-Host "Generated WinGet manifests at: $OutputDirectory"

if ($Validate) {
    if (Get-Command winget -ErrorAction SilentlyContinue) {
        Write-Host "Running winget validate..."
        & winget validate --manifest $OutputDirectory
    } else {
        Write-Host "winget command not available, skipping validation."
    }
}
