cmake_minimum_required(VERSION 3.15)

project(LogMonitor)

find_package(nlohmann_json CONFIG REQUIRED)

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
)

target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
)

# Link dependencies
target_link_libraries(LogMonitorLib PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(LogMonitor PRIVATE LogMonitorLib nlohmann_json::nlohmann_json)

# Set output path
set_target_properties(LogMonitor PROPERTIES
	OUTPUT_NAME "LogMonitor"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
