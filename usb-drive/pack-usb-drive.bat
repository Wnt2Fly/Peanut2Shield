@echo off
setlocal EnableExtensions
cd /d "%~dp0\.."

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
set "BOOTAPP="

if exist "usb-drive\vendor\boot_app0.bin" set "BOOTAPP=usb-drive\vendor\boot_app0.bin"

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

echo Copying binaries to usb-drive\ ...
if not exist "usb-drive\firmware" mkdir "usb-drive\firmware"
if not exist "usb-drive\firmware-full" mkdir "usb-drive\firmware-full"

copy /y "%BUILD%\firmware.bin" "usb-drive\firmware\firmware.bin" >nul
copy /y "%BUILD%\firmware.bin" "usb-drive\firmware-full\firmware.bin" >nul
copy /y "%BUILD%\bootloader.bin" "usb-drive\firmware-full\bootloader.bin" >nul
copy /y "%BUILD%\partitions.bin" "usb-drive\firmware-full\partitions.bin" >nul

if defined BOOTAPP (
  copy /y "%BOOTAPP%" "usb-drive\firmware-full\boot_app0.bin" >nul
) else (
  echo WARNING: boot_app0.bin not found — flash-full.bat/.sh will not work.
  echo          flash-update still OK.
)

echo Peanut2Shield USB flash kit> "usb-drive\VERSION.txt"
echo Built: %DATE% %TIME%>> "usb-drive\VERSION.txt"
echo Board: waveshare-esp32-s3-zero>> "usb-drive\VERSION.txt"
echo Source: %CD%>> "usb-drive\VERSION.txt"

echo.
echo Done — update kit ready. Copy usb-drive\ to your USB stick.
dir usb-drive\firmware
dir usb-drive\firmware-full
endlocal
exit /b 0
