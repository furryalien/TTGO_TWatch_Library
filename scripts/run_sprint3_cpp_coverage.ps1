$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $scriptDir "..")
$testSource = "tests/cpp/twatch_power_policy_test.cpp"
$outputDir = "coverage/cpp"
$buildDir = "$outputDir/build"
$reportFile = "$outputDir/coverage_cpp.xml"

Push-Location $repo
try {
    if (-not (Get-Command g++ -ErrorAction SilentlyContinue)) {
        Write-Host "Skipping Sprint 3 C++ coverage: g++ not found on PATH."
        exit 0
    }

    if (-not (Get-Command gcovr -ErrorAction SilentlyContinue)) {
        Write-Host "Skipping Sprint 3 C++ coverage: gcovr not found on PATH."
        exit 0
    }

    if (-not (Test-Path $outputDir)) {
        New-Item -ItemType Directory -Path $outputDir | Out-Null
    }
    if (-not (Test-Path $buildDir)) {
        New-Item -ItemType Directory -Path $buildDir | Out-Null
    }

    $binary = "$buildDir/policy_test"

    & g++ -std=c++11 -fprofile-arcs -ftest-coverage -I . $testSource -o $binary
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 3 C++ test compilation failed"
    }

    & $binary
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 3 C++ test execution failed"
    }

    & gcovr --root . --filter "src/board/twatch_power_policy.h" --xml-pretty --output $reportFile
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 3 C++ coverage report generation failed"
    }

    Write-Host "Sprint 3 C++ coverage checks passed."
}
finally {
    Pop-Location
}