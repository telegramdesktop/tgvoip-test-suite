cmake_minimum_required(VERSION 3.13)
project(tgvoipcall)

set(CMAKE_CXX_STANDARD 14)
include(ExternalProject)

include_directories(
        /usr/include/opus
)

IF(APPLE)
    LINK_DIRECTORIES(/usr/local/lib)
    include_directories(
            /usr/local/include/opus
            /usr/local/include
    )
ENDIF()

ExternalProject_Add(
        libtgvoip
	URL https://github.com/telegramdesktop/libtgvoip/archive/c5651ff.tar.gz
	#Unstable
	#URL https://github.com/telegramdesktop/libtgvoip/archive/da27199.tar.gz
        PREFIX libtgvoip
        BUILD_IN_SOURCE 1
	CONFIGURE_COMMAND ./configure CPPFLAGS=-DTGVOIP_LOG_VERBOSITY=5 --enable-audio-callback --without-pulse --without-alsa
	#CONFIGURE_COMMAND ./configure CPPFLAGS=-DTGVOIP_LOG_VERBOSITY=0 --enable-audio-callback --without-pulse --without-alsa
        BUILD_COMMAND make
        INSTALL_COMMAND ""
)
ExternalProject_Get_Property(libtgvoip binary_dir)
add_library(tgvoip SHARED IMPORTED)
set_property(TARGET tgvoip PROPERTY IMPORTED_LOCATION ${binary_dir}/.libs/libtgvoip.so)
add_dependencies(tgvoip libtgvoip)
include_directories(${binary_dir} ${binary_dir}/webrtc_dsp)
add_definitions(-DTGVOIP_USE_CALLBACK_AUDIO_IO)

add_executable(tgvoipcall main.cpp)
set_target_properties(tgvoipcall PROPERTIES LINK_FLAGS "-Wl,-rpath,./")
target_link_libraries(tgvoipcall tgvoip opus crypto dl pthread)
