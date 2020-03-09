cmake_minimum_required(VERSION 3.13)
project(tgvoipcall)

set(CMAKE_CXX_STANDARD 17)

option(BUILD_CALL "Build tgvoipcall util" OFF)
option(BUILD_RATE "Build tgvoiprate util" OFF)
option(BUILD_REFLECTOR "Build reflector util" OFF)
option(STATIC_TGVOIP "Link libtgvoip statically" OFF)


# look for shared libraries in current directory
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wl,--disable-new-dtags,-rpath,$ORIGIN/")

find_package(Threads REQUIRED)
link_libraries(${CMAKE_THREAD_LIBS_INIT})

set(AVIO_LIBS "")
if (BUILD_CALL OR BUILD_RATE)
    find_library(AVIO_CODEC_LIB avcodec)
    find_library(AVIO_UTIL_LIB avutil)
    find_library(AVIO_FORMAT_LIB avformat)
    find_library(AVIO_RESAMPLE_LIB avresample)

    set(AVIO_LIBS "${AVIO_RESAMPLE_LIB};${AVIO_CODEC_LIB};${AVIO_UTIL_LIB};${AVIO_FORMAT_LIB}")
endif ()

if (BUILD_CALL)
    add_executable(tgvoipcall src/main_call.cpp)

    if (STATIC_TGVOIP)
        find_library(DL_LIB dl)
        find_library(OPUS_LIB opus)
        find_library(CRY_LIB crypto)
        set(TGVOIP_LIB "libtgvoip.a;${OPUS_LIB};${DL_LIB};${CRY_LIB}")

        if (DEFINED TGVOIP_PATH)
            target_link_directories(tgvoipcall PRIVATE ${TGVOIP_PATH})
        endif ()
    else ()
        find_library(TGVOIP_LIB tgvoip)
    endif ()

    target_link_libraries(tgvoipcall PRIVATE "${TGVOIP_LIB}" "${AVIO_LIBS}")
    target_compile_options(tgvoipcall PRIVATE "-Wall;-Wextra")
endif ()

if (BUILD_RATE)
    add_executable(tgvoiprate src/main_rate.cpp src/rating/dsp.cpp src/rating/preprocessing.cpp src/rating/measure.cpp src/rating/model.cpp)
    target_link_libraries(tgvoiprate PRIVATE "${AVIO_LIBS}")
    target_compile_options(tgvoiprate PRIVATE "-Wall;-Wextra")
endif ()
