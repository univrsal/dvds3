cmake_minimum_required(VERSION 2.8)
project(dvd-screensaver)

option(LOCAL_INSTALLATION "Whether to install for the current user (default: OFF)" OFF)

set(CMAKE_MODULE_PATH "{CMAKE_MODULE_PATH}")

include(external/FindLibObs.cmake)

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

set(dvd-screensaver_SOURCES
    src/dvd.c)

add_library(dvd-screensaver MODULE
    ${dvd-screensaver_SOURCES})

set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

target_link_libraries(dvd-screensaver
    libobs
    ${dvd-screensaver_PLATFORM_DEPS})

set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

if (WIN32)
set(RELEASE_DIR "${PROJECT_SOURCE_DIR}/release")

	add_custom_command(TARGET dvd-screensaver POST_BUILD
		# If config is Release, package release files
		COMMAND if $<CONFIG:Release>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Release>==1 ("${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver")

		COMMAND if $<CONFIG:Release>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:dvd-screensaver>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")


		# If config is RelWithDebInfo, package release files
		COMMAND if $<CONFIG:RelWithDebInfo>==1 (
			"${CMAKE_COMMAND}" -E make_directory
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy_directory
			"${PROJECT_SOURCE_DIR}/data"
			"${RELEASE_DIR}/data/obs-plugins/dvd-screensaver")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_FILE:dvd-screensaver>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:RelWithDebInfo>==1 ("${CMAKE_COMMAND}" -E copy
			"$<TARGET_PDB_FILE:dvd-screensaver>"
			"${RELEASE_DIR}/obs-plugins/${ARCH_NAME}")

		# Copy to obs-studio dev environment for immediate testing
		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
				"$<TARGET_FILE:dvd-screensaver>"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy
				"$<TARGET_PDB_FILE:dvd-screensaver>"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/obs-plugins/${ARCH_NAME}")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E make_directory
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/dvd-screensaver")

		COMMAND if $<CONFIG:Debug>==1 (
			"${CMAKE_COMMAND}" -E copy_directory
				"${PROJECT_SOURCE_DIR}/data"
				"${LIBOBS_INCLUDE_DIR}/../${OBS_BUILDDIR_ARCH}/rundir/$<CONFIG>/data/obs-plugins/dvd-screensaver")
	)
endif()

if(UNIX AND NOT APPLE)

	include(GNUInstallDirs)

	set_target_properties(dvd-screensaver PROPERTIES PREFIX "")

	file(GLOB locale_files data/locale/*.ini)

	if (LOCAL_INSTALLATION)
		install(TARGETS dvd-screensaver
			LIBRARY DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/dvd-screensaver/bin/${OBS_ARCH_NAME}")

		install(FILES ${locale_files}
			DESTINATION "$ENV{HOME}/.config/obs-studio/plugins/dvd-screensaver/data/locale/")
	else()
		install(TARGETS dvd-screensaver
			LIBRARY DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/dvd-screensaver/bin/${OBS_ARCH_NAME}")

		install(FILES ${locale_files}
			DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/dist/dvd-screensaver/data/locale/")
		install(FILES ./COPYING
			DESTINATION "${CMAKE_CURRENT_SOURCE_DIR}/build/LICENSE.txt")
		configure_file(
			package/README.txt.in
			"${CMAKE_CURRENT_SOURCE_DIR}/dist/README.txt"
		)
	endif()
endif()
