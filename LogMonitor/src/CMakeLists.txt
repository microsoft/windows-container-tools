cmake_minimum_required(VERSION 3.15)

project(LogMonitor)

find_package(Boost REQUIRED COMPONENTS json)

# Gather source files
file(GLOB_RECURSE SourceFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp")
file(GLOB_RECURSE HeaderFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.h")

# Define LogMonitorLib as a static library
add_library(LogMonitorLib STATIC ${SourceFiles})

# Set the output name of the static library to "LogMonitor.lib"
set_target_properties(LogMonitorLib PROPERTIES
    OUTPUT_NAME "LogMonitor"
)

# Define LogMonitor as an executable
add_executable(LogMonitor ${SourceFiles})

# Add precompiled headers (PCH)
target_precompile_headers(LogMonitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/pch.h)

# Include directories for both targets
target_include_directories(LogMonitorLib PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}
)

target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}
)

# Link Boost JSON to LogMonitor and LogMonitorLib
target_link_libraries(LogMonitor PRIVATE Boost::json)
target_link_libraries(LogMonitorLib PRIVATE Boost::json)

# Link the executable with the static library
target_link_libraries(LogMonitor PRIVATE LogMonitorLib)

# Specify the output name for the executable
set_target_properties(LogMonitor PROPERTIES
    OUTPUT_NAME "LogMonitor"
)

# Specify output directories
set_target_properties(LogMonitor PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)