$ErrorActionPreference = 'Stop'

$python = "C:/Users/dgran/code/TTGO_TWatch_Library/.venv/Scripts/python.exe"
$repo = "C:/Users/dgran/code/TTGO_TWatch_Library"
$examples = @(
    "examples/BasicUnit/AXP20x_IRQ",
    "examples/BasicUnit/BMA423_Accel",
    "examples/BasicUnit/RTC",
    "examples/BasicUnit/TouchPad"
)

Push-Location $repo
try {
    foreach ($example in $examples) {
        Write-Host "Building $example for ttgo-t-watch (2020-v1)..."
        & $python -m platformio ci $example --board ttgo-t-watch --lib . --project-option "build_flags=-D LILYGO_WATCH_2020_V1"
        if ($LASTEXITCODE -ne 0) {
            throw "Build failed for $example"
        }
    }
    Write-Host "Integration compile checks passed."
}
finally {
    Pop-Location
}
