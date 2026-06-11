@echo off
setlocal EnableExtensions
rem Sets FLASHER_CMD and FLASHER_KIND in the caller. Prefer bundled espflash.exe.

set "FCMD="
set "FKIND="

set "BUNDLE=%~dp0espflash.exe"
if exist "%BUNDLE%" (
  set "FCMD=%BUNDLE%"
  set "FKIND=espflash"
  goto :export
)

where python >nul 2>&1
if errorlevel 1 (
  endlocal
  exit /b 1
)

python -m pip show esptool >nul 2>&1
if errorlevel 1 (
  echo Installing esptool (one-time, needs internet)...
  python -m pip install esptool
)

set "FCMD=python -m esptool"
set "FKIND=esptool"

:export
endlocal & set "FLASHER_CMD=%FCMD%" & set "FLASHER_KIND=%FKIND%"
exit /b 0
