cmake_minimum_required(VERSION 3.10)
project(mpegts)
set(CMAKE_CXX_STANDARD 17)

#Macro for printing all CMake variables
macro(print_all_variables)
    message(STATUS "print_all_variables------------------------------------------{")
    get_cmake_property(_variableNames VARIABLES)
    foreach (_variableName ${_variableNames})
        message(STATUS "${_variableName}=${${_variableName}}")
    endforeach()
    message(STATUS "print_all_variables------------------------------------------}")
endmacro()

find_package (Threads REQUIRED)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/mpegts/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/)

file(GLOB libfiles ${CMAKE_CURRENT_SOURCE_DIR}/mpegts/*.cpp)
add_library(mpegts STATIC ${libfiles})

add_executable(mpeg_ts_dmx_tests ${CMAKE_CURRENT_SOURCE_DIR}/main_dmx.cpp)
target_link_libraries(mpeg_ts_dmx_tests mpegts Threads::Threads)

add_executable(mpeg_ts_mx_tests ${CMAKE_CURRENT_SOURCE_DIR}/main_mx.cpp)
target_link_libraries(mpeg_ts_mx_tests mpegts Threads::Threads)

file(GLOB unit_tests_sources ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests/*.cpp)
add_executable(mpeg_ts_unit_tests ${unit_tests_sources} ${CMAKE_CURRENT_SOURCE_DIR}/unit_tests.cpp)
target_link_libraries(mpeg_ts_unit_tests mpegts Threads::Threads)