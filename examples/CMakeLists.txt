
include_directories(BEFORE
        ../src)

add_library(main_utils STATIC
        common.c common.h
        ../src/zsock.h ../src/zreactor.h ../src/macros.h)

target_link_libraries(main_utils zsock
        ${GLIB2_LIBRARIES})

add_executable(zservice main_service.c)
target_link_libraries(zservice zsock main_utils)

add_executable(zpipe main_pipe.c)
target_link_libraries(zpipe zsock main_utils)

install(TARGETS zservice zpipe
        RUNTIME DESTINATION bin)

