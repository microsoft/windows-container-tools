cmake_minimum_required(VERSION 3.15)

project(LogMonitor)

# Find Boost (Boost is installed)
find_package(Boost REQUIRED)

# Define LogMonitor as a library instead of an executable
add_library(LogMonitor
    LogMonitor/Main.cpp
    LogMonitor/EtwMonitor.cpp
    LogMonitor/EventMonitor.cpp
    LogMonitor/JsonFileParser.cpp
    LogMonitor/JsonProcessor.cpp
    LogMonitor/LogFileMonitor.cpp
    LogMonitor/ProcessMonitor.cpp
    LogMonitor/Utility.cpp
    LogMonitor/FileMonitor/FileMonitorUtilities.cpp
    LogMonitor/pch.cpp
)

# Include directories for LogMonitor
target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
	${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}
)

# Link Boost libraries to LogMonitor
target_link_libraries(LogMonitor PRIVATE ${Boost_LIBRARIES})
