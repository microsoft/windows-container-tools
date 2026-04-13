cmake_minimum_required(VERSION 3.15)

project(LogMonitorTests)

find_package(nlohmann_json CONFIG REQUIRED)

# Automatically gather all test source files
file(GLOB_RECURSE TEST_SOURCES RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp")

# Ensure all source files exist before proceeding
if(NOT TEST_SOURCES)
    message(FATAL_ERROR "No valid source files found for LogMonitorTests.")
endif()

# Define test shared library (DLL)
add_library(LogMonitorTests SHARED ${TEST_SOURCES})

# Add a definition for symbol exporting
target_compile_definitions(LogMonitorTests PRIVATE LOGMONITORTESTS_EXPORTS)

# Include directories (for headers)
target_include_directories(LogMonitorTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}  # Includes Utility.h and pch.h
    ${CMAKE_CURRENT_SOURCE_DIR}/../src
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/LogMonitor
)

# Set Windows-specific linker flags
# Use $<CONFIG> so binaries land in Release/ or Debug/, matching the pipeline's $(buildConfiguration) layout.
set_target_properties(LogMonitorTests PROPERTIES
    COMPILE_PDB_NAME "LogMonitorTests"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
    LINK_FLAGS "/DEBUG:FULL /OPT:REF /OPT:ICF"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>"
)

# Link LogMonitor and Nlohmann JSON
target_link_libraries(LogMonitorTests PRIVATE LogMonitorLib nlohmann_json::nlohmann_json)

# LogMonitorTests is a shared library (DLL) for VSTest, not a standalone executable.
# Run tests with: vstest.console.exe <build>/Release/LogMonitorTests.dll
# (as used by the Azure pipeline)
