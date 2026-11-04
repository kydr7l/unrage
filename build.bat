@echo off
rem Build unrage.exe with MinGW-w64.
rem Usage: build [test|clean]
setlocal
cd /d "%~dp0"

if "%~1"=="clean" (
	del /q unrage.exe rpf.o 2>nul
	exit /b 0
)

rem rpf.h is C99 and does not compile as C++, so compile it as C.
gcc -std=c99 -O2 -Wall -Wextra -c rpf.c || exit /b 1
g++ -std=c++17 -O2 -Wall -Wextra -o unrage.exe unrage.cpp rpf.o || exit /b 1

if "%~1"=="test" powershell -NoProfile -ExecutionPolicy Bypass -File test.ps1
exit /b %errorlevel%
