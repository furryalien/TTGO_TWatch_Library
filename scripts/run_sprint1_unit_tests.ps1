$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $scriptDir "..")
$venvPython = Join-Path $repo ".venv/Scripts/python.exe"

if (Test-Path $venvPython) {
    $python = $venvPython
} elseif (Get-Command py -ErrorAction SilentlyContinue) {
    $python = "py"
    $pythonArgs = @("-3")
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
    $python = "python"
} else {
    Write-Warning "Python was not found. Skipping Sprint 1 unit tests."
    Write-Host "Sprint 1 unit tests: SKIPPED (Python not installed)" -ForegroundColor Yellow
    exit 0
}

if (-not $pythonArgs) {
    $pythonArgs = @()
}

Push-Location $repo
try {
    # Check if pytest is available
    $pytestAvailable = $false
    try {
        & $python @pythonArgs -m pytest --version 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $pytestAvailable = $true
        }
    } catch {
        # Command failed
    }
    
    if (-not $pytestAvailable) {
        Write-Warning "pytest is not available. Skipping Sprint 1 unit tests."
        Write-Warning "To enable these tests, install pytest: pip install pytest pytest-cov"
        Write-Host "Sprint 1 unit tests: SKIPPED (pytest not installed)" -ForegroundColor Yellow
        exit 0
    }

    & $python @pythonArgs -m pytest tests -q --cov=tools.twatch_power_policy --cov-report=term --cov-report=xml --cov-fail-under=95
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 1 unit tests failed"
    }
    Write-Host "Sprint 1 unit tests passed."
}
finally {
    Pop-Location
}
