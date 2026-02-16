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
    throw "Python was not found. Install Python or create .venv first."
}

if (-not $pythonArgs) {
    $pythonArgs = @()
}
$examples = @(
    "examples/BasicUnit/AXP20x_IRQ",
    "examples/BasicUnit/BMA423_Accel",
    "examples/BasicUnit/RTC",
    "examples/BasicUnit/TouchPad"
)

Push-Location $repo
try {
    # Check if PlatformIO is available
    $pioAvailable = $false
    try {
        & $python @pythonArgs -m platformio --version 2>&1 | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $pioAvailable = $true
        }
    } catch {
        # Command failed
    }
    
    if (-not $pioAvailable) {
        Write-Warning "PlatformIO is not available in selected interpreter. Attempting install..."
        & $python @pythonArgs -m pip install --upgrade pip
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to upgrade pip in selected interpreter."
        }

        & $python @pythonArgs -m pip install platformio
        if ($LASTEXITCODE -ne 0) {
            throw "Failed to install PlatformIO in selected interpreter."
        }

        & $python @pythonArgs -m platformio --version 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            throw "PlatformIO installation completed but module still unavailable for selected interpreter."
        }

        Write-Host "PlatformIO installed successfully for selected interpreter." -ForegroundColor Green
    }

    foreach ($example in $examples) {
        Write-Host "Building $example for ttgo-t-watch (2020-v1)..."
        & $python @pythonArgs -m platformio ci $example --board ttgo-t-watch --lib . --project-option "build_flags=-D LILYGO_WATCH_2020_V1"
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed for $example"
        }
    }
    Write-Host "Integration compile checks passed."
}
finally {
    Pop-Location
}
