@echo off
setlocal enabledelayedexpansion

echo Setting up environment for MSVC...

:: 1. Try using vswhere (The most reliable way)
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
        echo Found VS at: !VS_PATH!
        call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x86
        goto :compile
    )
)

:: 2. Fallback to common paths if vswhere failed
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x86
    goto :compile
)

:: ... (other fallbacks)
if exist "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" (
    call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86
    goto :compile
)

echo ERROR: Could not find vcvarsall.bat. Please ensure Visual Studio or Build Tools are installed.
exit /b 1

:compile
echo Compiling tests...
cl /EHsc /Ifancontrol tests\logic_test.cpp fancontrol\ECManager.cpp fancontrol\SensorManager.cpp fancontrol\FanController.cpp fancontrol\ConfigManager.cpp /link /out:logic_test.exe

if %ERRORLEVEL% EQU 0 (
    echo Running tests...
    logic_test.exe
) else (
    echo Compilation failed.
)
pause
