@echo off
setlocal
cd /d "%~dp0"

echo.
echo Peanut2Shield - FULL flash (may clear Bluetooth pairings)
echo =========================================================
echo.
echo Only use if flash-update.bat failed or the board never worked.
echo.
pause

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

if not exist "firmware-full\firmware.bin" (
  echo Missing firmware-full\ files. Run pack-usb-drive.bat on the build PC.
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
echo Full flash to %COMPORT% ...
echo.
echo Erasing entire flash first (clears stale Bluetooth/NVS — fixes reboot loop)...
python -m esptool --chip esp32s3 --port %COMPORT% --baud 460800 erase_flash
if errorlevel 1 (
  echo.
  echo ERASE FAILED.
  pause
  exit /b 1
)

echo.
python -m esptool --chip esp32s3 --port %COMPORT% --baud 460800 ^
  write_flash 0x0 firmware-full\bootloader.bin ^
  0x8000 firmware-full\partitions.bin ^
  0xe000 firmware-full\boot_app0.bin ^
  0x10000 firmware-full\firmware.bin

if errorlevel 1 (
  echo.
  echo FLASH FAILED.
  pause
  exit /b 1
)

echo.
echo SUCCESS. Press RESET, then re-pair Shield and TiVo if needed.
echo.
pause
