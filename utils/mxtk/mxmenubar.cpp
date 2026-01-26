//
//                 mxToolKit (c) 1999 by Mete Ciragan
//
// file:           mxMenuBar.cpp
// implementation: Win32 API
// last modified:  Apr 28 1999, Mete Ciragan
// copyright:      The programs and associated files contained in this
//                 distribution were developed by Mete Ciragan. The programs
//                 are not in the public domain, but they are freely
//                 distributable without licensing fees. These programs are
//                 provided without guarantee or warrantee expressed or
//                 implied.
//
#include "stdafx.h"
#include "mxtk/mxmenubar.h"
#include "winlite.h"



class mxMenuBar_i
{
public:
	HWND d_hwnd;
};



mxMenuBar::mxMenuBar (mxWindow *parent)
: mxWidget (0, 0, 0, 0, 0)
{
	// dimhotepus: Store parent window to compute DPI.
	d_this = new mxMenuBar_i;

	void *handle = CreateMenu ();
	setHandle (handle);
	setType (MX_MENUBAR);
	setParent (parent);

	if (parent)
	{
		mxWidget *w = (mxWidget *) parent;
		
		d_this->d_hwnd = (HWND) w->getHandle ();

		SetMenu (d_this->d_hwnd, (HMENU) handle);
	}
}



mxMenuBar::~mxMenuBar ()
{
	delete d_this;
}



void
mxMenuBar::addMenu (const char *item, mxMenu *menu)
{
	AppendMenu ((HMENU) getHandle (), MF_POPUP, (UINT_PTR) ((mxWidget *) menu)->getHandle (), item);
}



void
mxMenuBar::setEnabled (int id, bool b)
{
	EnableMenuItem ((HMENU) getHandle (), (UINT) id, MF_BYCOMMAND | (b ? MF_ENABLED:MF_GRAYED));
}



void
mxMenuBar::setChecked (int id, bool b)
{
	CheckMenuItem ((HMENU) getHandle (), (UINT) id, MF_BYCOMMAND | (b ? MF_CHECKED:MF_UNCHECKED));
}



void
mxMenuBar::modify (int id, int newId, const char *newItem)
{
	ModifyMenu ((HMENU) getHandle (), (UINT) id, MF_BYCOMMAND | MF_STRING, (UINT) newId, (LPCTSTR) newItem);
}



bool
mxMenuBar::isEnabled (int id) const
{
	MENUITEMINFO mii;

	memset (&mii, 0, sizeof (mii));
	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STATE;
	GetMenuItemInfo ((HMENU) getHandle (), (UINT) id, false, &mii);
	if (mii.fState & MFS_GRAYED)
		return true;

	return false;
}



bool
mxMenuBar::isChecked (int id) const
{
	MENUITEMINFO mii;

	memset (&mii, 0, sizeof (mii));
	mii.cbSize = sizeof (mii);
	mii.fMask = MIIM_STATE;
	GetMenuItemInfo ((HMENU) getHandle (), (UINT) id, false, &mii);
	if (mii.fState & MFS_CHECKED)
		return true;

	return false;
}



int
mxMenuBar::getHeight () const
{
	const unsigned dpi{d_this->d_hwnd ? GetDpiForWindow( d_this->d_hwnd ) : USER_DEFAULT_SCREEN_DPI};
	// dimhotepus: Get real height of menu bar.
	return GetSystemMetricsForDpi( SM_CYMENU, dpi );
}
