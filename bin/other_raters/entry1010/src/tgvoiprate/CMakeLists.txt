cmake_minimum_required(VERSION 3.0.0)
project(tgvoiprate VERSION 0.1.0)

include(CTest)
enable_testing()

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/external_libs/opusfile)

add_executable(tgvoiprate
    main.cpp
    fft.cpp
    utils.cpp
    cmd_args.cpp
    opus_file_reader.cpp
)

target_include_directories(tgvoiprate PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/external_libs/opusfile/include
)

target_link_libraries(tgvoiprate
    opusfile
)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
