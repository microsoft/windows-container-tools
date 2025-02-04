cmake_minimum_required(VERSION 3.15)

# Define the project
project(LogMonitor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use vcpkg if available
if (DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Toolchain file for vcpkg" FORCE)
endif()

# Enable testing framework
enable_testing()

# Enable Unicode globally
add_definitions(-DUNICODE -D_UNICODE)

# Include subdirectories for main and test executables
add_subdirectory(src)  # Add main executable's CMake
add_subdirectory(LogMonitorTests)  # Add test executable's CMake
