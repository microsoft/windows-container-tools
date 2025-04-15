cmake_minimum_required(VERSION 3.15)

# Define the project
project(LogMonitor)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_BUILD_TYPE Release)
set(VCPKG_TARGET_TRIPLET x64-windows)

# Use vcpkg if available
if (DEFINED ENV{VCPKG_ROOT})
    set(VCPKG_ROOT $ENV{VCPKG_ROOT})
    set(CMAKE_TOOLCHAIN_FILE "${VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Toolchain file for vcpkg" FORCE)
	
	# Set Boost paths
    set(BOOST_ROOT "${VCPKG_ROOT}/installed/x64-windows" CACHE PATH "Boost installation root")
    set(Boost_INCLUDE_DIR "${VCPKG_ROOT}/installed/x64-windows/include" CACHE PATH "Boost include directory")
endif()

# Set Windows SDK version if available
if (DEFINED ENV{SDKVersion})
    set(CMAKE_SYSTEM_VERSION $ENV{SDKVersion})
endif()

# Enable testing framework
enable_testing()

# Enable Unicode globally
add_definitions(-DUNICODE -D_UNICODE)

# Include subdirectories for main and test executables
add_subdirectory(src)  # Add main executable's CMake
add_subdirectory(LogMonitorTests)  # Add test executable's CMake
