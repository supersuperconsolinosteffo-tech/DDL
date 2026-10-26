@echo off
title UtilityMod Native DLL Builder for Minecraft 1.21.11
cd /d "%~dp0"

echo ============================================================================
echo UtilityMod Native DLL Builder - Minecraft 1.21.11
echo ============================================================================

set NATIVE=utilitymod\src\main\cpp\native
set OUTDIR=%NATIVE%\build\bin\Release

REM ============================================================================
REM Step 1: Sanity-check dependencies
REM ============================================================================
echo.
echo [1/3] Checking dependencies...

if not exist "%NATIVE%\CMakeLists.txt" (
   echo ERROR: CMakeLists.txt not found. Run this from the project root.
   pause
   exit /b 1
)

if not exist "%NATIVE%\minhook\src\hook.c" (
   echo ERROR: MinHook sources missing from %NATIVE%\minhook\src
   echo        MinHook is vendored in the repository - restore the checkout.
   pause
   exit /b 1
)

if not exist "%NATIVE%\imgui\imgui.cpp" (
   echo ERROR: ImGui sources missing from %NATIVE%\imgui
   echo        ImGui is vendored in the repository - restore the checkout.
   pause
   exit /b 1
)

REM JNI headers (jni.h / jvmti.h) come from a JDK, not from this repository.
if not defined JAVA_HOME (
   echo WARNING: JAVA_HOME is not set.
   echo          JNI headers are required. Install JDK 17+ and set JAVA_HOME,
   echo          or pass -DUTILITYMOD_JNI_INCLUDE_DIR=^<jdk^>\include to CMake.
) else (
   echo   JAVA_HOME=%JAVA_HOME%
)

where cmake >nul 2>&1
if %errorlevel% neq 0 (
   echo ERROR: cmake not found in PATH - install CMake 3.20+.
   pause
   exit /b 1
)
echo   Dependencies OK.

REM ============================================================================
REM Step 2: Configure
REM ============================================================================
echo.
echo [2/3] Running CMake configuration...

if not exist "%NATIVE%\build" mkdir "%NATIVE%\build"
pushd "%NATIVE%\build"

cmake .. -G "Visual Studio 17 2022" -A x64 2>&1
if %errorlevel% neq 0 (
   echo.
   echo CMake configuration FAILED!
   echo.
   echo Make sure:
   echo   - Visual Studio 2022 is installed with "Desktop development with C++"
   echo   - CMake 3.20+ is available
   echo   - JAVA_HOME points at a JDK 17+ installation
   pause
   popd
   exit /b 1
)

REM ============================================================================
REM Step 3: Build and install
REM ============================================================================
echo.
echo [3/3] Building native DLL...

cmake --build . --config Release 2>&1
if %errorlevel% neq 0 (
   echo.
   echo Build FAILED! Check the output above for errors.
   pause
   popd
   exit /b 1
)
popd

if not exist "%OUTDIR%\utilitymod_native.dll" (
   echo.
   echo ERROR: Build reported success but %OUTDIR%\utilitymod_native.dll is missing.
   pause
   exit /b 1
)

echo.
echo ============================================================================
echo BUILD SUCCESSFUL!
echo ============================================================================
echo Output: %OUTDIR%\utilitymod_native.dll

echo.
echo Installing DLL to: %APPDATA%\.utilitymod\native
mkdir "%APPDATA%\.utilitymod\native" 2>nul
copy /y "%OUTDIR%\utilitymod_native.dll" "%APPDATA%\.utilitymod\native"

echo.
echo ============================================================================
echo Next Steps:
echo ============================================================================
echo 1. Launch Minecraft 1.21.11 with Fabric installed
echo 2. Press INSERT key to toggle native ImGui menu
echo 3. Press O key to toggle Java dropdown menu
echo 4. Use utility modules (AutoWalk, AutoEat, AutoSprint, FreeCam, NoClip)
echo ============================================================================

pause
