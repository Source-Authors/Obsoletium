# GNU LESSER GENERAL PUBLIC LICENSE
# Version 3, 29 June 2007
#
# Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
#
# Everyone is permitted to copy and distribute verbatim copies of this license
# document, but changing it is not allowed.
#
# This version of the GNU Lesser General Public License incorporates the terms and
# conditions of version 3 of the GNU General Public License, supplemented by the
# additional permissions listed below.

cmake_minimum_required(VERSION 3.5)
project(mpaheaderinfo VERSION 2.2.0 LANGUAGES CXX)

# C++ standard can be overridden when this is used as a sub-project.
if(NOT CMAKE_CXX_STANDARD)
  # This project requires C++11.
  set(CMAKE_CXX_STANDARD 11)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)
endif(NOT CMAKE_CXX_STANDARD)

# https://github.com/izenecloud/cmake/blob/master/SetCompilerWarningAll.cmake
if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Use the highest warning level for Visual Studio.
  set(CMAKE_CXX_WARNING_LEVEL 4)
  if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  else(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
  endif(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")

  # Disable C++ exceptions.
  # string(REGEX REPLACE "/EH[a-z]+" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /EHs-c-")
  # add_definitions(-D_HAS_EXCEPTIONS=0)

  # Disable RTTI.
  string(REGEX REPLACE "/GR" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GR-")

  # Use static runtime.
  foreach(flag_var
      CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
      CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
      CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
      CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO)
    if (${flag_var} MATCHES "/MD")
      string(REGEX REPLACE "/MD" "/MT" ${flag_var} "${${flag_var}}")
    endif()
  endforeach()
else(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
  # Use -Wall for clang and gcc.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
  endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wall")

  # Use -Wextra for clang and gcc.
  if(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wextra")
  endif(NOT CMAKE_CXX_FLAGS MATCHES "-Wextra")

  # Use -Werror for clang only.
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if(NOT CMAKE_CXX_FLAGS MATCHES "-Werror")
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Werror")
    endif(NOT CMAKE_CXX_FLAGS MATCHES "-Werror")
  endif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")

  # Disable C++ exceptions.
  # string(REGEX REPLACE "-fexceptions" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  # set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")

  # Disable RTTI.
  string(REGEX REPLACE "-frtti" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")

# BUILD_SHARED_LIBS is a standard CMake variable, but we declare it here to make
# it prominent in the GUI.
option(BUILD_SHARED_LIBS "Build shared libraries(DLLs)." OFF)

# Build without ATL support.
option(MPA_BUILD_WITHOUT_ATL "Build for VPC." OFF)

add_library(mpaheaderinfo "")

target_sources(mpaheaderinfo
  PRIVATE
    "resource.h"
    "stdafx.h"
    "APETag.cpp"
    "ID3V1Tag.cpp"
    "ID3V2Tag.cpp"
    "LameTag.cpp"
    "Lyrics3Tag.cpp"
    "MPAEndOfFileException.cpp"
    "MPAException.cpp"
    "MPAFile.cpp"
    "MPAFileStream.cpp"
    "MPAFrame.cpp"
    "MPAHeader.cpp"
    "MPAStream.cpp"
    "MusicMatchTag.cpp"
    "stdafx.cpp"
    "Tag.cpp"
    "Tags.cpp"
    "VBRHeader.cpp"
    "VBRIHeader.cpp"
    "XINGHeader.cpp"

  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/APETag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ID3V1Tag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/ID3V2Tag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/LameTag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Lyrics3Tag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAEndOfFileException.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAException.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAFile.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAFileStream.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAFrame.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAHeader.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MPAStream.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/MusicMatchTag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Tag.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/Tags.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/VBRHeader.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/VBRIHeader.h>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/XINGHeader.h>
)
target_include_directories(mpaheaderinfo
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)

if (MPA_BUILD_WITHOUT_ATL)
  target_compile_definitions(mpaheaderinfo
    PRIVATE
      -DMPA_BUILD_WITHOUT_ATL=1
  )
endif(MPA_BUILD_WITHOUT_ATL)

set_target_properties(mpaheaderinfo
  PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

if (BUILD_SHARED_LIBS)
  set_target_properties(mpaheaderinfo PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif(BUILD_SHARED_LIBS)
