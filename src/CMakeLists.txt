
include_directories(BEFORE .)

add_library(zeroflows SHARED 
        zenv.c
        zservice.c
        zsock.c
        zconfig.c
        zutils.c
        zreactor.c
        zeroflows.h
        macros.h)

set_target_properties(zeroflows PROPERTIES
        PUBLIC_HEADER "zeroflows.h"
        SOVERSION ${ABI_VERSION})

target_link_libraries(zeroflows
        ${ZMQ_LIBRARIES}
        ${JANSSON_LIBRARIES}
        ${ZK_LIBRARIES}
        ${GLIB2_LIBRARIES})

install(TARGETS zeroflows
        LIBRARY DESTINATION ${LD_LIBDIR}
        PUBLIC_HEADER DESTINATION include)

