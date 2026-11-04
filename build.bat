@echo off
rem Build unrage.exe with MSVC.
rem Usage: build [test|clean]
setlocal
cd /d "%~dp0"

if "%~1"=="clean" (
	del /q unrage.exe unrage.res *.obj 2>nul
	exit /b 0
)

rem Outside a Developer Command Prompt, find Visual Studio and load its environment.
where cl >nul 2>nul || call :vcvars || exit /b 1

rem rpf.h is C and does not compile as C++, so compile it as C.
cl /nologo /std:c11 /O2 /W4 /c rpf.c || exit /b 1
rc /nologo unrage.rc || exit /b 1
cl /nologo /std:c++17 /O2 /W4 /EHsc /Feunrage.exe unrage.cpp rpf.obj unrage.res advapi32.lib || exit /b 1

if "%~1"=="test" powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1
exit /b %errorlevel%

:vcvars
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "%VSWHERE%" for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSDIR=%%i"
if not defined VSDIR (
	echo build: cannot find MSVC. Install Visual Studio with the C++ tools.
	exit /b 1
)
rem vcvars64.bat runs vswhere.exe by name, so put it on the PATH.
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VSDIR%\VC\Auxiliary\Build\vcvars64.bat" >nul
exit /b %errorlevel%
