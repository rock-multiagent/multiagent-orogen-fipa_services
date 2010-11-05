cmake_minimum_required(VERSION 2.6)

SET(OROCOS_INSTALL /home/neo/Arbeit/Projekte/Rimres/software/Framework/install/tools)
SET(OROCOS_TARGET gnulinux)
SET(PROJ_SOURCE_DIR ${CMAKE_SOURCE_DIR})
SET(SOURCES client_socket_connection.cpp corba_connection.cpp)

include(FindPkgConfig RESULT_VARIABLE PKG_CONFIG_FOUND)
pkg_check_modules(ASTRIUM_PROXY REQUIRED "astrium-proxy")
#pkg_check_modules(MODULES-TRANSPORT-CORBA REQUIRED "modules-transport-corba-${OROCOS_TARGET}")
pkg_check_modules(OROCOS_CORBA REQUIRED "orocos-rtt-corba-${OROCOS_TARGET}")

message(CMAKE_SOURCE_DIR ${CMAKE_SOURCE_DIR})

include_directories ( 
    /home/neo/Arbeit/Projekte/Rimres/software/Framework/modules/root-module/tasks
#	${MODULES-TASKS_INCLUDE_DIRS}
	${OROCOS_CORBA_INCLUDE_DIRS}
    ${ASTRIUM_PROXY_INCLUDE_DIRS}
	)

link_directories ( 
#	${MODULES-TASKS_LIBRARY_DIRS}
	${OROCOS_CORBA_LIBRARY_DIRS}
    ${ASTRIUM_PROXY_LIBRARY_DIRS}
)

#SET (CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")
set(CMAKE_MODULE_PATH "/home/neo/Arbeit/Projekte/Rimres/software/Framework/modules/root-module/.orogen/config;${CMAKE_SOURCE_DIR}/cmake")
#include(CommonSetup)

find_package(OrocosRTT REQUIRED) # Needed for definitions.
#find_package(SIGC++ REQUIRED)

include_directories (
#    ${SIGC++_INCLUDES}
)

add_definitions(${OROCOS_RTT_DEFINES})

add_library(connections SHARED ${SOURCES})
target_link_libraries(connections ${ASTRIUM_PROXY_LIBRARIES})

#add_executable(${PROJECT_NAME} ${SOURCES})

#set( LIBRARIES
#    ${ASTRIUM_PROXY_LIBRARIES}
#    ${MODULES-TASKS_LIBRARIES}
#    ${MODULES-TRANSPORT-CORBA_LIBRARIES}
#)

#target_link_libraries(${PROJECT_NAME} ${LIBRARIES})

#install(TARGETS ${PROJECT_NAME} 
#    RUNTIME DESTINATION bin
#    LIBRARY DESTINATION lib
#)




