@echo off
setlocal EnableExtensions
set "KIT=%~dp0..\.."
cd /d "%KIT%"

call "%~dp0flasher.bat"
if errorlevel 1 goto :noflasher

echo.
echo Peanut2Shield - FULL flash (fixes yellow LED / new boards)
echo ==========================================================
echo.
echo Erases the chip first, then writes a complete image.
echo You will likely need to re-pair Shield and TiVo remote afterward.
echo.
pause

if not exist "firmware\full\firmware.bin" (
  echo Missing firmware\full\ files. Run pack\pack.bat on the build PC.
  pause
  exit /b 1
)

echo Available COM ports (if listed below):
if /i "%FLASHER_KIND%"=="espflash" "%FLASHER_CMD%" --skip-update-check list-all-ports 2>nul
echo.

set /p COMPORT=Enter COM port [e.g. COM19]: 
if "%COMPORT%"=="" (
  echo No port entered.
  pause
  exit /b 1
)

echo.
echo Erasing flash on %COMPORT% ...
echo.

if /i "%FLASHER_KIND%"=="espflash" (
  "%FLASHER_CMD%" --skip-update-check erase-flash -c esp32s3 -p %COMPORT% -B 460800
  if errorlevel 1 goto :failed
  echo.
  echo Writing full image...
  "%FLASHER_CMD%" --skip-update-check write-bin -c esp32s3 -p %COMPORT% -B 460800 -a no-reset 0x0 firmware\full\bootloader.bin
  if errorlevel 1 goto :failed
  "%FLASHER_CMD%" --skip-update-check write-bin -c esp32s3 -p %COMPORT% -B 460800 -a no-reset 0x8000 firmware\full\partitions.bin
  if errorlevel 1 goto :failed
  "%FLASHER_CMD%" --skip-update-check write-bin -c esp32s3 -p %COMPORT% -B 460800 -a no-reset 0xe000 firmware\full\boot_app0.bin
  if errorlevel 1 goto :failed
  "%FLASHER_CMD%" --skip-update-check write-bin -c esp32s3 -p %COMPORT% -B 460800 0x10000 firmware\full\firmware.bin
) else (
  call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 460800 erase_flash
  if errorlevel 1 goto :failed
  echo.
  call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 460800 write_flash 0x0 firmware\full\bootloader.bin 0x8000 firmware\full\partitions.bin 0xe000 firmware\full\boot_app0.bin 0x10000 firmware\full\firmware.bin
)

if errorlevel 1 goto :failed

echo.
echo SUCCESS. Press RESET, then pair Shield and TiVo if needed.
echo.
pause
exit /b 0

:failed
echo.
echo FLASH FAILED.
pause
exit /b 1

:noflasher
echo.
echo No flasher found. Run pack\pack.bat on the build PC first.
pause
exit /b 1
