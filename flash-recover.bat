@echo off
setlocal EnableExtensions
cd /d "%~dp0"

set "PORT=%~1"
if "%PORT%"=="" (
  set /p PORT=Enter COM port [e.g. COM20]: 
)
if "%PORT%"=="" (
  echo No port given.
  exit /b 1
)

echo.
echo Peanut2Shield recover flash on %PORT%
echo ===================================
echo 1. Close any serial monitor on this port first.
echo 2. Full erase + upload (clears bad NVS / crash loop).
echo.

pio run -t erase -e waveshare-esp32-s3-zero --upload-port %PORT%
if errorlevel 1 exit /b 1

pio run -t upload -e waveshare-esp32-s3-zero --upload-port %PORT%
if errorlevel 1 exit /b 1

echo.
echo Done. Press RESET on the board, then:
echo   pio device monitor -p %PORT% -b 115200
echo.
echo Expect: [BOOT] PSRAM=2048 KB  then  [HID] Peripheral ready
echo If PSRAM=0 or red blink: wrong board chip (needs 2 MB PSRAM).
echo.
endlocal
