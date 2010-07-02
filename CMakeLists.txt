PROJECT(modules)
cmake_minimum_required(VERSION 2.6)

SET (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")

include(FindPkgConfig RESULT_VARIABLE PKG_CONFIG_FOUND)
pkg_check_modules(SERVICE_DISCOVERY REQUIRED "service-discovery")

include_directories(${SERVICE_DISCOVERY_INCLUDE_DIRS})
link_directories(${SERVICE_DISCOVERY_LIBRARY_DIRS})

# using the scripts in cmake
find_package(AVAHI REQUIRED)
find_package(SIGC++ REQUIRED)
include_directories( ${AVAHI_INCLUDES} ${SIGC++_INCLUDES})


INCLUDE(modulesBase)

# FIND_PACKAGE(KDL)
# FIND_PACKAGE(OCL)

