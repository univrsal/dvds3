cmake_minimum_required(VERSION 3.17)
project(dvd-screensaver VERSION 1.1.0)

set(PLUGIN_AUTHOR "univrsal")
set(PLUGIN_GIT dvds3)
set(LINUX_MAINTAINER_EMAIL "uni@vrsal.cf")
set(MACOS_BUNDLEID "cf.vrsal.dvds3")

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR TRUE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
#set(CMAKE_AUTOMOC ON)
#set(CMAKE_AUTOUIC ON)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(LOCAL_INSTALLATION "Whether to install for the current user (default: OFF)" OFF)
option(GLOBAL_INSTALLATION "Whether to install for all users (default: OFF)" OFF)

include(${CMAKE_SOURCE_DIR}/cmake/ObsPluginHelpers.cmake)
if (MSVC OR APPLE)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
    include(${CMAKE_SOURCE_DIR}/cmake/FindLibObs.cmake)
endif()

configure_file(
    package/installer-macOS.pkgproj.in
    ../package/installer-macOS.pkgproj
)
configure_file(
    package/installer-Windows.iss.in
    ../package/installer-Windows.iss
)
configure_file(
    package/README.txt.in
    ../package/README.txt
)

if(MSVC)
    set(dvd-screensaver_PLATFORM_DEPS
        ${${W32_PTHREADS_LIB})
endif()

if(UNIX AND NOT APPLE)
    add_definitions(-DUNIX=1)
    set(LIBOBS_LIB "libobs")
    set(OBS_FRONTEND_LIB "obs-frontend-api")
endif()

if(APPLE)
    add_definitions(-DMAC_OS=1)
endif()

find_package(LibObs REQUIRED)
#find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

set(dvd-screensaver_SOURCES
    src/dvd.c)

add_library(dvd-screensaver MODULE
    ${dvd-screensaver_SOURCES})

if (OBS_FRONTEND_INCLUDE)
    include_directories(${OBS_FRONTEND_INCLUDE})
endif()

include_directories(
    "${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
    ${LIBOBS_INCLUDE_DIR}
#    ${Qt5Core_INCLUDES}
#    ${Qt5Widgets_INCLUDES}
)

target_link_libraries(dvd-screensaver
    ${LIBOBS_LIB}
#    ${OBS_FRONTEND_LIB}
#    Qt5::Core
#    Qt5::Widgets
    ${dvd-screensaver_PLATFORM_DEPS})


# --- Windows-specific build settings and tasks ---
if(WIN32)
    if(NOT DEFINED OBS_FRONTEND_LIB)
        set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
        message(FATAL_ERROR "Could not find OBS Frontend API\'s library !")
    endif()

    # Enable Multicore Builds and disable FH4 (to not depend on VCRUNTIME140_1.DLL when building with VS2019)
    if (MSVC)
        add_definitions(/MP /d2FH4-)
    endif()

    include_directories(
        "${LIBOBS_INCLUDE_DIR}"
    )

    install_external_plugin_with_data(${CMAKE_PROJECT_NAME} "data")
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)

    set_target_properties(dvd-screensaver PROPERTIES PREFIX "")

    if (LOCAL_INSTALLATION)
        # Installs into home directory
        install(TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/${CMAKE_PROJECT_NAME}/bin/${OBS_ARCH_NAME}")

        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
            DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/${CMAKE_PROJECT_NAME}/")
    elseif(GLOBAL_INSTALLATION)
        # For *.deb installer
        install(TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "/usr/lib/obs-plugins") # Ubuntu apparently doesn't know the correct path
        
        file(GLOB locale_files data/locale/*.ini)
        file(GLOB other_files data/*.*)
        
        install(FILES ${locale_files}
            DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
        install(FILES ${other_files}
		    DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
    else()
        # For *.zip binary
        install(TARGETS ${CMAKE_PROJECT_NAME}
            LIBRARY DESTINATION "${CMAKE_PROJECT_NAME}/bin/${OBS_ARCH_NAME}")

        install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/data
            DESTINATION "${CMAKE_PROJECT_NAME}")
        
        install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/package/README.txt"
            DESTINATION "${CMAKE_PROJECT_NAME}")
    endif()
elseif(APPLE)
    if(NOT DEFINED OBS_FRONTEND_LIB)
        set(OBS_FRONTEND_LIB "OBS_FRONTEND_LIB-NOTFOUND" CACHE FILEPATH "OBS frontend library")
        message(FATAL_ERROR "Could not find OBS Frontend API\'s library !")
    endif()
endif()
