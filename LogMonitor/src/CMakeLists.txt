cmake_minimum_required(VERSION 3.15)

project(LogMonitor)

find_package(Boost REQUIRED COMPONENTS json)

file(GLOB_RECURSE SourceFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp")

# Define LogMonitor as a library
add_library(LogMonitor ${SourceFiles})

# Add precompiled headers (PCH)
target_precompile_headers(LogMonitor PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/pch.h)

# Include directories for LogMonitor
target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
	${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}
)

# Link Boost JSON to LogMonitor
target_link_libraries(LogMonitor PRIVATE Boost::json)
