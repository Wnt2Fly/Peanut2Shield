@echo off
setlocal EnableExtensions
set "KIT=%~dp0..\.."
cd /d "%KIT%"

call "%~dp0flasher.bat"
if errorlevel 1 goto :noflasher
if not defined FLASHER_CMD goto :noflasher

echo.
echo Peanut2Shield - FULL flash (fixes yellow LED / new boards)
echo ==========================================================
echo.
echo Erases the chip first, then writes a complete image.
echo You will likely need to re-pair Shield and TiVo remote afterward.
echo.
type VERSION.txt 2>nul
echo.
pause

if not exist "firmware\full\combined.bin" (
  if not exist "firmware\full\firmware.bin" (
    echo Missing firmware\full\ files. Run pack\pack.bat on the build PC.
    pause
    exit /b 1
  )
  echo NOTE: combined.bin not found — using multi-step flash.
  set "USE_COMBINED=0"
) else (
  set "USE_COMBINED=1"
)

echo Available COM ports (if listed below):
if /i "%FLASHER_KIND%"=="espflash" "%FLASHER_CMD%" --skip-update-check list-ports 2>nul
echo.
echo If no port is listed: hold BOOT, plug in USB, release BOOT after 2 sec.
echo Or check Device Manager — the COM number may have changed.
echo.

set /p COMPORT=Enter COM port [e.g. COM19]: 
if "%COMPORT%"=="" (
  echo No port entered.
  pause
  exit /b 1
)
if /i not "%COMPORT:~0,3%"=="COM" set "COMPORT=COM%COMPORT%"

echo.
echo Erasing flash on %COMPORT% ...
echo.

if /i "%FLASHER_KIND%"=="espflash" (
  set "EF=--skip-update-check -c esp32s3 -p %COMPORT% -B 115200 --non-interactive"
  "%FLASHER_CMD%" erase-flash %EF% -b usb-reset
  if errorlevel 1 goto :failed
  echo.
  echo Writing full image...
  if "%USE_COMBINED%"=="1" (
    "%FLASHER_CMD%" write-bin %EF% -b usb-reset 0x0 firmware\full\combined.bin
  ) else (
    "%FLASHER_CMD%" write-bin %EF% -b usb-reset -a no-reset-no-stub 0x0 firmware\full\bootloader.bin
    if errorlevel 1 goto :failed
    "%FLASHER_CMD%" write-bin %EF% -b no-reset -a no-reset-no-stub 0x8000 firmware\full\partitions.bin
    if errorlevel 1 goto :failed
    "%FLASHER_CMD%" write-bin %EF% -b no-reset -a no-reset-no-stub 0xe000 firmware\full\boot_app0.bin
    if errorlevel 1 goto :failed
    "%FLASHER_CMD%" write-bin %EF% -b no-reset 0x10000 firmware\full\firmware.bin
  )
) else (
  call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 115200 erase_flash
  if errorlevel 1 goto :failed
  echo.
  if "%USE_COMBINED%"=="1" (
    call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 115200 write_flash 0x0 firmware\full\combined.bin
  ) else (
    call %FLASHER_CMD% --chip esp32s3 --port %COMPORT% --baud 115200 write_flash 0x0 firmware\full\bootloader.bin 0x8000 firmware\full\partitions.bin 0xe000 firmware\full\boot_app0.bin 0x10000 firmware\full\firmware.bin
  )
)

if errorlevel 1 goto :failed

echo.
echo SUCCESS. Press RESET, then pair Shield and TiVo if needed.
echo Power from a WALL USB adapter — not the Nvidia Shield USB port.
echo (Charge-only cable OK for Shield power only; data cable hangs the board.)
echo LED should BLINK PURPLE — not solid yellow or solid purple.
echo.
pause
exit /b 0

:failed
echo.
echo FLASH FAILED. Press RESET on the board, wait 5 sec, and try again.
echo If it keeps failing, try another USB port or cable.
pause
exit /b 1

:noflasher
echo.
echo No flasher found. Run pack\pack.bat on the build PC first.
pause
exit /b 1
