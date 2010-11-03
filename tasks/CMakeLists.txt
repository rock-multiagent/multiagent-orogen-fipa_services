include(rootTaskLib)

SET(HEADERS
    module_id.h
    connections/connection_interface.h
    connections/client_socket_connection.h
    connections/corba_connection.h
    messages/message_interface.h
    messages/fipa_message.h
)

SET(SOURCES 
    module_id.cpp
    messages/message_interface.cpp
    messages/fipa_message.cpp
    connections/corba_connection.cpp
)

SET(ROOT_TASKLIB_HEADERS ${ROOT_TASKLIB_HEADERS} ${HEADERS})
SET(ROOT_TASKLIB_SOURCES ${ROOT_TASKLIB_SOURCES} ${SOURCES})

ADD_LIBRARY(${ROOT_TASKLIB_NAME} SHARED ${ROOT_TASKLIB_SOURCES})

ADD_DEPENDENCIES(${ROOT_TASKLIB_NAME} regen-toolkit)

TARGET_LINK_LIBRARIES(${ROOT_TASKLIB_NAME}
    ${OrocosRTT_LIBRARIES}
    ${ROOT_TASKLIB_DEPENDENT_LIBRARIES}
)

SET_TARGET_PROPERTIES(${ROOT_TASKLIB_NAME}
    PROPERTIES LINK_INTERFACE_LIBRARIES "${ROOT_TASKLIB_INTERFACE_LIBRARIES}")

INSTALL(TARGETS ${ROOT_TASKLIB_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/orocos)

INSTALL(FILES ${ROOT_TASKLIB_HEADERS}
    DESTINATION include/orocos/root)


