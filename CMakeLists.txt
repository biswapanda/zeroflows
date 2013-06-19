
cmake_minimum_required (VERSION 2.6)
project(Zero-Flows C)



####################################
### Environment and dependencies ###
####################################

include(CheckTypeSize)
include(CheckIncludeFile)
include(CheckLibraryExists)
find_package(PkgConfig)

pkg_check_modules(GLIB2 REQUIRED glib-2.0 gthread-2.0 gmodule-2.0)
pkg_check_modules(JANSSON REQUIRED jansson)
pkg_check_modules(ZMQ REQUIRED libzmq)

CHECK_TYPE_SIZE(long SIZEOF_LONG)
if(SIZEOF_LONG EQUAL 8)
    set (LD_LIBDIR "lib64")
else(SIZEOF_LONG EQUAL 8)
    set (LD_LIBDIR "lib")
endif(SIZEOF_LONG EQUAL 8)

macro(test_FOUND var msg)
    if (${var})
        message(STATUS "FOUND ${msg} : ${${var}}")
    else (${var})
        message(FATAL_ERROR "NOT FOUND ${msg}")
    endif (${var})
    unset(${var} CACHE)
endmacro(test_FOUND)

set(ZK_INCLUDE_DIRS "/usr/include/zookeeper")
set(ZK_LIBRARY_DIRS "/usr/${LD_LIBDIR}")
if (ZK_INCDIR)
    set(ZK_INCLUDE_DIRS ${ZK_INCDIR})
endif(ZK_INCDIR)
if(ZK_LIBDIR)
    set(ZK_LIBRARY_DIRS ${ZK_LIBDIR})
endif(ZK_LIBDIR)
find_file(FOUND zookeeper/zookeeper.h ${ZK_INCLUDE_DIRS})
test_FOUND(FOUND "ZooKeeper header not found")
find_library(ZK_LIBRARIES libzookeeper_st.so ${ZK_LIBRARY_DIRS})



##########################
### Build instructions ###
##########################

set(ABI_VERSION 0)

set(CMAKE_C_FLAGS "-pipe -Wall -Wextra -std=c99")

if (CMAKE_COMPILER_IS_GNUCC)
    MESSAGE("GCC ...")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-variadic-macros")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wsequence-point")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wredundant-decls")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wshadow")
    if(CMAKE_C_COMPILER_VERSION MATCHES ".*4\\.[6789].*")
        MESSAGE("GCC >= 4.6")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wcomment")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmain")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wparentheses")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wfloat-equal")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunsafe-loop-optimizations")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wmissing-prototypes")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-but-set-parameter")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wunused-but-set-variable")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wframe-larger-than=1024")
        set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wswitch-enum")
    endif()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -finline-limit=1024")
endif()

set(CMAKE_C_FLAGS_DEBUG          "-g -O0 -fno-inline")
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g -O2 -fno-inline")
set(CMAKE_C_FLAGS_RELEASE        "-O2")
set(CMAKE_C_FLAGS_MINSIZEREL     "-Os -s")

if (NOT CMAKE_BUILD_TYPE STREQUAL "MinSizeRel")
    add_definitions(-DHAVE_ASSERT=1)
endif()

add_definitions(-D_XOPEN_SOURCE=700)
add_definitions(-D_REENTRANT)
add_definitions(-D_LARGE_FILES)
add_definitions(-D_LARGEFILE_SOURCE)
add_definitions(-D_LARGEFILE64_SOURCE)
add_definitions(-D_FILE_OFFSET_BITS=64)
add_definitions(-DG_ERRORCHECK_MUTEXES)
add_definitions(-DG_DISABLE_DEPRECATED=1)

include_directories(AFTER
        ${ZMQ_INCLUDE_DIRS}
        ${ZK_INCLUDE_DIRS}/zookeeper
        ${GLIB2_INCLUDE_DIRS})

link_directories(
        ${ZMQ_LIBRARY_DIRS}
        ${ZK_LIBRARY_DIRS})

add_subdirectory(./src)
add_subdirectory(./examples)

