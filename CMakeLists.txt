cmake_minimum_required(VERSION 3.0)
project(interrupt)

find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})
add_library(interrupt SHARED native/javaFns.cxx)

#find_package(Java REQUIRED COMPONENTS Runtime Development)
#configure_file(java.sh.in java.sh)
