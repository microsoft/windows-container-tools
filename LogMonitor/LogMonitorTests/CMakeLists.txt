cmake_minimum_required(VERSION 3.15)

# Find Boost
find_package(Boost REQUIRED)

# Define test executable
add_executable(LogMonitorTests
    ConfigFileParserTests.cpp
    EtwMonitorTests.cpp
    EventMonitorTests.cpp
    LogFileMonitorTests.cpp
    LogMonitorTests.cpp
    UtilityTests.cpp
    Utility.cpp
    Utility.h
    pch.cpp
    pch.h
)

# Include directories
target_include_directories(LogMonitorTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../src/LogMonitor
    ${Boost_INCLUDE_DIRS}
)

# Link with LogMonitor and Boost
target_link_libraries(LogMonitorTests PRIVATE LogMonitor ${Boost_LIBRARIES})

# Enable CTest
enable_testing()
add_test(NAME LogMonitorTests COMMAND LogMonitorTests)
