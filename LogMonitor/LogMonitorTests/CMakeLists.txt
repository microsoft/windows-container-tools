cmake_minimum_required(VERSION 3.15)

project(LogMonitorTests)

find_package(Boost REQUIRED COMPONENTS json)

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
    ${Boost_INCLUDE_DIRS}
)

# Set Windows-specific linker flags
set_target_properties(LogMonitorTests PROPERTIES
    COMPILE_PDB_NAME "LogMonitorTests"
    COMPILE_PDB_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LINK_FLAGS "/DEBUG:FULL /OPT:REF /OPT:ICF"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Link LogMonitor Lib and Boost.JSON to the test shared library
target_link_libraries(LogMonitorTests PRIVATE LogMonitorLib Boost::json)

target_compile_definitions(LogMonitorTests PRIVATE BOOST_ALL_NO_LIB BOOST_JSON_NO_LIB)

# Enable testing
enable_testing()
add_test(NAME LogMonitorTests COMMAND LogMonitorTests)
