cmake_minimum_required(VERSION 3.17)
project(dvd-screensaver VERSION 1.1.0)

set(PLUGIN_AUTHOR "univrsal")
set(PLUGIN_GIT dvds3)
set(LINUX_MAINTAINER_EMAIL "uni@vrsal.cf")
set(MACOS_BUNDLEID "cf.vrsal.dvds3")

set(CMAKE_PREFIX_PATH "${QTDIR}")
set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(LOCAL_INSTALLATION "Whether to install for the current user (default: OFF)" OFF)
option(GLOBAL_INSTALLATION "Whether to install for all users (default: OFF)" OFF)

configure_file(
    package/installer-macOS.pkgproj.in
    ../package/installer-macOS.generated.pkgproj
)
configure_file(
    package/installer-Windows.iss.in
    ../package/installer-Windows.generated.iss
)
configure_file(
    ci/ci_includes.sh.in
    ../ci/ci_includes.generated.sh
)
configure_file(
    ci/ci_includes.cmd.in
    ../ci/ci_includes.generated.cmd
)
configure_file(
	package/README.txt.in
	package/README.txt
)

if (WIN32 OR APPLE)
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
endif()

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

find_package(LibObs REQUIRED)
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

set(dvd-screensaver_SOURCES
    src/dvd.c)

add_library(dvd-screensaver MODULE
    ${dvd-screensaver_SOURCES})

include_directories(
	"${LIBOBS_INCLUDE_DIR}/../UI/obs-frontend-api"
	${Qt5Core_INCLUDES}
	${Qt5Widgets_INCLUDES}
)

target_link_libraries(dvd-screensaver
	libobs
	Qt5::Core
	Qt5::Widgets
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
		"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/UI"
	)

	target_link_libraries(${CMAKE_PROJECT_NAME}
		"${OBS_FRONTEND_LIB}"
    )

	# --- Release package helper ---
	# The "release" folder has a structure similar OBS' one on Windows
	set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

    add_custom_command(TARGET ${CMAKE_PROJECT_NAME} POST_BUILD
        # If config is Release or RelWithDebInfo, package release files
        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${RELEASE_DIR}/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Release>==1 OR $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # If config is RelWithDebInfo, copy the pdb file
        COMMAND if $<CONFIG:RelWithDebInfo>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${RELEASE_DIR}/obs-plugins/${ARCH_NAME}"
        )

        # Copy to obs-studio dev environment for immediate testing
        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy
                "$<TARGET_PDB_FILE:${CMAKE_PROJECT_NAME}>"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E make_directory
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )

        COMMAND if $<CONFIG:Debug>==1 (
            "${CMAKE_COMMAND}" -E copy_directory
                "${PROJECT_SOURCE_DIR}/data"
                "${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/${CMAKE_PROJECT_NAME}"
        )
    )
endif()

if(UNIX AND NOT APPLE)

	include(GNUInstallDirs)

	set_target_properties(dvd-screensaver PROPERTIES PREFIX "")
	target_link_libraries(${CMAKE_PROJECT_NAME} obs-frontend-api)

	file(GLOB locale_files data/locale/*.ini)

	if (LOCAL_INSTALLATION)
		# Installs into home directory
		install(TARGETS dvd-screensaver
			LIBRARY DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/dvd-screensaver/bin/${OBS_ARCH_NAME}")

		install(FILES ${locale_files}
			DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/dvd-screensaver/data/locale/")
	elseif(GLOBAL_INSTALLATION)
		# For *.deb installer
		install(TARGETS ${CMAKE_PROJECT_NAME}
			LIBRARY DESTINATION "${CMAKE_INSTALL_FULL_LIBDIR}/obs-plugins")

		install(FILES ${locale_files}
			DESTINATION "${CMAKE_INSTALL_FULL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
	else()
		# For *.zip binary
		install(TARGETS dvd-screensaver
			LIBRARY DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/dvd-screensaver/bin/${OBS_ARCH_NAME}")

		install(FILES ${locale_files}
			DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/dvd-screensaver/data/locale/")
		install(FILES ./COPYING
			DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/")
		install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/package/README.txt"
			DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/")
	endif()
endif()
