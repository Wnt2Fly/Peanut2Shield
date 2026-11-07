# Builds both firmware variants and packages the public flasher.
#
# Output:
#   firmware/parts/{standard,longpress}/   — four-file sets
#   firmware/Peanut2Shield-<ver>-*.bin     — merged images (CLI only)
#   docs/                                  — GitHub Pages one-click flasher
#     https://wnt2fly.github.io/Peanut2Shield/
#
# Run:  powershell -ExecutionPolicy Bypass -File make-release.ps1

$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot

$version = (Select-String -Path 'src\config.h' -Pattern 'CFG_FIRMWARE_VERSION\s+"([^"]+)"').Matches[0].Groups[1].Value
$build   = '.pio\build\waveshare-esp32-s3-zero'
$app0    = Join-Path $env:USERPROFILE '.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin'
$out     = 'firmware'
$docs    = 'docs'

New-Item -ItemType Directory -Force $out | Out-Null
Get-ChildItem $out -Filter 'Peanut2Shield-*.bin' -ErrorAction SilentlyContinue | Remove-Item
Remove-Item -Recurse -Force (Join-Path $out 'parts') -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force (Join-Path $out 'web') -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force $docs -ErrorAction SilentlyContinue

function Build-Variant([string]$name, [string]$flags) {
    if ($flags) { $env:PLATFORMIO_BUILD_FLAGS = $flags }
    else { Remove-Item Env:PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue }

    pio run
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $name" }

    $merged = Join-Path $out "Peanut2Shield-$version-$name.bin"
    pio pkg exec -p tool-esptoolpy -- esptool.py --chip esp32s3 merge_bin `
        --flash_mode qio --flash_freq 80m --flash_size 4MB -o $merged `
        0x0 "$build\bootloader.bin" 0x8000 "$build\partitions.bin" `
        0xe000 $app0 0x10000 "$build\firmware.bin"
    if ($LASTEXITCODE -ne 0) { throw "Merge failed: $name" }
    Write-Host "Wrote $merged"

    $parts = Join-Path $out "parts\$name"
    New-Item -ItemType Directory -Force $parts | Out-Null
    Copy-Item "$build\bootloader.bin" (Join-Path $parts 'bootloader.bin') -Force
    Copy-Item "$build\partitions.bin" (Join-Path $parts 'partitions.bin') -Force
    Copy-Item $app0 (Join-Path $parts 'boot_app0.bin') -Force
    Copy-Item "$build\firmware.bin" (Join-Path $parts 'firmware.bin') -Force
    Write-Host "Wrote $parts\"
}

try {
    Build-Variant 'longpress' '-DCFG_LONG_PRESS=1'
    Build-Variant 'standard' ''
}
finally {
    Remove-Item Env:PLATFORMIO_BUILD_FLAGS -ErrorAction SilentlyContinue
}

# ---- Public flasher (GitHub Pages = /docs) ----
New-Item -ItemType Directory -Force $docs | Out-Null
Copy-Item -Recurse (Join-Path $out 'parts') (Join-Path $docs 'parts') -Force
'' | Set-Content -Encoding ascii (Join-Path $docs '.nojekyll')  # serve .bin files as-is

$verShort = $version.TrimStart('v')
@"
{
  "name": "Peanut2Shield standard",
  "version": "$verShort",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "parts/standard/bootloader.bin", "offset": 0 },
        { "path": "parts/standard/partitions.bin", "offset": 32768 },
        { "path": "parts/standard/boot_app0.bin", "offset": 57344 },
        { "path": "parts/standard/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
"@ | Set-Content -Encoding utf8 (Join-Path $docs 'manifest-standard.json')

@"
{
  "name": "Peanut2Shield longpress",
  "version": "$verShort",
  "new_install_prompt_erase": true,
  "builds": [
    {
      "chipFamily": "ESP32-S3",
      "parts": [
        { "path": "parts/longpress/bootloader.bin", "offset": 0 },
        { "path": "parts/longpress/partitions.bin", "offset": 32768 },
        { "path": "parts/longpress/boot_app0.bin", "offset": 57344 },
        { "path": "parts/longpress/firmware.bin", "offset": 65536 }
      ]
    }
  ]
}
"@ | Set-Content -Encoding utf8 (Join-Path $docs 'manifest-longpress.json')

@"
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="utf-8" />
  <meta name="viewport" content="width=device-width, initial-scale=1" />
  <title>Peanut2Shield flasher</title>
  <script type="module" src="https://unpkg.com/esp-web-tools@10/dist/web/install-button.js?module"></script>
  <style>
    body { font-family: system-ui, sans-serif; max-width: 36rem; margin: 2rem auto; padding: 0 1rem; line-height: 1.45; }
    h1 { font-size: 1.4rem; }
    .card { border: 1px solid #ccc; border-radius: 8px; padding: 1rem 1.1rem; margin: 1rem 0; }
    .note { color: #444; font-size: 0.95rem; }
    a { color: #0b57d0; }
  </style>
</head>
<body>
  <h1>Peanut2Shield flasher</h1>
  <p class="note">Firmware <b>$version</b>. Use <b>Chrome</b> or <b>Edge</b> and a USB-C <b>data</b> cable.
  After install: close this tab, unplug, plug into a <b>wall USB</b> adapter, press <b>RESET</b>.
  Expect yellow, then purple blink. Then pair Shield, then TiVo remote.</p>

  <div class="card">
    <h2>Standard</h2>
    <p>Normal short taps. Use this unless you need TiviMate long-press.</p>
    <esp-web-install-button manifest="./manifest-standard.json"></esp-web-install-button>
  </div>

  <div class="card">
    <h2>Long-press</h2>
    <p>Hold Back / OK for TiviMate.</p>
    <esp-web-install-button manifest="./manifest-longpress.json"></esp-web-install-button>
  </div>

  <p class="note">If Connect fails: hold <b>BOOT</b>, plug in, release, try again.
  If the PC beeps / reconnects: close this page, hold BOOT, install again, then wall power + RESET.
  Project: <a href="https://github.com/Wnt2Fly/Peanut2Shield">github.com/Wnt2Fly/Peanut2Shield</a></p>
</body>
</html>
"@ | Set-Content -Encoding utf8 (Join-Path $docs 'index.html')

# Keep firmware/web as a mirror for local `python -m http.server --directory firmware` users
$web = Join-Path $out 'web'
New-Item -ItemType Directory -Force $web | Out-Null
Copy-Item (Join-Path $docs 'index.html') (Join-Path $web 'index.html') -Force
# Local firmware server root is firmware/, so manifests need ../docs-style paths under parts/
@"
{
  "name": "Peanut2Shield standard",
  "version": "$verShort",
  "new_install_prompt_erase": true,
  "builds": [{
    "chipFamily": "ESP32-S3",
    "parts": [
      { "path": "../parts/standard/bootloader.bin", "offset": 0 },
      { "path": "../parts/standard/partitions.bin", "offset": 32768 },
      { "path": "../parts/standard/boot_app0.bin", "offset": 57344 },
      { "path": "../parts/standard/firmware.bin", "offset": 65536 }
    ]
  }]
}
"@ | Set-Content -Encoding utf8 (Join-Path $web 'manifest-standard.json')
@"
{
  "name": "Peanut2Shield longpress",
  "version": "$verShort",
  "new_install_prompt_erase": true,
  "builds": [{
    "chipFamily": "ESP32-S3",
    "parts": [
      { "path": "../parts/longpress/bootloader.bin", "offset": 0 },
      { "path": "../parts/longpress/partitions.bin", "offset": 32768 },
      { "path": "../parts/longpress/boot_app0.bin", "offset": 57344 },
      { "path": "../parts/longpress/firmware.bin", "offset": 65536 }
    ]
  }]
}
"@ | Set-Content -Encoding utf8 (Join-Path $web 'manifest-longpress.json')

Write-Host "Wrote $docs\ (GitHub Pages) and $web\"
Write-Host "Done. Version $version"
Write-Host "Public URL after enabling Pages on /docs: https://wnt2fly.github.io/Peanut2Shield/"
