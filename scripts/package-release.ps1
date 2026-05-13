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
$logicTest = Join-Path $binRoot "logic_test.exe"
$coreTest = Join-Path $binRoot "core_test.exe"
$sampleConfig = Join-Path $repoRoot "fancontrol/TPFanCtrl2.ini"

foreach ($required in @($appExe, $logicTest, $coreTest, $sampleConfig)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Required package input is missing: $required"
    }
}

Copy-Item -LiteralPath $appExe -Destination $appStage
Copy-Item -LiteralPath $sampleConfig -Destination $appStage
Copy-Item -LiteralPath $logicTest -Destination $testStage
Copy-Item -LiteralPath $coreTest -Destination $testStage

$manifest = [ordered]@{
    name = "TPFanCtrl2"
    version = $Version
    configuration = $Configuration
    architecture = $Architecture
    generatedAtUtc = (Get-Date).ToUniversalTime().ToString("o")
    packages = @(
        [ordered]@{ name = "app"; path = "TPFanCtrl2-v$Version-windows-$Architecture-app.zip" },
        [ordered]@{ name = "tests"; path = "TPFanCtrl2-v$Version-windows-$Architecture-tests.zip" }
    )
}

$manifestPath = Join-Path $outputRootPath "manifest.json"
$manifest | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
Copy-Item -LiteralPath $manifestPath -Destination $appStage
Copy-Item -LiteralPath $manifestPath -Destination $testStage

$appZip = Join-Path $packageRoot "TPFanCtrl2-v$Version-windows-$Architecture-app.zip"
$testZip = Join-Path $packageRoot "TPFanCtrl2-v$Version-windows-$Architecture-tests.zip"
Remove-Item -LiteralPath $appZip, $testZip -Force -ErrorAction SilentlyContinue

Compress-Archive -Path (Join-Path $appStage "*") -DestinationPath $appZip
Compress-Archive -Path (Join-Path $testStage "*") -DestinationPath $testZip
Remove-Item -LiteralPath $stageRoot -Recurse -Force

Write-Host "Created packages:"
Write-Host "  $appZip"
Write-Host "  $testZip"
