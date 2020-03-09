cmake_minimum_required(VERSION 3.13)
project(tgvoiprate)

set(CMAKE_CXX_STANDARD 14)

include_directories(
        /usr/include/opus
)

add_executable(tgvoiprate main.cpp)
target_link_libraries(tgvoiprate opusfile)