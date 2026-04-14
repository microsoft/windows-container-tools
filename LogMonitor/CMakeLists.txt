cmake_minimum_required(VERSION 3.15)

# Set vcpkg toolchain before project() so CMake's compiler detection uses it.
# Callers may also pass -DCMAKE_TOOLCHAIN_FILE on the command line (takes precedence).
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "Vcpkg toolchain file")
endif()

set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "Vcpkg target triplet")

# Define the project
project(LogMonitor)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
