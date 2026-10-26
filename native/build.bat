@echo off
rem Builds UtilityClient.dll + UtilityInjector.exe with Visual Studio 2022 (x64)
rem and copies them to ..\dist. Run from anywhere; paths are relative to this file.
setlocal
cd /d "%~dp0"

where cmake >nul 2>&1
if errorlevel 1 (
  echo [UtilityClient] CMake not found on PATH. Install CMake or open a
  echo                  "x64 Native Tools Command Prompt for VS 2022".
  exit /b 1
)

if not exist build mkdir build
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
if errorlevel 1 goto :fail
cmake --build build --config Release
if errorlevel 1 goto :fail

if not exist ..\dist mkdir ..\dist
copy /y build\Release\UtilityClient.dll ..\dist\ >nul
copy /y build\Release\UtilityInjector.exe ..\dist\ >nul
echo [UtilityClient] Done: ..\dist\UtilityClient.dll and ..\dist\UtilityInjector.exe
exit /b 0

:fail
echo [UtilityClient] Build failed. You need Visual Studio 2022 with the
echo                  "Desktop development with C++" workload (MSVC x64).
exit /b 1
