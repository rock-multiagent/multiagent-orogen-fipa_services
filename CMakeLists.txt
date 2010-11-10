PROJECT(root)
cmake_minimum_required(VERSION 2.6)

SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")

set(Boost_LOCALFIRST TRUE)
set(Boost_ADDITIONAL_VERSION 1.42 1.43)
find_package(Boost 1.42 REQUIRED)

include_directories (
    ${CMAKE_SOURCE_DIR}/tasks
    ${Boost_INCLUDE_DIRS}
)

INCLUDE(rootBase)

# FIND_PACKAGE(KDL)
# FIND_PACKAGE(OCL)
