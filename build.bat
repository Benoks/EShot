@echo off
setlocal enabledelayedexpansion

REM ============================================================================
REM  EShot local build script
REM
REM  Mirrors .github/workflows/build.yml (CMake + Visual Studio 17 2022 + Qt6),
REM  producing a self-contained dist\bin tree with the Qt runtime deployed.
REM
REM  Usage:
REM     build.bat                 Build x64 Release (default)
REM     build.bat arm64           Build ARM64 Release
REM     build.bat x64 Debug       Build x64 Debug
REM     build.bat clean           Remove build\ and dist\ then exit
REM
REM  Qt location:
REM     The CI gets Qt from install-qt-action. Locally, point the script at your
REM     Qt install by setting QT_DIR before running, e.g.:
REM         set QT_DIR=C:\Qt\6.8.2\msvc2022_64
REM         build.bat
REM     If QT_DIR is unset the script probes common C:\Qt locations.
REM ============================================================================

set "SCRIPT_DIR=%~dp0"
set "BUILD_DIR=%SCRIPT_DIR%build"
set "DIST_DIR=%SCRIPT_DIR%dist"

REM ---- argument parsing ------------------------------------------------------
if /I "%~1"=="clean" (
    echo Removing "%BUILD_DIR%" and "%DIST_DIR%" ...
    if exist "%BUILD_DIR%" rmdir /s /q "%BUILD_DIR%"
    if exist "%DIST_DIR%"  rmdir /s /q "%DIST_DIR%"
    echo Done.
    exit /b 0
)

set "ARCH=%~1"
if "%ARCH%"=="" set "ARCH=x64"
set "CONFIG=%~2"
if "%CONFIG%"=="" set "CONFIG=Release"

REM Map our arch token to the CMake -A value used by the VS generator.
if /I "%ARCH%"=="x64"   set "CMAKE_ARCH=x64"
if /I "%ARCH%"=="arm64" set "CMAKE_ARCH=ARM64"
if not defined CMAKE_ARCH (
    echo ERROR: unknown arch "%ARCH%". Use x64 or arm64.
    exit /b 1
)

echo ============================================================
echo  EShot build : arch=%ARCH%  config=%CONFIG%
echo ============================================================

REM ---- locate Qt -------------------------------------------------------------
REM CMAKE_PREFIX_PATH is how CMake's find_package(Qt6) locates Qt. Prefer an
REM explicit QT_DIR; otherwise probe a couple of standard install paths.
if not defined QT_DIR (
    if /I "%ARCH%"=="x64"   set "QT_GUESS=C:\Qt\6.8.2\msvc2022_64"
    if /I "%ARCH%"=="arm64" set "QT_GUESS=C:\Qt\6.8.2\msvc2022_arm64"
    REM Validate on Qt6Core.dll, not windeployqt.exe: the cross-compiled arm64
    REM kit has no windeployqt (it's a host-only tool in the companion x64 kit).
    if exist "!QT_GUESS!\bin\Qt6Core.dll" set "QT_DIR=!QT_GUESS!"
)

if not defined QT_DIR (
    echo ERROR: Qt not found.
    echo   Set QT_DIR to your Qt kit, e.g.:
    echo     set QT_DIR=C:\Qt\6.8.2\msvc2022_64
    exit /b 1
)
if not exist "%QT_DIR%\bin\Qt6Core.dll" (
    echo ERROR: "%QT_DIR%" does not look like a Qt kit ^(no bin\Qt6Core.dll^).
    exit /b 1
)
echo Using Qt: %QT_DIR%

REM ---- locate host Qt (cross-compiled kits only) -----------------------------
REM A cross-compiled Qt (the arm64 kit) requires QT_HOST_PATH pointing at the
REM companion host x64 Qt that owns the build tools. install-qt-action sets this
REM in CI via --autodesktop; locally we derive it from the sibling msvc2022_64
REM kit. Native kits (x64) have windeployqt in-kit and need no host path.
set "HOST_ARG="
if not exist "%QT_DIR%\bin\windeployqt.exe" (
    for %%i in ("%QT_DIR%") do set "VER_ROOT=%%~dpi"
    if exist "!VER_ROOT!msvc2022_64\bin\Qt6Core.dll" (
        set "QT_HOST_PATH=!VER_ROOT!msvc2022_64"
        set "HOST_ARG=-DQT_HOST_PATH=!VER_ROOT!msvc2022_64"
        echo Using host Qt: !VER_ROOT!msvc2022_64
    ) else (
        echo ERROR: cross-compiled kit needs a host x64 Qt, none found at "!VER_ROOT!msvc2022_64".
        echo   Reinstall with --autodesktop:
        echo     aqt install-qt windows desktop 6.8.2 win64_msvc2022_arm64_cross_compiled --autodesktop -O C:\Qt
        goto :fail
    )
)

REM ---- configure -------------------------------------------------------------
echo.
echo [1/4] Configuring...
cmake -S "%SCRIPT_DIR%." -B "%BUILD_DIR%" ^
    -G "Visual Studio 17 2022" ^
    -A %CMAKE_ARCH% ^
    -DCMAKE_BUILD_TYPE=%CONFIG% ^
    -DCMAKE_PREFIX_PATH="%QT_DIR%" ^
    !HOST_ARG!
if errorlevel 1 goto :fail

REM ---- build -----------------------------------------------------------------
echo.
echo [2/4] Building...
cmake --build "%BUILD_DIR%" --config %CONFIG% --parallel
if errorlevel 1 goto :fail

REM ---- install ---------------------------------------------------------------
echo.
echo [3/4] Installing to "%DIST_DIR%"...
cmake --install "%BUILD_DIR%" --config %CONFIG% --prefix "%DIST_DIR%"
if errorlevel 1 goto :fail

REM ---- deploy Qt runtime -----------------------------------------------------
REM Bundle the Qt DLLs/plugins next to the exe so dist\bin is self-contained,
REM matching the CI's "Deploy Qt runtime" step.
echo.
echo [4/4] Deploying Qt runtime...
set "EXE=%DIST_DIR%\bin\EShot.exe"
if not exist "%EXE%" (
    echo ERROR: expected "%EXE%" after install but it is missing.
    goto :fail
)

set "WDQ=%QT_DIR%\bin\windeployqt.exe"
if exist "%WDQ%" (
    REM Native kit (x64): windeployqt ships in the kit, deploy directly.
    echo Using windeployqt: %WDQ%
    "%WDQ%" --release --no-translations "%EXE%"
    if errorlevel 1 goto :fail
) else (
    REM Cross-compiled kit (arm64): windeployqt is a host-only x64 tool in the
    REM companion desktop kit. Find it under the Qt version root and point it at
    REM this target kit via qtpaths6.bat so it deploys arm64 DLLs (see build.yml).
    set "WDQ="
    for %%i in ("%QT_DIR%") do set "VER_ROOT=%%~dpi"
    for /f "delims=" %%f in ('dir /b /s "!VER_ROOT!windeployqt.exe" 2^>nul') do (
        if not defined WDQ set "WDQ=%%f"
    )
    if not defined WDQ (
        echo ERROR: windeployqt.exe not found under "!VER_ROOT!".
        goto :fail
    )
    if not exist "%QT_DIR%\bin\qtpaths6.bat" (
        echo ERROR: "%QT_DIR%\bin\qtpaths6.bat" not found for cross-deploy.
        goto :fail
    )
    echo Using host windeployqt: !WDQ!
    "!WDQ!" --release --no-translations --qtpaths "%QT_DIR%\bin\qtpaths6.bat" "%EXE%"
    if errorlevel 1 goto :fail
)

echo.
echo ============================================================
echo  Build complete: %DIST_DIR%\bin\EShot.exe
echo ============================================================
endlocal
exit /b 0

:fail
echo.
echo *** BUILD FAILED (see output above) ***
endlocal
exit /b 1
