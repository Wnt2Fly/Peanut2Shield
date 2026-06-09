@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "DEST=%~dp0..\tools\win"
set "EXE=%DEST%\espflash.exe"
set "VER=v4.4.0"
set "URL=https://github.com/esp-rs/espflash/releases/download/%VER%/espflash-x86_64-pc-windows-msvc.zip"
set "ZIP=%DEST%\espflash-dl.zip"
set "TMP=%DEST%\espflash-dl"

if exist "%EXE%" (
  echo Bundled flasher OK: %EXE%
  exit /b 0
)

echo Downloading espflash %VER% for Windows...
if not exist "%DEST%" mkdir "%DEST%"

powershell -NoProfile -Command ^
  "$ErrorActionPreference='Stop';" ^
  "Invoke-WebRequest -Uri '%URL%' -OutFile '%ZIP%';" ^
  "Expand-Archive -Path '%ZIP%' -DestinationPath '%TMP%' -Force;" ^
  "Move-Item -Force (Join-Path '%TMP%' 'espflash.exe') '%EXE%';" ^
  "Remove-Item -Recurse -Force '%TMP%','%ZIP%'"

if not exist "%EXE%" (
  echo FAILED to download espflash.exe
  exit /b 1
)

echo Bundled flasher ready: %EXE%
exit /b 0
