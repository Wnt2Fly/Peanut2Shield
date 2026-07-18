@echo off
setlocal
cd /d "%~dp0"

echo.
echo  ============================================
echo   Peanut2Shield firmware update
echo   No Python required - everything is included
echo  ============================================
echo.
type VERSION.txt 2>nul
echo.

if not exist "firmware\update\firmware.bin" (
  echo  ERROR: No firmware on this stick.
  echo  On the build PC run:  pack-usb-drive.bat
  echo  Then copy the whole usb-drive folder to this USB stick again.
  echo.
  pause
  exit /b 1
)

if not exist "tools\win\espflash.exe" (
  echo  ERROR: Missing tools\win\espflash.exe
  echo  On the build PC run:  pack-usb-drive.bat
  echo.
  pause
  exit /b 1
)

echo  1  Normal update     (remote was working — keeps pairing)
echo  2  Full fix           (YELLOW LED stuck / new board / update failed)
echo.
echo  If the LED is yellow or the remote stopped working, use 2 not 1.
echo  After any flash: press RESET, then WALL USB power (not Shield USB).
echo  Solid purple = hung on USB serial — use wall adapter or charge-only cable.
echo  Q  Quit
echo.

choice /c 12Q /n /m "Pick 1, 2, or Q: "
if errorlevel 3 exit /b 0
if errorlevel 2 (
  call "%~dp0tools\win\flash-full.bat"
  exit /b %errorlevel%
)
call "%~dp0tools\win\flash-update.bat"
exit /b %errorlevel%
