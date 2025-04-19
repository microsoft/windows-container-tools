@echo off
setlocal

REM Set up environment variables
set VCPKG_DIR=%~dp0vcpkg
set BUILD_DIR=%~dp0build
set TOOLCHAIN_FILE=%VCPKG_DIR%\scripts\buildsystems\vcpkg.cmake

REM Step 1: Clone and bootstrap vcpkg if not already done
if not exist "%VCPKG_DIR%\vcpkg.exe" (
    echo === Cloning vcpkg ===
    git clone https://github.com/microsoft/vcpkg.git "%VCPKG_DIR%"
    echo === Bootstrapping vcpkg ===
    pushd "%VCPKG_DIR%"
    .\bootstrap-vcpkg.bat
    popd
)

REM Step 2: Install dependencies
echo === Installing Boost JSON ===
"%VCPKG_DIR%\vcpkg.exe" install boost-json:x64-windows

REM Step 3: Configure CMake
echo === Configuring CMake ===
cmake -S "%~dp0LogMonitor" -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
    -DCMAKE_BUILD_TYPE=Release ^
    -A x64

if errorlevel 1 (
    echo CMake configuration failed.
    exit /b 1
)

REM Step 4: Build the project
echo === Building Project ===
cmake --build "%BUILD_DIR%" --config Release --parallel

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo === Build completed successfully ===
endlocal
