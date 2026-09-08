@echo off
setlocal
echo ========================================================
echo  TPFanCtrl2 - Legacy TVicPort Driver Installer
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

set IS_64BIT=0
if /i "%PROCESSOR_ARCHITECTURE%"=="AMD64" set IS_64BIT=1
if defined PROCESSOR_ARCHITEW6432 set IS_64BIT=1

if %IS_64BIT%==1 (
    echo [INFO] Detected 64-bit Windows environment.
    echo [INFO] Installing TVicPort64 driver...
    
    copy /y "%~dp0TVicPort64.sys" "%SystemRoot%\system32\drivers\TVicPort64.sys" >nul
    copy /y "%~dp0TVicPort.dll" "%SystemRoot%\SysWOW64\TVicPort.dll" >nul 2>&1
    copy /y "%~dp0TVicPort.dll" "%SystemRoot%\system32\TVicPort.dll" >nul 2>&1

    sc query TVicPort64 >nul 2>&1
    if %errorlevel% equ 0 (
        echo [INFO] Service TVicPort64 already exists. Updating configuration...
        sc config TVicPort64 binPath= "%SystemRoot%\system32\drivers\TVicPort64.sys" start= auto >nul
    ) else (
        sc create TVicPort64 binPath= "%SystemRoot%\system32\drivers\TVicPort64.sys" type= kernel start= auto error= normal group= "Extended Base" displayname= "TVicPort64" >nul
    )
    
    echo [INFO] Starting TVicPort64 service...
    sc start TVicPort64 >nul 2>&1
) else (
    echo [INFO] Detected 32-bit Windows environment.
    echo [INFO] Installing TVicPort driver...
    
    copy /y "%~dp0TVicPort.sys" "%SystemRoot%\system32\drivers\TVicPort.sys" >nul
    copy /y "%~dp0TVicPort.dll" "%SystemRoot%\system32\TVicPort.dll" >nul 2>&1

    sc query TVicPort >nul 2>&1
    if %errorlevel% equ 0 (
        echo [INFO] Service TVicPort already exists. Updating configuration...
        sc config TVicPort binPath= "%SystemRoot%\system32\drivers\TVicPort.sys" start= auto >nul
    ) else (
        sc create TVicPort binPath= "%SystemRoot%\system32\drivers\TVicPort.sys" type= kernel start= auto error= normal group= "Extended Base" displayname= "TVicPort" >nul
    )
    
    echo [INFO] Starting TVicPort service...
    sc start TVicPort >nul 2>&1
)

echo.
echo [SUCCESS] TVicPort driver installation and registration complete.
echo Note: On Windows 10/11 with Memory Integrity (HVCI) enabled, 
echo       TVicPort may be blocked by Windows Defender. 
echo       For modern Windows 11, PawnIO driver is strongly recommended instead.
echo.
pause