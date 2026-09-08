param(
    [string]$Version = "dev",
    [string]$Configuration = "release",
    [string]$Architecture = "x86",
    [string]$OutputRoot = "artifacts/dist"
)

$ErrorActionPreference = "Stop"

$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
$outputRootPath = Join-Path $repoRoot $OutputRoot
$stageRoot = Join-Path $outputRootPath "stage"
$packageRoot = Join-Path $outputRootPath "packages"
$appStage = Join-Path $stageRoot "TPFanCtrl2-app"
$testStage = Join-Path $stageRoot "TPFanCtrl2-tests"

Remove-Item -LiteralPath $stageRoot -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $appStage, $testStage, $packageRoot | Out-Null

$binRoot = Join-Path $repoRoot "artifacts/bin"
$appExe = Join-Path $binRoot "TPFanCtrl2.exe"
$cliExe = Join-Path $binRoot "TPFanCtrl2-cli.exe"
$logicTest = Join-Path $binRoot "logic_test.exe"
$coreTest = Join-Path $binRoot "core_test.exe"
$sampleConfig = Join-Path $repoRoot "fancontrol/TPFanCtrl2.ini"
$lpcAcpiEc = Join-Path $repoRoot "assets/LpcACPIEC.bin"
$license = Join-Path $repoRoot "LICENSE"

$requiredFiles = @($sampleConfig)
if (-not (Test-Path -LiteralPath $appExe) -and -not (Test-Path -LiteralPath $cliExe)) {
    throw "Required package input is missing: neither TPFanCtrl2.exe nor TPFanCtrl2-cli.exe was found in $binRoot"
}

Copy-Item -LiteralPath $sampleConfig -Destination $appStage
if (Test-Path -LiteralPath $appExe) { Copy-Item -LiteralPath $appExe -Destination $appStage }
if (Test-Path -LiteralPath $cliExe) { Copy-Item -LiteralPath $cliExe -Destination $appStage }
if (Test-Path -LiteralPath $lpcAcpiEc) { Copy-Item -LiteralPath $lpcAcpiEc -Destination $appStage }
if (Test-Path -LiteralPath $license) { Copy-Item -LiteralPath $license -Destination $appStage }

$hasTests = (Test-Path -LiteralPath $logicTest) -and (Test-Path -LiteralPath $coreTest)
if ($hasTests) {
    Copy-Item -LiteralPath $logicTest -Destination $testStage
    Copy-Item -LiteralPath $coreTest -Destination $testStage
}

$packagesList = @(
    [ordered]@{ name = "app"; path = "TPFanCtrl2-v$Version-windows-$Architecture-app.zip" }
)
if ($hasTests) {
    $packagesList += [ordered]@{ name = "tests"; path = "TPFanCtrl2-v$Version-windows-$Architecture-tests.zip" }
}

$manifest = [ordered]@{
    name = "TPFanCtrl2"
    version = $Version
    configuration = $Configuration
    architecture = $Architecture
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    packages = $packagesList
}

$manifestPath = Join-Path $outputRootPath "manifest.json"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Copy-Item -LiteralPath $manifestPath -Destination $appStage
if ($hasTests) {
    Copy-Item -LiteralPath $manifestPath -Destination $testStage
}

$appZip = Join-Path $packageRoot "TPFanCtrl2-v$Version-windows-$Architecture-app.zip"
Remove-Item -LiteralPath $appZip -Force -ErrorAction SilentlyContinue
Compress-Archive -Path (Join-Path $appStage "*") -DestinationPath $appZip

Write-Host "Created packages:"
Write-Host "  $appZip"

if ($hasTests) {
    $testZip = Join-Path $packageRoot "TPFanCtrl2-v$Version-windows-$Architecture-tests.zip"
    Remove-Item -LiteralPath $testZip -Force -ErrorAction SilentlyContinue
    Compress-Archive -Path (Join-Path $testStage "*") -DestinationPath $testZip
    Write-Host "  $testZip"
}
Remove-Item -LiteralPath $stageRoot -Recurse -Force

$appSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $appZip).Hash.ToLower()
Write-Host "App package SHA256: $appSha256"

# Generate matching WinGet manifests with verified package dependency
$genWingetScript = Join-Path $PSScriptRoot "generate-winget-manifest.ps1"
if (Test-Path -LiteralPath $genWingetScript) {
    & $genWingetScript -Version $Version -Architecture $Architecture -InstallerSha256 $appSha256
}
