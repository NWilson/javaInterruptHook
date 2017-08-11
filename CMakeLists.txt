cmake_minimum_required(VERSION 3.0)
set(CMAKE_CONFIGURATION_TYPES Debug CACHE TYPE INTERNAL FORCE)
project(interrupt)

find_package(JNI REQUIRED)
find_package(Java REQUIRED COMPONENTS Runtime Development)

math(EXPR BITS "${CMAKE_SIZEOF_VOID_P} * 8")
execute_process(COMMAND "${Java_JAVA_EXECUTABLE}" "-d${BITS}" -version
                RESULT_VARIABLE INCORRECT_BITS OUTPUT_QUIET ERROR_QUIET)
if(INCORRECT_BITS)
  message(FATAL_ERROR "You are building using the wrong bitness (32/64-bit) "
                      "for your JDK.  On Windows, perhaps you forgot to run "
                      "with -G\"${CMAKE_GENERATOR} Win64\"?\n"
                      "The CMake compile platform must match the JRE.")
endif()

include_directories(${JNI_INCLUDE_DIRS})
add_library(interrupt SHARED native/javaFns.cxx)

configure_file(java.sh.in java.sh NEWLINE_STYLE UNIX)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
file(MAKE_DIRECTORY "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
add_custom_command(TARGET interrupt POST_BUILD
                   COMMAND "${CMAKE_COMMAND}" -E  copy
                           "$<TARGET_FILE:interrupt>"
                           "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/")

message(STATUS "")
message(STATUS "Now build the DLL by running 'cmake --build .', then")
message(STATUS "test using 'sh ./java.sh'.")
message(STATUS "")
