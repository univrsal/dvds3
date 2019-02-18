cmake_minimum_required(VERSION 2.8)
project(dvd-screensaver)

if(MSVC)
    set(dvd-screensaver_PLATFORM_DEPS
        w32-pthreads)
endif()

if(UNIX)
    add_definitions(-DUNIX=1)
endif()

set(dvd-screensaver_SOURCES
    src/dvd.c)

add_library(dvd-screensaver MODULE
    ${dvd-screensaver_SOURCES})

target_link_libraries(dvd-screensaver
    libobs
    ${dvd-screensaver_PLATFORM_DEPS})

install_obs_plugin_with_data(dvd-screensaver data)