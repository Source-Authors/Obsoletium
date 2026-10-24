#//-----------------------------------------------------------------------------
#//	APPFRAMEWORK.cmake
#//
#//	Project Script
#//-----------------------------------------------------------------------------
cmake_minimum_required(VERSION 3.16)
project(appframework)

set(SRCDIR "${CMAKE_SOURCE_DIR}")

set(OUTLIBNAME "${_IMPLIB_PREFIX}appframework")
set(AF_SRCDIR "${SRCDIR}/appframework")
set(AF_INCDIR "${SRCDIR}/public/appframework")

include(${CMAKE_MODULE_PATH}/detect_platform.cmake)
include(${CMAKE_MODULE_PATH}/source_lib_base.cmake)

if( NOT IS_LIB_PROJECT AND GL )
    message(FATAL_ERROR "can't build appframework as dll")
    #		$AdditionalProjectDependencies	"$BASE;togl"	[!$IS_LIB_PROJECT && $GL]
endif()

if( (WINDOWS AND GL) OR LINUXALL )
    add_definitions(-DVERSION_SAFE_STEAM_API_INTERFACES -DDX_TO_GL_ABSTRACTION)
endif()
if( CSTRIKE_TRUNK_BUILD OR CSTRIKE_STAGING_BUILD )
    add_definitions(-DALLOW_TEXT_MODE=1)
endif()

#Sources
if( LINUXALL )
    #Use system SDL2 for linux
    include_directories("/usr/include/SDL2")
    target_compile_options(${OUTLIBNAME} PRIVATE -fpic -fno-semantic-interposition)
else()
    include_directories(${SRCDIR}/thirdparty/SDL2)
endif()

# $Folder "Source Files"
target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/AppSystemGroup.cpp")
target_sources(${OUTLIBNAME} PRIVATE "${SRCDIR}/public/filesystem_init.cpp")

if( WINDOWS )
    target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/vguimatsysapp.cpp")
    target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/winapp.cpp")
endif()

if( POSIX )
    target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/posixapp.cpp")
endif()

if( SDL )
    target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/sdlmgr.cpp")
endif()

# $Folder "Interface"
target_sources(${OUTLIBNAME} PRIVATE
    "${AF_INCDIR}/IAppSystem.h"
    "${AF_INCDIR}/AppFramework.h"
    "${AF_INCDIR}/IAppSystem.h"
    "${AF_INCDIR}/IAppSystemGroup.h"
    "${AF_INCDIR}/tier2app.h"
    "${AF_INCDIR}/tier3app.h"
    "${AF_INCDIR}/VguiMatSysApp.h"
    "${AF_INCDIR}/ilaunchermgr.h")

if( OSXALL )
    target_sources(${OUTLIBNAME} PRIVATE "${AF_SRCDIR}/glmrendererinfo_osx.mm")
endif()

#$Folder	"Link Libraries"
#{
#    $ImpLib togl [!$IS_LIB_PROJECT && $GL]
#    $ImpLib SDL2 [!$IS_LIB_PROJECT && $SDL]
#}