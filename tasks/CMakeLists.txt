# Generated from orogen/lib/orogen/templates/tasks/CMakeLists.txt
FIND_PACKAGE(Boost COMPONENTS thread REQUIRED) 

include(fipa_servicesTaskLib)
ADD_LIBRARY(${FIPA_SERVICES_TASKLIB_NAME} SHARED 
    ${FIPA_SERVICES_TASKLIB_SOURCES})
add_dependencies(${FIPA_SERVICES_TASKLIB_NAME}
    regen-typekit)

TARGET_LINK_LIBRARIES(${FIPA_SERVICES_TASKLIB_NAME}
    ${OrocosRTT_LIBRARIES}
    ${Boost_THREAD_LIBRARY}
    ${FIPA_SERVICES_TASKLIB_DEPENDENT_LIBRARIES})
SET_TARGET_PROPERTIES(${FIPA_SERVICES_TASKLIB_NAME}
    PROPERTIES LINK_INTERFACE_LIBRARIES "${FIPA_SERVICES_TASKLIB_INTERFACE_LIBRARIES}")

INSTALL(TARGETS ${FIPA_SERVICES_TASKLIB_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib/orocos)

INSTALL(FILES ${FIPA_SERVICES_TASKLIB_HEADERS}
    DESTINATION include/orocos/fipa_services)

