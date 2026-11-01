//========= Copyright Valve Corporation, All rights reserved. ============//
//
//	VPROJECT.H
//
//	Master Header.
//=====================================================================================//
#pragma once

#include <winsock2.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <cstdio>
#include <malloc.h>
#include <richedit.h>
#include <cassert>
#include <ctime>
#include "resource.h"
#include <sys/stat.h>
#include "sys_utils.h"

#define VPROJECT_VERSION		"1.0"
#define VPROJECT_CLASSNAME		"VPROJECTCLASS"
#define VPROJECT_TITLE			"VProject"
#define VPROJECT_MAGIC			"2\\"
#define VPROJECT_REGISTRY		"HKEY_CURRENT_USER\\Software\\VProject\\" VPROJECT_MAGIC
#ifndef NDEBUG
#define VPROJECT_BUILDTYPE		"Debug"
#else
#define VPROJECT_BUILDTYPE		"Release"
#endif


