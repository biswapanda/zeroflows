
include_directories(BEFORE
        ../src)

add_library(main_utils STATIC common.c common.h ../src/zeroflows.h)

target_link_libraries(main_utils zeroflows
        ${GLIB2_LIBRARIES})

add_executable(zservice main_service.c)
target_link_libraries(zservice zeroflows main_utils)

add_executable(zpush
        main_pipe_push.c
        common_pipe_out.h
        common_pipe_out.c)
target_link_libraries(zpush zeroflows main_utils)

add_executable(zpub
        main_pipe_pub.c
        common_pipe_out.h
        common_pipe_out.c)
target_link_libraries(zpub zeroflows main_utils)

install(TARGETS zservice zpub zpush
        RUNTIME DESTINATION bin)

