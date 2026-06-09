@echo off
setlocal EnableExtensions
rem Sets FLASHER_CMD and FLASHER_KIND. Prefer bundled espflash.exe.

set "FLASHER_CMD="
set "FLASHER_KIND="

set "BUNDLE=%~dp0espflash.exe"
if exist "%BUNDLE%" (
  set "FLASHER_CMD=%BUNDLE%"
  set "FLASHER_KIND=espflash"
  exit /b 0
)

where python >nul 2>&1
if errorlevel 1 exit /b 1

python -m pip show esptool >nul 2>&1
if errorlevel 1 (
  echo Installing esptool (one-time, needs internet)...
  python -m pip install esptool
)

set "FLASHER_CMD=python -m esptool"
set "FLASHER_KIND=esptool"
exit /b 0
