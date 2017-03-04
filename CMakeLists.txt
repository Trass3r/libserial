cmake_minimum_required(VERSION 3.0)

add_library(serial STATIC serial.cpp serial.h utf8conv.h)
target_include_directories(serial SYSTEM INTERFACE .)
if (NOT WIN32)
	target_link_libraries(serial INTERFACE pthread)
endif()
