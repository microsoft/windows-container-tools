cmake_minimum_required(VERSION 3.15)

project(LogMonitor)

# Find Boost (Boost is installed)
find_package(Boost REQUIRED)

file(GLOB_RECURSE SourceFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}" "*.cpp")

# Define LogMonitor as a library
add_library(LogMonitor ${SourceFiles})

# Include directories for LogMonitor
target_include_directories(LogMonitor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor
	${CMAKE_CURRENT_SOURCE_DIR}/LogMonitor/FileMonitor
    ${Boost_INCLUDE_DIRS}
)

# Link Boost libraries to LogMonitor
target_link_libraries(LogMonitor PRIVATE ${Boost_LIBRARIES})
