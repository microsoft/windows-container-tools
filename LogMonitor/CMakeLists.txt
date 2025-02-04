cmake_minimum_required(VERSION 3.15)
project(LogMonitor)

# Use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add src and tests directories
add_subdirectory(src)
add_subdirectory(LogMonitorTests)
