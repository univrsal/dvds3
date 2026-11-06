cmake_minimum_required(VERSION 3.28...3.30)

# dvd.c uses some API that is deprecated in newer OBS versions, don't fail the build over it
set(CMAKE_COMPILE_WARNING_AS_ERROR OFF)
include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)
option(INCLUDE_SIMDE "Include simde (needed for the libobs headers of the Ubuntu PPA)" OFF)
option(LOCAL_INSTALLATION "Copy the plugin into the OBS Studio plugin folder in the home directory (Linux only)" OFF)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

if(INCLUDE_SIMDE)
  include(FetchContent)
  FetchContent_Declare(simde GIT_REPOSITORY https://github.com/simd-everywhere/simde.git GIT_TAG v0.8.2)
  FetchContent_MakeAvailable(simde)
  target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE ${simde_SOURCE_DIR})
endif()

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/dvd.c)

if(APPLE)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE MAC_OS=1)
elseif(UNIX)
  target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE UNIX=1)
  # Needed for the runtime lookup of the image functions in dvd.c
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_DL_LIBS})
endif()

if(LOCAL_INSTALLATION AND UNIX AND NOT APPLE)
  # Installs into home directory, OBS Studio 33 moved user plugins from
  # $XDG_CONFIG_HOME/obs-studio/plugins/<plugin>/bin/64bit to $XDG_DATA_HOME/obs-studio/plugins/<plugin>
  if(libobs_VERSION VERSION_GREATER_EQUAL 33)
    if(DEFINED ENV{XDG_DATA_HOME})
      set(_local_plugin_dir "$ENV{XDG_DATA_HOME}/obs-studio/plugins/${CMAKE_PROJECT_NAME}")
    else()
      set(_local_plugin_dir "$ENV{HOME}/.local/share/obs-studio/plugins/${CMAKE_PROJECT_NAME}")
    endif()
    set(_local_binary_dir "${_local_plugin_dir}")
  else()
    set(_local_plugin_dir "$ENV{HOME}/.config/obs-studio/plugins/${CMAKE_PROJECT_NAME}")
    set(_local_binary_dir "${_local_plugin_dir}/bin/64bit")
  endif()
  # Read by the CI build script to package the local installation
  set(LOCAL_PLUGIN_DIR "${_local_plugin_dir}" CACHE INTERNAL "")

  add_custom_command(
    TARGET ${CMAKE_PROJECT_NAME}
    POST_BUILD
    # Copy to obs-studio dev environment for immediate testing
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_local_binary_dir}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${_local_plugin_dir}/data"
    COMMAND "${CMAKE_COMMAND}" -E copy "$<TARGET_FILE:${CMAKE_PROJECT_NAME}>" "${_local_binary_dir}"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${PROJECT_SOURCE_DIR}/data" "${_local_plugin_dir}/data"
  )
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
