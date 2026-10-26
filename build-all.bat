@echo off
rem One-click build: native DLL (MSVC) + Fabric bridge mod (Gradle, needs JDK 21 + internet).
rem Outputs land in dist\ : UtilityClient.dll, UtilityInjector.exe, utilityclient-bridge-*.jar
setlocal
cd /d "%~dp0"

call native\build.bat
if errorlevel 1 exit /b 1

cd bridge-mod
call gradlew.bat build
if errorlevel 1 (
  echo [UtilityClient] Bridge build failed. Needs JDK 21 on PATH/JAVA_HOME + internet
  echo                  ^(first build downloads Gradle, Loom, Minecraft and Fabric API^).
  exit /b 1
)
cd ..
copy /y bridge-mod\build\libs\utilityclient-bridge-*.jar dist\ >nul 2>&1
echo [UtilityClient] All artifacts in dist\
dir dist
