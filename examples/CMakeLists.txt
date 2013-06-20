
include_directories(BEFORE
        ../src)

add_library(main_utils STATIC
        common_pipe_out.c common_pipe_out.h
        common_pipe_in.c common_pipe_in.h
        common.c common.h
        ../src/zeroflows.h)

target_link_libraries(main_utils zeroflows
        ${GLIB2_LIBRARIES})

add_executable(zservice main_service.c)
target_link_libraries(zservice zeroflows main_utils)

add_executable(zpush main_pipe_push.c)
target_link_libraries(zpush zeroflows main_utils)

add_executable(zpull main_pipe_pull.c)
target_link_libraries(zpull zeroflows main_utils)

add_executable(zpub main_pipe_pub.c)
target_link_libraries(zpub zeroflows main_utils)

add_executable(zsub main_pipe_sub.c)
target_link_libraries(zsub zeroflows main_utils)

install(TARGETS zservice zpub zsub zpush zpull
        RUNTIME DESTINATION bin)

