@echo off
setlocal enabledelayedexpansion

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x86
    msbuild fancontrol\fancontrol.vcxproj /p:Configuration=Release /p:Platform=x86 /t:Rebuild
) else (
    echo VS not found
    exit /b 1
)
