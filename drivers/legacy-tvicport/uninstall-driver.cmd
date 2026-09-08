@echo off
setlocal
echo ========================================================
echo  TPFanCtrl2 - Legacy TVicPort Driver Uninstaller
echo ========================================================
echo.

:: Check for administrative privileges
net session >nul 2>&1
if %errorlevel% neq 0 (
    echo [ERROR] Administrator permissions are required!
    echo Please right-click this script and select 'Run as administrator'.
    echo.
    pause
    exit /b 1
)

echo [INFO] Stopping and removing TVicPort64 service...
sc stop TVicPort64 >nul 2>&1
sc delete TVicPort64 >nul 2>&1

echo [INFO] Stopping and removing TVicPort service...
sc stop TVicPort >nul 2>&1
sc delete TVicPort >nul 2>&1

echo [INFO] Removing driver files...
del /f /q "%SystemRoot%\system32\drivers\TVicPort64.sys" >nul 2>&1
del /f /q "%SystemRoot%\system32\drivers\TVicPort.sys" >nul 2>&1
del /f /q "%SystemRoot%\SysWOW64\TVicPort.dll" >nul 2>&1
del /f /q "%SystemRoot%\system32\TVicPort.dll" >nul 2>&1

echo.
echo [SUCCESS] TVicPort driver service and files removed successfully.
echo.
pause