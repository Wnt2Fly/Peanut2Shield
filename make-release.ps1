# Builds both firmware variants and merges each into a single image that flashes
# at address 0x0 (browser: https://espressif.github.io/esptool-js/ or
# esptool.py --chip esp32s3 write_flash 0x0 <file>). Output: firmware\
#
# Run from PowerShell:  powershell -ExecutionPolicy Bypass -File make-release.ps1

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$version = (Select-String -Path 'src\config.h' -Pattern 'CFG_FIRMWARE_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
$build   = '.pio\build\waveshare-esp32-s3-zero'
$app0    = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'
$out     = 'firmware'

New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter 'Peanut2Shield-*.bin' | Remove-Item

function Build-Variant([string]$name, [string]$flags) {
    if ($flags) { $env:PLATFORMIO_BUILD_FLAGS = $flags }
    else { Remove-Item Env:PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue }

    pio run
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $name" }

    $file = Join-Path $out "Peanut2Shield-$version-$name.bin"
    pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 merge_bin -o $file `
        0x0 "$build\bootloader.bin" 0x8000 "$build\partitions.bin" `
        0xe000 $app0 0x10000 "$build\firmware.bin"
    if ($LASTEXITCODE -ne 0) { throw "Merge failed: $name" }
    Write-Host "Wrote $file"
}

try {
    Build-Variant 'longpress' '-DCFG_LONG_PRESS=1'
    # Standard last so .pio is left on the default build for `pio run -t upload`.
    Build-Variant 'standard' ''
}
finally {
    Remove-Item Env:PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue
}
