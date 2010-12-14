PROJECT(root)
cmake_minimum_required(VERSION 2.6)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")
include_directories(${CMAKE_SOURCE_DIR}/tasks)
include_directories(${CMAKE_SOURCE_DIR}/.orogen/tasks)
 
INCLUDE(rootBase)

# FIND_PACKAGE(KDL)
# FIND_PACKAGE(OCL)

