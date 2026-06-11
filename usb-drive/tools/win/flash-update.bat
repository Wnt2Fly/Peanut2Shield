@echo off
setlocal EnableExtensions
set "KIT=%~dp0..\.."
cd /d "%KIT%"

call "%~dp0flasher.bat"
if errorlevel 1 goto :noflasher
if not defined FLASHER_CMD goto :noflasher

echo.
echo Peanut2Shield - firmware UPDATE (pairing usually kept)
echo ======================================================
echo.

if not exist "firmware\update\firmware.bin" (
  echo Missing firmware\update\firmware.bin
  echo Ask for a newer USB stick or run pack\pack.bat on the build PC.
  pause
  exit /b 1
)

echo Available COM ports (if listed below):
if /i "%FLASHER_KIND%"=="espflash" "%FLASHER_CMD%" --skip-update-check list-ports 2>nul
echo.
echo In Device Manager: Ports ^(COM ^& LPT^) - look for USB Serial ^(COMxx^)
echo.

set /p COMPORT=Enter COM port [e.g. COM19]: 
if "%COMPORT%"=="" (
  echo No port entered.
  pause
  exit /b 1
)
if /i not "%COMPORT:~0,3%"=="COM" set "COMPORT=COM%COMPORT%"

echo.
echo Flashing firmware to %COMPORT% ...
echo.

if /i "%FLASHER_KIND%"=="espflash" (
  "%FLASHER_CMD%" --skip-update-check write-bin -c esp32s3 -p %COMPORT% -B 115200 -b usb-reset --non-interactive 0x10000 firmware\update\firmware.bin
) else (
  call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 115200 write_flash 0x10000 firmware\update\firmware.bin
)

if errorlevel 1 goto :failed

echo.
echo SUCCESS. Press the RESET button on the board once, then unplug from PC.
echo LED should blink purple (pair Shield) or go green if already paired.
echo.
pause
exit /b 0

:failed
echo.
echo FLASH FAILED. Try option 2 in UPDATE.bat, another USB cable/port, or see docs\windows.txt
pause
exit /b 1

:noflasher
echo.
echo No flasher found. Run pack\pack.bat on the build PC to bundle espflash.exe.
echo.
pause
exit /b 1
