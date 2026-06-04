@echo off
setlocal
cd /d "%~dp0"

echo.
echo Peanut2Shield - firmware UPDATE (pairing usually kept)
echo ======================================================
echo.

where python >nul 2>&1
if errorlevel 1 (
  echo Python not found. Install from https://www.python.org/downloads/
  echo Enable "Add python to PATH", then run:  pip install esptool
  pause
  exit /b 1
)

python -m pip show esptool >nul 2>&1
if errorlevel 1 (
  echo Installing esptool...
  python -m pip install esptool
)

if not exist "firmware\firmware.bin" (
  echo Missing firmware\firmware.bin
  echo Run pack-usb-drive.bat on the build PC first.
  pause
  exit /b 1
)

set /p COMPORT=Enter COM port [e.g. COM19]: 
if "%COMPORT%"=="" (
  echo No port entered.
  pause
  exit /b 1
)

echo.
echo Flashing firmware.bin to %COMPORT% ...
echo.

python -m esptool --chip esp32s3 --port %COMPORT% --baud 460800 ^
  write_flash 0x10000 firmware\firmware.bin

if errorlevel 1 (
  echo.
  echo FLASH FAILED. Try another USB port/cable or use flash-full.bat
  pause
  exit /b 1
)

echo.
echo SUCCESS. Press the RESET button on the board once, then unplug from PC.
echo.
pause
