@echo off
setlocal

REM Set the path to the LogMonitor folder
set PROJECT_DIR=%~dp0LogMonitor

REM Set up environment variables
set VCPKG_DIR=%~dp0vcpkg
set BUILD_DIR=%~dp0build
set TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake
set BUILD_TYPE=Release
set BUILD_ARCH=x64
set VCPKG_TRIPLET=%BUILD_ARCH%-windows-static

REM Step 1: Clone and bootstrap vcpkg if not already done
if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo === Cloning vcpkg ===
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
    echo === Bootstrapping vcpkg ===
    pushd "%VCPKG_DIR%"
    .\bootstrap-vcpkg.bat
    popd
)

REM Step 2: Install nlohmann-json dependencies
echo === Installing nlohmann dependencies ===
"%VCPKG_DIR%\vcpkg.exe" install nlohmann-json:%VCPKG_TRIPLET%

if errorlevel 1 (
    echo Failed to install nlohmann-json dependencies.
    exit /b 1
)

REM Create the build directory if it doesn't exist
if not exist "%PROJECT_DIR%\build" (
    mkdir "%PROJECT_DIR%\build"
)

REM Navigate into the build directory
cd /d "%PROJECT_DIR%\build"

REM Run CMake configuration with toolchain and triplet
cmake .. ^
 -DCMAKE_TOOLCHAIN_FILE=%TOOLCHAIN_FILE% ^
 -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
 -DVCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET% ^
 -A %BUILD_ARCH%
 
if errorlevel 1 (
    echo CMake configuration failed.
    exit /b 1
)

REM Build the project
cmake --build . --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo === Build completed successfully ===

endlocal
