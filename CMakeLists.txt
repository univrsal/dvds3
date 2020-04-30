cmake_minimum_required(VERSION 2.8)
project(dvd-screensaver)

include(cmake/FindLibobs.cmake)

if(MSVC)
    set(dvd-screensaver_PLATFORM_DEPS
        w32-pthreads)
endif()

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCH 64)
else()
    set(ARCH 32)
endif()

if(ARCH EQUAL 64)
    set(OBS_ARCH_NAME "64bit")
    set(OBS_BUILDDIR_ARCH "build64")
else()
    set(OBS_ARCH_NAME "32bit")
    set(OBS_BUILDDIR_ARCH "build32")
endif()

if(UNIX)
    add_definitions(-DUNIX=1)
endif()

if(APPLE)
    add_definitions(-DMAC_OS=1)
endif()

set(dvd-screensaver_SOURCES
    src/dvd.c)

add_library(dvd-screensaver MODULE
    ${dvd-screensaver_SOURCES})

set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

target_link_libraries(dvd-screensaver
    libobs
    ${dvd-screensaver_PLATFORM_DEPS})

add_custom_command(TARGET dvd-screensaver POST_BUILD
	COMMAND if $<CONFIG:Release>==1 (
		"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver"
			"${RELEASE_DIR}/obs-plugins/${OBS_ARCH_NAME}")

	COMMAND if $<CONFIG:Release>==1 (
		"${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/src/data"
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver")

	COMMAND if $<CONFIG:Release>==1 (
		"${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:dvd-screensaver>"
			"${RELEASE_DIR}/obs-plugins/${OBS_ARCH_NAME}")
			
    #Copy to obs-studio dev environment for immediate testing
    COMMAND if $<CONFIG:Debug>==1 (
        "${CMAKE_COMMAND}" -E copy
            "$<TARGET_FILE:dvd-screensaver>"
            "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${OBS_ARCH_NAME}")

    COMMAND if $<CONFIG:Debug>==1 (
        "${CMAKE_COMMAND}" -E make_directory
            "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/dvd-screensaver")

    COMMAND if $<CONFIG:Debug>==1 (
        "${CMAKE_COMMAND}" -E copy_directory
            "${PROJECT_SOURCE_DIR}/data"
            "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/dvd-screensaver")
)