cmake_minimum_required(VERSION 3.9)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(serial STATIC serial.cpp serial.h utf8conv.h)
target_include_directories(serial SYSTEM INTERFACE .)
if (NOT WIN32)
	target_link_libraries(serial INTERFACE pthread)
endif()

add_library(bufferedserial STATIC bufferedserial.cpp ringbufferbase.h)
target_link_libraries(bufferedserial PRIVATE serial)

add_subdirectory(test)