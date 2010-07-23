PROJECT(modules)
cmake_minimum_required(VERSION 2.6)

SET (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")

include(FindPkgConfig RESULT_VARIABLE PKG_CONFIG_FOUND)
pkg_check_modules(MESSAGE_GENERATOR REQUIRED "message-generator")
pkg_check_modules(MESSAGE_PARSER REQUIRED "message-parser")
pkg_check_modules(SERVICE_DISCOVERY REQUIRED "service-discovery")

include_directories(
    ${MESSAGE_GENERATOR_INCLUDE_DIRS}
	${MESSAGE_PARSER_INCLUDE_DIRS} 
    ${SERVICE_DISCOVERY_INCLUDE_DIRS}
)

link_directories(
    ${MESSAGE_PARSER_LIBRARY_DIRS}
    ${SERVICE_DISCOVERY_LIBRARY_DIRS}
)

# using the scripts in cmake
find_package(SIGC++ REQUIRED)

include_directories ( 
    ${SIGC++_INCLUDES}
)

INCLUDE(modulesBase)


