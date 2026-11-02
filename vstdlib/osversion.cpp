//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//
//=============================================================================//

#include "vstdlib/osversion.h"
#include "winlite.h"
#include "tier1/strtools.h"
#include "tier0/dbg.h"

#include <cstdio>
#include <cstring>

#if defined(OSX)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#if defined(LINUX)
#include <sys/utsname.h>
#endif

namespace
{

bool IsMacOSType(EOSType type)
{
	return type >= k_eMacOSUnknown && type < k_eOSUnknown;
}

bool IsLinuxType(EOSType type)
{
	return type >= k_eLinuxUnknown && type < k_eMacOSUnknown;
}

bool IsWindowsType(EOSType type)
{
	return type >= k_eWinUnknown;
}

#if defined(OSX)
bool GetMacOSVersion(unsigned &major, unsigned &minor, unsigned &patch)
{
	char version[64] = {};
	size_t size = sizeof(version);

	if (sysctlbyname("kern.osproductversion", version, &size, nullptr, 0) != 0)
		return false;

	return sscanf(version, "%u.%u.%u", &major, &minor, &patch) >= 2;
}
#endif

#if defined(_WIN32)
DWORD GetWindowsBuildNumber()
{
	OSVERSIONINFOEXW version = {};
	version.dwOSVersionInfoSize = sizeof(version);

	using RtlGetVersionFn = LONG(WINAPI *)(OSVERSIONINFOW *);
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (!ntdll)
		return 0;

	RtlGetVersionFn rtlGetVersion =
		reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));

	if (!rtlGetVersion ||
		rtlGetVersion(reinterpret_cast<OSVERSIONINFOW *>(&version)) != 0)
	{
		return 0;
	}

	return version.dwBuildNumber;
}
#endif
}

EOSType GetOSType()
{
	static EOSType cachedType = k_eOSUnknown;

	if (cachedType != k_eOSUnknown)
		return cachedType;

#if defined(_WIN32)
	cachedType = k_eWinUnknown;

	if (IsWindows10OrGreater())
		cachedType = GetWindowsBuildNumber() >= 22000
			? k_eWindows11
			: k_eWindows10;
	else if (IsWindows8Point1OrGreater())
		cachedType = k_eWindows8_1;
	else if (IsWindows8OrGreater())
		cachedType = k_eWindows8;
	else if (IsWindows7OrGreater())
		cachedType = k_eWindows7;
	else if (IsWindowsVistaOrGreater())
		cachedType = k_eWinVista;
	else if (IsWindowsXPOrGreater())
		cachedType = k_eWinXP;

#elif defined(OSX)
	unsigned major = 0;
	unsigned minor = 0;
	unsigned patch = 0;

	if (!GetMacOSVersion(major, minor, patch))
		return k_eMacOSUnknown;

	if (major == 10)
	{
		switch (minor)
		{
		case 4:  cachedType = k_eMacOS104; break;
		case 5:  cachedType = patch >= 8 ? k_eMacOS1058 : k_eMacOS105; break;
		case 6:  cachedType = patch >= 3 ? k_eMacOS1063 : k_eMacOS106; break;
		case 7:  cachedType = k_eMacOS107; break;
		case 8:  cachedType = k_eMacOS108; break;
		case 9:  cachedType = k_eMacOS109; break;
		case 10: cachedType = k_eMacOS1010; break;
		case 11: cachedType = k_eMacOS1011; break;
		case 12: cachedType = k_eMacOS1012; break;
		case 13: cachedType = k_eMacOS1013; break;
		case 14: cachedType = k_eMacOS1014; break;
		case 15: cachedType = k_eMacOS1015; break;
		default: cachedType = k_eMacOSUnknown; break;
		}
	}
	else if (major >= 11)
	{
		switch (major)
		{
		case 11: cachedType = k_eMacOS11; break;
		case 12: cachedType = k_eMacOS12; break;
		case 13: cachedType = k_eMacOS13; break;
		case 14: cachedType = k_eMacOS14; break;
		default: cachedType = k_eMacOS15; break;
		}
	}
	else
	{
		cachedType = k_eMacOSUnknown;
	}

#elif defined(LINUX)
	cachedType = k_eLinuxUnknown;

	struct utsname info;
	if (uname(&info) == 0)
	{
		unsigned major = 0;
		unsigned minor = 0;

		if (sscanf(info.release, "%u.%u", &major, &minor) == 2)
		{
			if (major == 2 && minor == 2)
				cachedType = k_eLinux22;
			else if (major == 2 && minor == 4)
				cachedType = k_eLinux24;
			else if (major == 2 && minor >= 6)
				cachedType = k_eLinux26;
			else if (major >= 3)
				cachedType = k_eLinux3OrGreater;
		}
	}
#endif

	return cachedType;
}

const char *GetOSDetailString(char *outBuf, int outBufSize)
{
	if (!outBuf || outBufSize <= 0)
		return nullptr;

#if defined(_WIN32)
	(void)outBuf;
	(void)outBufSize;
	return nullptr;
#else
#if defined(LINUX)
	FILE *file = fopen("/etc/os-release", "r");
	if (file)
	{
		size_t bytesRead =
			fread(outBuf, 1, static_cast<size_t>(outBufSize - 1), file);
		fclose(file);

		outBuf[bytesRead] = '\0';
		if (bytesRead > 0)
			return outBuf;
	}
#endif

	struct utsname info;
	if (uname(&info) != 0)
		return nullptr;

	snprintf(outBuf, static_cast<size_t>(outBufSize), "%s %s %s",
		info.sysname, info.release, info.machine);
	return outBuf;
#endif
}

const char *GetNameFromOSType(EOSType type)
{
	switch (type)
	{
	case k_eWinUnknown: return "Windows";
	case k_eWin311: return "Windows 3.11";
	case k_eWin95: return "Windows 95";
	case k_eWin98: return "Windows 98";
	case k_eWinME: return "Windows ME";
	case k_eWinNT: return "Windows NT";
	case k_eWin2000: return "Windows 2000";
	case k_eWinXP: return "Windows XP";
	case k_eWin2003: return "Windows 2003";
	case k_eWinVista: return "Windows Vista";
	case k_eWindows7: return "Windows 7";
	case k_eWin2008: return "Windows 2008";
	case k_eWindows8: return "Windows 8";
	case k_eWindows8_1: return "Windows 8.1";
	case k_eWindows10: return "Windows 10";
	case k_eWindows11: return "Windows 11";

	case k_eMacOSUnknown: return "Mac OS";
	case k_eMacOS104: return "macOS 10.4";
	case k_eMacOS105: return "macOS 10.5";
	case k_eMacOS1058: return "macOS 10.5.8";
	case k_eMacOS106: return "macOS 10.6";
	case k_eMacOS1063: return "macOS 10.6.3 or later";
	case k_eMacOS107: return "macOS 10.7";
	case k_eMacOS108: return "macOS 10.8";
	case k_eMacOS109: return "macOS 10.9";
	case k_eMacOS1010: return "macOS 10.10";
	case k_eMacOS1011: return "macOS 10.11";
	case k_eMacOS1012: return "macOS 10.12";
	case k_eMacOS1013: return "macOS 10.13";
	case k_eMacOS1014: return "macOS 10.14";
	case k_eMacOS1015: return "macOS 10.15";
	case k_eMacOS11: return "macOS 11";
	case k_eMacOS12: return "macOS 12";
	case k_eMacOS13: return "macOS 13";
	case k_eMacOS14: return "macOS 14";
	case k_eMacOS15: return "macOS 15 or later";

	case k_eLinuxUnknown: return "Linux";
	case k_eLinux22: return "Linux 2.2";
	case k_eLinux24: return "Linux 2.4";
	case k_eLinux26: return "Linux 2.6";
	case k_eLinux3OrGreater: return "Linux 3 or later";

	default: return "Unknown";
	}
}

bool OSTypesAreCompatible(EOSType detected, EOSType required)
{
	if (required == k_eOSUnknown)
		return true;

	if (IsWindowsType(required))
		return IsWindowsType(detected) && detected >= required;

	if (IsMacOSType(required))
		return IsMacOSType(detected) && detected >= required;

	if (IsLinuxType(required))
		return IsLinuxType(detected) && detected >= required;

	return false;
}

const char *GetPlatformName(bool *is64Bit)
{
	if (is64Bit)
		*is64Bit = Is64BitOS();

	const EOSType type = GetOSType();

	if (IsWindowsType(type))
		return "windows";
	if (IsMacOSType(type))
		return "macos";
	if (IsLinuxType(type))
		return "linux";

	return "unknown";
}
