cmake_minimum_required(VERSION 3.9)

option(BUILD_TESTS "requires Catch2 header" ON)

if (BUILD_TESTS)

include_directories(..)
add_library(testmain main.cpp)

add_executable(ringbuffertest ringbuffertest.cpp)
target_link_libraries(ringbuffertest PRIVATE testmain)

endif()
