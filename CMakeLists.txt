cmake_minimum_required(VERSION 3.0)
set(CMAKE_CONFIGURATION_TYPES Debug CACHE TYPE INTERNAL FORCE)
project(interrupt)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})
add_library(interrupt SHARED native/javaFns.cxx)

find_package(Java REQUIRED COMPONENTS Runtime Development)
configure_file(java.sh.in java.sh NEWLINE_STYLE UNIX)

message(STATUS "")
message(STATUS "Now build the DLL by running 'make' or using")
message(STATUS "Visual Studio, then test using 'sh ./java.sh'.")
message(STATUS "")
