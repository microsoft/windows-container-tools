@echo off
setlocal

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

REM Step 2: Install static Boost dependencies
echo === Installing static Boost dependencies ===
"%VCPKG_DIR%\vcpkg.exe" install boost-json:%VCPKG_TRIPLET% ^
                               boost-algorithm:%VCPKG_TRIPLET%

if errorlevel 1 (
    echo Failed to install Boost dependencies.
    exit /b 1
)

REM Step 3: Configure CMake
echo === Configuring CMake ===
cmake -S "%~dp0LogMonitor" -B "%BUILD_DIR%" ^
    -DCMAKE_TOOLCHAIN_FILE="%TOOLCHAIN_FILE%" ^
    -DCMAKE_BUILD_TYPE=%BUILD_TYPE% ^
    -DVCPKG_TARGET_TRIPLET=%VCPKG_TRIPLET% ^
    -A %BUILD_ARCH%

if errorlevel 1 (
    echo CMake configuration failed.
    exit /b 1
)

REM Step 4: Build the project
echo === Building Project ===
cmake --build "%BUILD_DIR%" --config %BUILD_TYPE% --parallel

if errorlevel 1 (
    echo Build failed.
    exit /b 1
)

echo === Build completed successfully ===
endlocal
