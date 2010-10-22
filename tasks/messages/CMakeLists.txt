cmake_minimum_required(VERSION 2.6)

set(MESSAGES_SOURCES message_interface.cpp fipa_message.cpp)

# FIPA ACL
include(FindPkgConfig RESULT_VARIABLE PKG_CONFIG_FOUND)
pkg_check_modules(MESSAGE_PARSER REQUIRED "message-parser")
pkg_check_modules(MESSAGE_GENERATOR REQUIRED "message-generator")
include_directories(
    ${MESSAGE_PARSER_INCLUDE_DIRS}
    ${MESSAGE_GENERATOR_INCLUDE_DIRS}
)
link_directories(
    ${MESSAGE_PARSER_LIBRARY_DIRS}
    ${MESSAGE_GENERATOR_LIBRARY_DIRS}
)

add_library(module_messages SHARED ${MESSAGES_SOURCES})
target_link_libraries(
    module_messages
    ${MESSAGE_PARSER_LIBRARIES} # contains the message-generator library.
)

# test program
message(CMAKE_BINARY_DIR ${CMAKE_BINARY_DIR})
include_directories(${CMAKE_SOURCE_DIR})
link_directories(${CMAKE_BINARY_DIR})
add_executable(test test.cpp)
target_link_libraries(test module_messages)
