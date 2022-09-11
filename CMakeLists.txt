cmake_minimum_required(VERSION 3.0)

set(CMAKE_CXX_STANDARD 14)

project(RecordStreamer)

if(DEFINED ENV{SNAPCRAFT_BUILD_ENVIRONMENT})
    add_definitions(-DSNAPCRAFT_BUILD=1)
    set(SNAPCRAFT_BUILD YES)
endif()

add_subdirectory(WebRTSP)

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_search_module(LIBCONFIG REQUIRED libconfig)
pkg_search_module(GSOAP REQUIRED gsoap++)

file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    *.cpp
    *.h
    *.conf.sample
    README.md
    *.sh
    *.cmake
    onvif/*.h
    onvif/*.cpp
    onvif/*.nsmap
    onvif/*.sh)

file(GLOB_RECURSE SNAP RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    snap/*)

file(GLOB_RECURSE WWW RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    www/[^.]*)

add_executable(${PROJECT_NAME} ${SOURCES} ${SNAP} ${WWW})
target_include_directories(${PROJECT_NAME} PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/WebRTSP
    ${LIBCONFIG_INCLUDE_DIRS}
    ${SPDLOG_INCLUDE_DIRS}
    ${GSOAP_INCLUDE_DIRS}
    ${GSOAP_SHARE_DIR})
target_link_libraries(${PROJECT_NAME}
    ${LIBCONFIG_LIBRARIES}
    ${SPDLOG_LDFLAGS}
    ${GSOAP_LDFLAGS}
    RtspParser
    Signalling
    RtStreaming
    Client)
target_precompile_headers(${PROJECT_NAME} PRIVATE pch.h)

if(SNAPCRAFT_BUILD)
    install(TARGETS ${PROJECT_NAME} DESTINATION bin)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/record-streamer.conf.sample DESTINATION etc)
endif()
