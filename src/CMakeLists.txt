
include_directories(BEFORE .)

add_library(zsock SHARED 
        zservice.c zsock.c zsock_config.c zutils.c zsock.h
        zreactor.c zreactor.h
        macros.h)

set_target_properties(zsock PROPERTIES SOVERSION ${ABI_VERSION})

target_link_libraries(zsock
        ${ZMQ_LIBRARIES}
        ${JANSSON_LIBRARIES}
        ${ZK_LIBRARIES}
        ${GLIB2_LIBRARIES})

install(TARGETS zsock
        LIBRARY DESTINATION ${LD_LIBDIR})

