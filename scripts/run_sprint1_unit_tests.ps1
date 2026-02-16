$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$repo = Resolve-Path (Join-Path $scriptDir "..")
$venvPython = Join-Path $repo ".venv/Scripts/python.exe"
$python = if (Test-Path $venvPython) { $venvPython } else { "python" }

Push-Location $repo
try {
    & $python -m pytest tests -q --cov=tools.twatch_power_policy --cov-report=term --cov-report=xml --cov-fail-under=95
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 1 unit tests failed"
    }
    Write-Host "Sprint 1 unit tests passed."
}
finally {
    Pop-Location
}
