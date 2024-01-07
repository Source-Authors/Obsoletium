# Copyright (c) 2024 The Source Authors.  All rights reserved.

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

# Compiler id for Apple Clang is now AppleClang.
if (POLICY CMP0025)
  cmake_policy(SET CMP0025 NEW)
endif()

# Include file check macros honor CMAKE_REQUIRED_LIBRARIES.
if (POLICY CMP0075)
  cmake_policy(SET CMP0075 NEW)
endif()

# option() honor variables.
if (POLICY CMP0077)
  cmake_policy(SET CMP0077 NEW)
endif()

# Setup root directory of the Obsoletium project.
set(SRC_ROOT_DIR "${CMAKE_SOURCE_DIR}" CACHE STRING
  "Directory root of the Obsoletium project.")

## Package information.
set(PACKAGE_NAME   "Obsoletium")

# Git is required to build software version via Git.
find_package(Git)

if (NOT DEFINED PACKAGE_VERSION)
  if (NOT Git_FOUND)
    message(FATAL "[common]: Unable to extract Git software version - no Git.")
  endif()

  message(STATUS "[common]: Extracting Git software version.")

  if ("${CMAKE_HOST_SYSTEM_NAME}" STREQUAL "Windows")
    execute_process(COMMAND cmd /c "git rev-list --branches HEAD | find /v " " /c" OUTPUT_VARIABLE SRC_GIT_VERSION WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  else()
    execute_process(COMMAND bash "-c" "git rev-list --branches HEAD | wc -l | tr -d ' ' | tr -d '\n'" OUTPUT_VARIABLE SRC_GIT_VERSION WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  set(PACKAGE_VER_PATCH 2)
  math(EXPR PACKAGE_VER_TWEAK ${SRC_GIT_VERSION})
  message(STATUS
    "[common]: Build version from Git (patch.tweak): ${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}.")

  set(PACKAGE_VER_MAJOR 1)
  set(PACKAGE_VER_MINOR 3)

  set(PACKAGE_VERSION
    "${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}.${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}")
endif()
set(PACKAGE_STRING     "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_TARNAME    "${PACKAGE_NAME}-${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT  "https://github.com/Source-Authors/obsolete-source-engine/issues")

project(${PACKAGE_NAME}
  VERSION ${PACKAGE_VER_MAJOR}.${PACKAGE_VER_MINOR}.${PACKAGE_VER_PATCH}.${PACKAGE_VER_TWEAK}
  LANGUAGES CXX)

message(STATUS "[common]: CMake version: ${CMAKE_VERSION}.")
message(STATUS "[common]: ${PACKAGE_NAME} version: ${PACKAGE_VERSION}.")

## Common variables.

set(SRC_BINARY_DIR "${CMAKE_BINARY_DIR}" CACHE STRING
  "Directory of the Obsoletium binaries output.")
set(SRC_SYSTEM_NAME "${CMAKE_SYSTEM_NAME}" CACHE STRING
  "Name of the Obsoletium build system.")

if (${SRC_SYSTEM_NAME} STREQUAL "Darwin")
  set(SRC_OS_MACOS ON CACHE BOOL "MacOS is build system.")
else()
  set(SRC_OS_MACOS OFF CACHE BOOL "MacOS is NOT build system.")
endif()

if (${SRC_SYSTEM_NAME} STREQUAL "Linux")
  set(SRC_OS_LINUX ON CACHE BOOL "Linux distro is build system.")
else()
  set(SRC_OS_LINUX OFF CACHE BOOL "Linux distro is NOT build system.")
endif()

if (${SRC_SYSTEM_NAME} STREQUAL "Windows")
  set(SRC_OS_WIN ON CACHE BOOL "Windows is build system.")
else()
  set(SRC_OS_WIN OFF CACHE BOOL "Windows is NOT build system.")
endif()

if (SRC_OS_MACOS OR SRC_OS_LINUX)
  set(SRC_OS_POSIX ON CACHE BOOL "POSIX compatible os is build system.")
else()
  set(SRC_OS_POSIX OFF CACHE BOOL "POSIX compatible os is NOT build system.")
endif()

set(SRC_PRODUCT_NAME "Obsoletium" CACHE STRING
  "Name of the Obsoletium product.")

# Minimum standard level is C++17.
set(CMAKE_CXX_STANDARD 17)
# On g++ this ensures: -std=c++17 and not -std=gnu++17
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRC_BUILD_TYPE "${CMAKE_BUILD_TYPE}" CACHE STRING
  "CMake build type used to build ${PACKAGE_NAME}.")
set(SRC_CMAKE_GENERATOR "${CMAKE_GENERATOR}" CACHE STRING
  "CMake generator used to generate build tree." FORCE)
set(SRC_CXX_COMPILER "${CMAKE_CXX_COMPILER}" CACHE STRING
  "Compiler used to build ${PACKAGE_NAME}.")
set(SRC_CXX_COMPILER_ID "${CMAKE_CXX_COMPILER_ID}" CACHE STRING
  "Compiler id used to build ${PACKAGE_NAME}.")
set(SRC_CXX_COMPILER_FRONTEND_VARIANT "${CMAKE_CXX_COMPILER_FRONTEND_VARIANT}"
  CACHE STRING "Compiler frontend used to build ${PACKAGE_NAME}.")
set(SRC_EXPORT_COMPILE_COMMANDS ${CMAKE_EXPORT_COMPILE_COMMANDS} CACHE BOOL
  "Enable/Disable output of compile commands during generation.")

if (SRC_CXX_COMPILER_ID MATCHES "Clang")
  set(SRC_COMPILER_CLANG ON CACHE BOOL "Compiler is Clang.")
else()
  set(SRC_COMPILER_CLANG OFF CACHE BOOL "Compiler is NOT Clang.")
endif()

if (SRC_CXX_COMPILER_ID STREQUAL "GNU")
  set(SRC_COMPILER_GCC ON CACHE BOOL "Compiler is GCC.")
else()
  set(SRC_COMPILER_GCC OFF CACHE BOOL "Compiler is NOT GCC.")
endif()

if (SRC_CXX_COMPILER_ID STREQUAL "MSVC")
  set(SRC_COMPILER_MSVC ON CACHE BOOL "Compiler is MSVC.")
else()
  set(SRC_COMPILER_MSVC OFF CACHE BOOL "Compiler is NOT MSVC.")
endif()

## Common dependencies.

if (Git_FOUND)
  message(STATUS
    "[common]: Git version ${GIT_VERSION_STRING} found at '${GIT_EXECUTABLE}'.")

  # Get the current working branch.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse --abbrev-ref HEAD
    WORKING_DIRECTORY ${SRC_ROOT_DIR}
    OUTPUT_VARIABLE SRC_GIT_BRANCH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the latest commit hash.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" rev-parse HEAD
    WORKING_DIRECTORY ${SRC_ROOT_DIR}
    OUTPUT_VARIABLE SRC_GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE)

  # Get the subject of the commit.
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" log -1 --format=%s
    WORKING_DIRECTORY "${SRC_ROOT_DIR}"
    OUTPUT_VARIABLE SRC_GIT_COMMIT_SUBJECT
    ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)

  message(STATUS
    "[common]: Git using branch '${SRC_GIT_BRANCH}', "
    "commit ${SRC_GIT_COMMIT_HASH} | ${SRC_GIT_COMMIT_SUBJECT}.")
endif()

message(STATUS
  "[common]: ${PACKAGE_NAME} using CMake generator: ${SRC_CMAKE_GENERATOR}.")

# Select correct threading library.
if (NOT SRC_OS_WIN)
  set(CMAKE_THREAD_PREFER_PTHREAD ON)
  set(THREADS_PREFER_PTHREAD_FLAG ON)
endif()
find_package(Threads REQUIRED)
list(APPEND CMAKE_REQUIRED_LIBRARIES Threads::Threads)
if (NOT CMAKE_THREAD_LIBS_INIT)
  set(SRC_THREADS_LIBRARY "system default" CACHE STRING
    "Threads library used to build ${PACKAGE_NAME}.")
else()
  set(SRC_THREADS_LIBRARY "${CMAKE_THREAD_LIBS_INIT}" CACHE STRING
    "Threads library used to build ${PACKAGE_NAME}.")
endif()
message(STATUS
  "[common]: ${PACKAGE_NAME} using threading library: ${SRC_THREADS_LIBRARY}.")

if (XCODE_VERSION)
  message(STATUS
    "[common]: ${PACKAGE_NAME} using XCode version: ${XCODE_VERSION}.")
endif()

message(STATUS "[common]: ${PACKAGE_NAME} cxx compiler: ${SRC_CXX_COMPILER}.")
message(STATUS "[common]: ${PACKAGE_NAME} build type: ${SRC_BUILD_TYPE}.")

if (SRC_OS_WIN)
  # Generate common product version info.
  configure_file(
    ${SRC_ROOT_DIR}/build/product_version_config.h.cmake.in
    ${SRC_BINARY_DIR}/build/product_version_config.h
  )

  # Generate app version info.
  configure_file(
    ${SRC_ROOT_DIR}/build/app_version_config.h.cmake.in
    ${SRC_BINARY_DIR}/build/app_version_config.h
  )

  # Generate visual styles manifest info.
  configure_file(
    ${SRC_ROOT_DIR}/build/win/manifests/enable-visual-styles.manifest.cmake.in
    ${SRC_BINARY_DIR}/manifests/enable-visual-styles.manifest
  )
endif()
