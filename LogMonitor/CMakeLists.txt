cmake_minimum_required(VERSION 3.15)

# Define the project
project(LogMonitor)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME ON)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Release)
set(VCPKG_TARGET_TRIPLET x64-windows-static)

# Use vcpkg if available
if(DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file" FORCE)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg target triplet")
endif()

# Enforce static MSVC runtime (/MT or /MTd)
if(MSVC)
    foreach(flag_var CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG
                     CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
        string(REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endforeach()
endif()

# Set Windows SDK version if available
if (DEFINED ENV{SDKVersion})
    set(CMAKE_SYSTEM_VERSION $ENV{SDKVersion})
endif()

# Enable Unicode globally
add_definitions(-DUNICODE -D_UNICODE)

# Enable warnings
if (MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

# Enable testing framework
enable_testing()

# Find dependencies
find_package(nlohmann_json CONFIG REQUIRED)

# Include subdirectories for main and test executables
add_subdirectory(src)  # Add main executable's CMake
add_subdirectory(LogMonitorTests)  # Add test executable's CMake
