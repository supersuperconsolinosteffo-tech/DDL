@echo off
title UtilityMod Native DLL Builder for Minecraft 1.21.11
cd /d "%~dp0"

echo ============================================================================
echo UtilityMod Native DLL Builder - Minecraft 1.21.11
echo ============================================================================

REM ============================================================================
REM Step 1: Ensure MinHook source files exist
REM ============================================================================
echo.
echo [1/4] Downloading MinHook source files...

mkdir utilitymod\src\main\cpp\native\minhook 2>nul

REM Download individual .c files from MinHook GitHub repository
echo   Downloading buffer.c...
curl -L -s -o utilitymod\src\main\cpp\native\minhook\buffer.c ^
   "https://raw.githubusercontent.com/TsudaK/minhook/v2.8.2/src/buffer.c" || (
   echo ERROR: Failed to download buffer.c
   pause
   exit /b 1
)

echo   Downloading hook.c...
curl -L -s -o utilitymod\src\main\cpp\native\minhook\hook.c ^
   "https://raw.githubusercontent.com/TsudaK/minhook/v2.8.2/src/hook.c" || (
   echo ERROR: Failed to download hook.c
   pause
   exit /b 1
)

echo   Downloading trampoline.c...
curl -L -s -o utilitymod\src\main\cpp\native\minhook\trampoline.c ^
   "https://raw.githubusercontent.com/TsudaK/minhook/v2.8.2/src/trampoline.c" || (
   echo ERROR: Failed to download trampoline.c
   pause
   exit /b 1
)

echo.
echo [2/4] Verifying MinHook files...

if not exist utilitymod\src\main\cpp\native\minhook\buffer.c echo ERROR: buffer.c missing! & pause & exit /b 1
if not exist utilitymod\src\main\cpp\native\minhook\hook.c echo ERROR: hook.c missing! & pause & exit /b 1
if not exist utilitymod\src\main\cpp\native\minhook\trampoline.c echo ERROR: trampoline.c missing! & pause & exit /b 1

echo   All MinHook source files verified successfully.

REM ============================================================================
REM Step 2: CMake configuration and build
REM ============================================================================
echo.
echo [3/4] Running CMake configuration...

mkdir utilitymod\src\main\cpp\native\build 2>nul
cd utilitymod\src\main\cpp\native\build

cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release 2>&1

if %errorlevel% neq 0 (
   echo.
   echo CMake configuration FAILED!
   echo.
   echo Make sure:
   echo   - Visual Studio 2022 is installed with Desktop development with C++ workload
   echo   - CMake 3.20+ is available
   echo   - You are running this from the project root directory
   pause
   exit /b 1
)

echo.
echo [4/4] Building native DLL...

cmake --build . --config Release 2>&1

if %errorlevel% neq 0 (
   echo.
   echo Build FAILED!
   echo.
   echo Check the output above for errors.
   pause
   exit /b 1
)

echo.
echo ============================================================================
echo BUILD SUCCESSFUL!
echo ============================================================================

echo.
echo "Output: utilitymod\src\main\cpp\native\build\Release\utilitymod_native.dll"

echo.
echo "Installing DLL to: %APPDATA%\.utilitymod\native"

mkdir "%APPDATA%\.utilitymod\native" 2>nul
copy utilitymod\src\main\cpp\native\build\Release\utilitymod_native.dll "%APPDATA%\.utilitymod\native"

echo.
echo DLL installed successfully!
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