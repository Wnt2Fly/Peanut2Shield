@echo off
setlocal EnableExtensions
cd /d "%~dp0..\.."

rem Capture home before "call pio" — some terminals clear USERPROFILE afterward.
set "PIO_HOME=%USERPROFILE%"
if not defined PIO_HOME set "PIO_HOME=%HOMEDRIVE%%HOMEPATH%"
if not defined PIO_HOME (
  for /f "usebackq delims=" %%H in (`powershell -NoProfile -Command "Write-Output $env:USERPROFILE"`) do set "PIO_HOME=%%H"
)

echo Building Peanut2Shield...
call pio run -e waveshare-esp32-s3-zero
if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)

set "BUILD=.pio\build\waveshare-esp32-s3-zero"
set "KIT=usb-drive"
set "BOOTAPP="

if exist "%KIT%\pack\vendor\boot_app0.bin" set "BOOTAPP=%KIT%\pack\vendor\boot_app0.bin"
if not defined BOOTAPP if exist "%KIT%\vendor\boot_app0.bin" set "BOOTAPP=%KIT%\vendor\boot_app0.bin"

if not defined BOOTAPP if defined PIO_HOME (
  set "PIO=%PIO_HOME%\.platformio"
  if exist "%PIO%\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" (
    set "BOOTAPP=%PIO%\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin"
  )
  if not defined BOOTAPP if exist "%PIO%\packages\" (
    for /d %%D in ("%PIO%\packages\framework-arduinoespressif32*") do (
      if exist "%%D\tools\partitions\boot_app0.bin" set "BOOTAPP=%%D\tools\partitions\boot_app0.bin"
    )
  )
)

if not exist "%BUILD%\firmware.bin" (
  echo Missing %BUILD%\firmware.bin
  pause
  exit /b 1
)

echo Copying binaries to %KIT%\firmware\ ...
if not exist "%KIT%\firmware\update" mkdir "%KIT%\firmware\update"
if not exist "%KIT%\firmware\full" mkdir "%KIT%\firmware\full"

set "FW_VER="
for /f "tokens=3" %%V in ('findstr /C:"CFG_FIRMWARE_VERSION" src\config.h') do set "FW_VER=%%~V"

copy /y "%BUILD%\firmware.bin" "%KIT%\firmware\update\firmware.bin" >nul
copy /y "%BUILD%\firmware.bin" "%KIT%\firmware\full\firmware.bin" >nul
copy /y "%BUILD%\bootloader.bin" "%KIT%\firmware\full\bootloader.bin" >nul
copy /y "%BUILD%\partitions.bin" "%KIT%\firmware\full\partitions.bin" >nul

if defined BOOTAPP (
  copy /y "%BOOTAPP%" "%KIT%\firmware\full\boot_app0.bin" >nul
  copy /y "%BOOTAPP%" "%KIT%\pack\vendor\boot_app0.bin" >nul
) else (
  echo WARNING: boot_app0.bin not found — full flash will not work.
  echo          update flash is still OK.
)

echo Peanut2Shield USB update kit> "%KIT%\VERSION.txt"
if defined FW_VER echo Firmware: %FW_VER%>> "%KIT%\VERSION.txt"
echo Built: %DATE% %TIME%>> "%KIT%\VERSION.txt"
echo Board: waveshare-esp32-s3-zero>> "%KIT%\VERSION.txt"
echo Source: %CD%>> "%KIT%\VERSION.txt"

echo.
echo Bundling Windows flasher (espflash.exe)...
call "%KIT%\pack\bundle-flasher.bat"
if errorlevel 1 (
  echo WARNING: Could not bundle espflash.exe
)

echo.
echo Done — copy %KIT%\ to your USB stick.
dir "%KIT%\firmware\update"
dir "%KIT%\firmware\full"
endlocal
exit /b 0
