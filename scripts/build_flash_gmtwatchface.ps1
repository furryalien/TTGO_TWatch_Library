param(
    [string]$Port = "COM4",
    [string]$Fqbn = "esp32:esp32:twatch",
    [string]$SketchPath = "examples/LVGL/GMTWatchFace",
    [string]$BuildPath = ".arduino-build/GMTWatchFace",
    [string]$ArduinoCliPath = "C:\Program Files\Arduino CLI\arduino-cli.exe",
    [string]$Esp32CoreVersion = "2.0.17",
    [switch]$CompileOnly
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $ArduinoCliPath)) {
    throw "Arduino CLI not found at: $ArduinoCliPath"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$additionalUrl = "https://espressif.github.io/arduino-esp32/package_esp32_index.json"

function Invoke-ArduinoCli {
    param([string[]]$CliArgs)
    & $ArduinoCliPath @CliArgs
    Write-Host "Arduino CLI command: $($CliArgs -join ' ')"
    if ($LASTEXITCODE -ne 0) {
        throw "arduino-cli failed: $($CliArgs -join ' ')"
    }
}

function Get-InstalledEsp32CoreVersion {
    try {
        $json = & $ArduinoCliPath core list --format json | ConvertFrom-Json
        if (-not $json) {
            return $null
        }

        $cores = @($json)
        $esp32Core = $cores | Where-Object { $_.ID -eq "esp32:esp32" } | Select-Object -First 1
        if ($esp32Core) {
            return $esp32Core.Installed
        }

        return $null
    }
    catch {
        return $null
    }
}

Push-Location $repoRoot
try {
    $installedCoreVersion = Get-InstalledEsp32CoreVersion

    if ($installedCoreVersion -ne $Esp32CoreVersion) {
        Invoke-ArduinoCli @("core", "update-index", "--additional-urls", $additionalUrl)
        Invoke-ArduinoCli @("core", "install", "esp32:esp32@$Esp32CoreVersion", "--additional-urls", $additionalUrl)
    }

    Invoke-ArduinoCli @(
        "compile",
        "--fqbn", $Fqbn,
        "--library", ".",
        "--build-path", $BuildPath,
        $SketchPath
    )

    if (-not $CompileOnly) {
        Invoke-ArduinoCli @(
            "upload",
            "--fqbn", $Fqbn,
            "--port", $Port,
            "--input-dir", $BuildPath,
            $SketchPath
        )
    }
}
finally {
    Pop-Location
}
