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
echo  1  Normal update     (keeps Shield/TiVo pairing)
echo  2  Full fix           (yellow LED stuck / first-time setup)
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
