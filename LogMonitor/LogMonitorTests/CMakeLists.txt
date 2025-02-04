cmake_minimum_required(VERSION 3.15)

project(LogMonitorTests)

find_package(Boost REQUIRED)

set(TEST_SOURCES
    JsonProcessorTests.cpp
    EtwMonitorTests.cpp
    EventMonitorTests.cpp
    LogFileMonitorTests.cpp
    LogMonitorTests.cpp
    UtilityTests.cpp
    Utility.cpp
    pch.cpp
    ConfigFileParserTests.cpp
)

# Ensure all source files exist before proceeding
foreach(FILE ${TEST_SOURCES})
    if (NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}")
        message(WARNING "File not found: ${FILE}")
        list(REMOVE_ITEM TEST_SOURCES ${FILE})
    endif()
endforeach()

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

# Link with LogMonitor and Boost
target_link_libraries(LogMonitorTests PRIVATE LogMonitor ${Boost_LIBRARIES})

# Enable testing
enable_testing()

add_test(NAME LogMonitorTests COMMAND LogMonitorTests)
