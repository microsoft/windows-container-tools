cmake_minimum_required(VERSION 3.15)

# Find Boost (ensure Boost is installed on your system)
find_package(Boost REQUIRED)

# Define the executable
add_executable(LogMonitor
    LogMonitor/Main.cpp
    LogMonitor/EtwMonitor.cpp
    LogMonitor/EtwMonitor.h
    LogMonitor/EventMonitor.cpp
    LogMonitor/EventMonitor.h
    LogMonitor/JsonFileParser.cpp
    LogMonitor/JsonProcessor.cpp
    LogMonitor/JsonProcessor.h
    LogMonitor/LogFileMonitor.cpp
    LogMonitor/LogFileMonitor.h
    LogMonitor/ProcessMonitor.cpp
    LogMonitor/ProcessMonitor.h
    LogMonitor/Utility.cpp
    LogMonitor/Utility.h
    LogMonitor/FileMonitor/FileMonitorUtilities.cpp
    LogMonitor/FileMonitor/FileMonitorUtilities.h
    LogMonitor/pch.cpp
    LogMonitor/pch.h
)

# Include directories
target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}  # Add Boost include directories
)

# Link Boost
target_link_libraries(LogMonitor PRIVATE ${Boost_LIBRARIES})
