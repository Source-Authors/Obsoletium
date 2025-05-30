// Copyright Valve Corporation, All rights reserved.
//
//

#ifndef IFILESYSTEMOPENDIALOG_H
#define IFILESYSTEMOPENDIALOG_H

#include "tier0/platform.h"

#define FILESYSTEMOPENDIALOG_VERSION	"FileSystemOpenDlg003"


class IFileSystem;


abstract_class IFileSystemOpenDialog
{
public:
	// You must call this first to set the hwnd.
	virtual void Init( CreateInterfaceFn factory, void *parentHwnd ) = 0;

	// Call this to free the dialog.
	virtual void Release() = 0;

	// Use these to configure the dialog.
	virtual void AddFileMask( const char *pMask ) = 0;
	virtual void SetInitialDir( const char *pDir, const char *pPathID = NULL ) = 0;
	virtual void SetFilterMdlAndJpgFiles( bool bFilter ) = 0;
	virtual void GetFilename( OUT_Z_CAP(outLen) char *pOut, intp outLen ) const = 0;	// Get the filename they choose.
	template<intp outSize>
	void GetFilename( OUT_Z_ARRAY char (&out)[outSize] )
	{
		GetFilename( out, outSize );
	}

	// Call this to make the dialog itself. Returns true if they clicked OK and false 
	// if they canceled it.
	virtual bool DoModal() = 0;

	// This uses the standard windows file open dialog.
	virtual bool DoModal_WindowsDialog() = 0;
};


#endif // IFILESYSTEMOPENDIALOG_H
