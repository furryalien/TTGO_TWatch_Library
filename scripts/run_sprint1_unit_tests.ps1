$ErrorActionPreference = 'Stop'

$python = "C:/Users/dgran/code/TTGO_TWatch_Library/.venv/Scripts/python.exe"
$repo = "C:/Users/dgran/code/TTGO_TWatch_Library"

Push-Location $repo
try {
    & $python -m pytest tests -q --cov=tools --cov-report=term --cov-report=xml --cov-fail-under=95
    if ($LASTEXITCODE -ne 0) {
        throw "Sprint 1 unit tests failed"
    }
    Write-Host "Sprint 1 unit tests passed."
}
finally {
    Pop-Location
}
