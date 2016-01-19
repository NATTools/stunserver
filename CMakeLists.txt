#
#
# top level build file for stunserver

## prepare CMAKE
cmake_minimum_required ( VERSION 3.2.0 )

set ( STUNSERVER_VERSION_MAJOR "0" CACHE STRING "Project major version number")
set ( STUNSERVER_VERSION_MINOR "1" CACHE STRING "Project minor version number" )
set ( STUNSERVER_VERSION_PATCH "0" CACHE STRING "Project patch version number" )
mark_as_advanced(STUNSERVER_VERSION_MAJOR STUNSERVER_VERSION_MINOR STUNSERVER_VERSION_PATCH)

set ( STUNSERVER_VERSION "${STUNSERVER_VERSION_MAJOR}.${STUNSERVER_VERSION_MINOR}.${STUNSERVER_VERSION_PATCH}" )

project ( "STUNSERVER" VERSION "${STUNSERVER_VERSION}")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include ( Uncrustify )

#find_package(Doxygen)

## setup options
option ( verbose "Produce verbose makefile output" OFF )
option ( optimize "Set high optimization level" OFF )
option ( fatal_warnings "Treat build warnings as errors" ON )
option ( coveralls "Generate coveralls data" ON )
option ( coveralls_send "Send data to coveralls site" OFF )
option ( build_docs "Create docs using Doxygen" ${DOXYGEN_FOUND} )
option ( uncrustify "Uncrustify the source code" ${UNCRUSTIFY_FOUND} )

set ( dist_dir    ${CMAKE_BINARY_DIR}/dist )
set ( prefix      ${CMAKE_INSTALL_PREFIX} )
set ( exec_prefix ${CMAKE_INSTALL_PREFIX}/bin )
set ( libdir      ${CMAKE_INSTALL_PREFIX}/lib )
set ( includedir  ${CMAKE_INSTALL_PREFIX}/include )

set ( package_prefix "${CMAKE_PACKAGE_NAME}-${CMAKE_SYSTEM_NAME}" )

set ( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${dist_dir}/bin )
set ( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${dist_dir}/lib )
set ( CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${dist_dir}/lib )
set ( CMAKE_BUILD_TYPE Debug )

## check and generate configuration

include ( CheckIncludeFiles )
include ( CheckLibraryExists )
include ( CheckFunctionExists )
include ( CheckTypeSize )

check_include_files ( stdint.h HAVE_STDINT_H )
check_include_files ( stdlib.h HAVE_STDLIB_H )
check_include_files ( stdbool.h HAVE_STDBOOL_H )
check_function_exists ( arc4random HAVE_ARC4RANDOM )
check_library_exists ( pthread pthread_create "" HAVE_LIBPTHREAD )
check_library_exists ( m tan "" HAVE_LIBM )

configure_file ( ${CMAKE_CURRENT_SOURCE_DIR}/config.h.cmake ${CMAKE_CURRENT_BINARY_DIR}/config.h )

## setup global compiler options
include_directories ( ${CMAKE_CURRENT_BINARY_DIR} )
if ( CMAKE_C_COMPILER_ID STREQUAL "GNU" OR
     CMAKE_C_COMPILER_ID MATCHES "Clang" )
  message ( STATUS "adding GCC/Clang options ")
  add_definitions ( -std=gnu99 -Wall -Wextra -pedantic )
  ## disable VLA "is a GNU extension" warning
  add_definitions ( -Wno-gnu-zero-variadic-macro-arguments )
  if ( fatal_warnings )
    add_definitions ( -Werror )
  endif ()
  if ( optimize )
    add_definitions ( -O2 )
  endif ()
elseif ( MSVC )
  add_definitions ( /W3 )
  if ( fatal_warnings )
    add_definitions ( /WX )
  endif ()
else ()
  message ( FATAL_ERROR "unhandled compiler id: ${CMAKE_C_COMPILER_ID}" )
endif ()
if ( verbose )
  set ( CMAKE_VERBOSE_MAKEFILE ON )
endif ()

find_program( MEMORYCHECK_COMMAND valgrind )
set( MEMORYCHECK_COMMAND_OPTIONS "--trace-children=yes --leak-check=full" )

install (FILES AUTHORS LICENSE README.md DESTINATION .)

## setup packaging
set ( CPACK_GENERATOR "TGZ" )
set ( CPACK_PACKAGE_VERSION "${PROJECT_VERSION}" )
set ( CPACK_SOURCE_GENERATOR "TGZ" )
set ( CPACK_SOURCE_IGNORE_FILES "/\\\\.git/" )
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/.gitignore igs)
foreach (ig IN ITEMS ${igs})
    # remove comments
    string ( REGEX REPLACE "^\\s*#.*" "" ig "${ig}")
    # remove any other whitespace
    string ( STRIP "${ig}" ig)
    # anything left?
    if (ig)
      # dots are literal
      string ( REPLACE "." "\\\\." ig "${ig}" )
      # stars are on thars
      string ( REPLACE "*" ".*" ig "${ig}" )
      list ( APPEND CPACK_SOURCE_IGNORE_FILES "/${ig}/" )
    endif()
endforeach()

#message ( "CPACK_SOURCE_IGNORE_FILES: " ${CPACK_SOURCE_IGNORE_FILES} )
set ( CPACK_PACKAGE_DESCRIPTION_FILE ${CMAKE_CURRENT_SOURCE_DIR}/README.md )
set ( CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE" )

include ( CPack )

UncrustifyTop(${uncrustify})

Include(ExternalProject)

ExternalProject_Add(
   project_sockaddrutil
   GIT_REPOSITORY https://github.com/NATTools/sockaddrutil.git
   CMAKE_ARGS -Doptimize=OFF -Duse_context=ON -Dbuild_docs=OFF -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
   INSTALL_DIR "${dist_dir}"
   UPDATE_DISCONNECTED 1
)

ExternalProject_Get_Property(project_sockaddrutil install_dir)
include_directories ( "${install_dir}/include" )
add_library(sockaddrutil STATIC IMPORTED)
set_property(TARGET sockaddrutil PROPERTY IMPORTED_LOCATION "${install_dir}/lib/${CMAKE_SHARED_MODULE_PREFIX}sockaddrutil${CMAKE_SHARED_LIBRARY_SUFFIX}")
add_dependencies(sockaddrutil project_sockaddrutil)

ExternalProject_Add(
   project_stunlib
   GIT_REPOSITORY https://github.com/NATTools/stunlib.git
   CMAKE_ARGS -Doptimize=OFF -Duse_context=ON -Dbuild_docs=OFF -DCMAKE_INSTALL_PREFIX:PATH=<INSTALL_DIR>
   INSTALL_DIR "${dist_dir}"
   UPDATE_DISCONNECTED 1
)

ExternalProject_Get_Property(project_stunlib install_dir)
include_directories ( "${install_dir}/include" )
add_library(stunlib STATIC IMPORTED)
set_property(TARGET stunlib PROPERTY IMPORTED_LOCATION "${install_dir}/lib/${CMAKE_SHARED_MODULE_PREFIX}stunlib${CMAKE_SHARED_LIBRARY_SUFFIX}")
add_dependencies(stunlib project_stunlib)



## include the parts
add_subdirectory ( include )
add_subdirectory ( src )
