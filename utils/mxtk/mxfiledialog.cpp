//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxFileDialog.cpp
// implementation: Win32 API
// last modified:  Mar 14 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "mxtk/mxfiledialog.h"
#include "mxtk/mxwindow.h"

#include "winlite.h"
#include <commdlg.h>

#include "tier1/strtools.h"


static char sd_path[_MAX_PATH] = "";



const char*
mxGetOpenFileName (mxWindow *parent, const char *path, const char *filter)
{
	CHAR szPath[_MAX_PATH], szFilter[_MAX_PATH];

	sd_path[0] = '\0';

	if (path)
		V_strcpy_safe (szPath, path);
	else
		szPath[0] = '\0';

	if (filter)
	{
		memset (szFilter, 0, _MAX_PATH);
		V_strcpy_safe (szFilter, filter);
		strcpy (szFilter + strlen (szFilter) + 1, filter);
	}
	else
		szFilter[0] = '\0';


	OPENFILENAME ofn;
	memset (&ofn, 0, sizeof (ofn));
	ofn.lStructSize = sizeof (ofn);
	if (parent)
		ofn.hwndOwner = (HWND) parent->getHandle ();
	ofn.hInstance = (HINSTANCE) GetModuleHandle (NULL);
	ofn.lpstrFilter = szFilter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = sd_path;
	ofn.nMaxFile = _MAX_PATH;
	if (path && strlen (path))
		ofn.lpstrInitialDir = szPath;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName (&ofn))
			return sd_path;
		else
			return 0;
}



const char*
mxGetSaveFileName (mxWindow *parent, const char *path, const char *filter)
{
	CHAR szPath[_MAX_PATH], szFilter[_MAX_PATH];

	sd_path[0] = '\0';

	if (path)
		V_strcpy_safe (szPath, path);
	else
        szPath[0] = '\0';

	if (filter)
	{
		memset (szFilter, 0, _MAX_PATH);
		V_strcpy_safe (szFilter, filter);
		strcpy (szFilter + strlen (szFilter) + 1, filter);
	}
	else
		szFilter[0] = '\0';

	OPENFILENAME ofn;
	memset (&ofn, 0, sizeof (ofn));
	ofn.lStructSize = sizeof (ofn);
	if (parent)
		ofn.hwndOwner = (HWND) parent->getHandle ();
	ofn.hInstance = (HINSTANCE) GetModuleHandle (NULL);
	ofn.lpstrFilter = szFilter;
	ofn.lpstrFile = sd_path;
	ofn.nMaxFile = _MAX_PATH;
	if (path && strlen (path))
		ofn.lpstrInitialDir = szPath;
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

	if (GetSaveFileName (&ofn))
			return sd_path;
		else
			return 0;
}
