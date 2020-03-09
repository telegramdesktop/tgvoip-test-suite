add_library(opusfile
    info.c
    internal.c
    opusfile.c
    stream.c
    wincerts.c
)

target_include_directories(opusfile PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(opusfile
    opus
    ogg
)
