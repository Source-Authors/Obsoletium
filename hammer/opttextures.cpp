//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
//=============================================================================//

#include "stdafx.h"
#include "hammer.h"
#include "GameConfig.h"
#include "OptionProperties.h"
#include "OPTTextures.h"
#include "MainFrm.h"
#include "mapdoc.h"
#include "Options.h"
#include "tier1/strtools.h"
#include "com_ptr.h"
#include <shlobj.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>
/////////////////////////////////////////////////////////////////////////////
// COPTTextures property page

IMPLEMENT_DYNCREATE(COPTTextures, CBasePropertyPage)

COPTTextures::COPTTextures() : CBasePropertyPage(COPTTextures::IDD)
{
	//{{AFX_DATA_INIT(COPTTextures)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT

	m_pMaterialConfig = NULL;
	m_bDeleted = FALSE;
}

COPTTextures::~COPTTextures()
{
	// detach the material exclusion list box
	m_MaterialExcludeList.Detach();
}

void COPTTextures::DoDataExchange(CDataExchange* pDX)
{
	__super::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(COPTTextures)
	DDX_Control(pDX, IDC_TEXTUREFILES, m_TextureFiles);
	DDX_Control(pDX, IDC_BRIGHTNESS, m_cBrightness);
	DDX_Control(pDX, IDC_BRIGHTNESSTEXT, m_cBrightnessText);
	//}}AFX_DATA_MAP

	m_cBrightness.SetRange(1, 50, TRUE);

	//
	// If going from controls to data.
	//
	if (pDX->m_bSaveAndValidate)
	{
		Options.textures.fBrightness = (float)m_cBrightness.GetPos() / 10.0f;
	}
	//
	// Else going from data to controls.
	//
	else
	{
		CString str;

		//
		// Brightness.
		//
		m_cBrightness.SetPos(int(Options.textures.fBrightness * 10));
		int iBrightness = m_cBrightness.GetPos();
		str.Format("%d", iBrightness);
		m_cBrightnessText.SetWindowText(str);
	}
}


BEGIN_MESSAGE_MAP(COPTTextures, CBasePropertyPage)
	//{{AFX_MSG_MAP(COPTTextures)
	ON_BN_CLICKED(IDC_EXTRACT, OnExtract)
	ON_BN_CLICKED(IDC_ADDTEXFILE, OnAddtexfile)
	ON_BN_CLICKED(IDC_REMOVETEXFILE, OnRemovetexfile)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDC_ADDTEXFILE2, OnAddtexfile2)

	ON_BN_CLICKED( ID_MATERIALEXCLUDE_ADD, OnMaterialExcludeAdd )
	ON_BN_CLICKED( ID_MATERIALEXCLUDE_REM, OnMaterialExcludeRemove )
	ON_LBN_SELCHANGE(ID_MATERIALEXCLUDE_LIST, OnMaterialExcludeListSel)

	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// COPTTextures message handlers

BOOL COPTTextures::OnInitDialog() 
{
	__super::OnInitDialog();
	
	// load texture file list with options
	int i;
	for(i = 0; i < Options.textures.nTextureFiles; i++)
	{
		m_TextureFiles.AddString(Options.textures.TextureFiles[i]);
	}

	// attach the material exclusion list box
	m_MaterialExcludeList.Attach( GetDlgItem( ID_MATERIALEXCLUDE_LIST )->m_hWnd );

	return TRUE;
}


void COPTTextures::OnExtract() 
{
	// redo listbox content
	m_TextureFiles.ResetContent();
	for(int i = 0; i < Options.textures.nTextureFiles; i++)
	{
		m_TextureFiles.AddString(Options.textures.TextureFiles[i]);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : 
//-----------------------------------------------------------------------------
void COPTTextures::OnAddtexfile(void)
{
	static char szInitialDir[MAX_PATH] = "\0";

	CFileDialog dlg(TRUE, "wad", NULL, OFN_ALLOWMULTISELECT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_FILEMUSTEXIST, "Texture Files (*.wad;*.pak)|*.wad; *.pak||");
	dlg.m_ofn.lpstrTitle = "Open Texture File";

	if (szInitialDir[0] == '\0')
	{
		Q_snprintf( szInitialDir, sizeof( szInitialDir ), "%s\\wads\\", g_pGameConfig->m_szModDir );
	}

	dlg.m_ofn.lpstrInitialDir = szInitialDir;

	if (dlg.DoModal() != IDOK)
	{
		return;
	}

	//
	// Get all the filenames from the open file dialog.
	//
	POSITION pos = dlg.GetStartPosition();
	CString str;
	while (pos != NULL)
	{
		str = dlg.GetNextPathName(pos);
		str.MakeLower();
		m_TextureFiles.AddString(str);
		SetModified();
	}

	//
	// Use this directory as the default directory for the next time.
	//
	int nBackslash = str.ReverseFind('\\');
	if (nBackslash != -1)
	{
		V_strncpy(szInitialDir, str, nBackslash + 1);
	}
}


void COPTTextures::OnRemovetexfile() 
{
	int i = m_TextureFiles.GetCount();

	for (i--; i >= 0; i--)
	{
		if (m_TextureFiles.GetSel(i))
			m_TextureFiles.DeleteString(i);
	}

	m_bDeleted = TRUE;
}

void COPTTextures::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar) 
{
	//
	// If it is the brightness scroll bar, update the brightness text.
	// Also, notify the 3D view so that it can update in realtime.
	//
	if (pScrollBar->m_hWnd == m_cBrightness.m_hWnd)
	{
		SetModified();

		int iBrightness = m_cBrightness.GetPos();

		CString str;
		str.Format("%d", iBrightness);
		m_cBrightnessText.SetWindowText(str);

		CMainFrame *pMainWnd = GetMainWnd();
		if (pMainWnd != NULL)
		{
			Options.textures.fBrightness = (float)m_cBrightness.GetPos() / 10.0f;
			pMainWnd->UpdateAllDocViews( MAPVIEW_OPTIONS_CHANGED | MAPVIEW_RENDER_NOW );
		}
	}
	
	__super::OnHScroll(nSBCode, nPos, pScrollBar);
}

BOOL COPTTextures::OnApply() 
{
	int iSize = m_TextureFiles.GetCount();
	CString str;
	Options.textures.nTextureFiles = iSize;
	Options.textures.TextureFiles.RemoveAll();
	for(int i = 0; i < iSize; i++)
	{
		m_TextureFiles.GetText(i, str);
		Options.textures.TextureFiles.Add(str);
	}
	
	if(m_bDeleted)
	{
		// inform them that deleted files will only be reapplied after
		// they reload the editor
		MessageBox("You have removed some texture files from the list.\n"
			"These texture files will continue to be used during this "
			"session, but will not be loaded the next time you run "
			"Hammer.", "Hammer - Quick Note", MB_ICONINFORMATION);
	}

	Options.PerformChanges(COptions::secTextures);

	return __super::OnApply();
}


void COPTTextures::OnAddtexfile2() 
{
	BROWSEINFO bi;
	char szDisplayName[MAX_PATH];

	bi.hwndOwner = m_hWnd;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = szDisplayName;
	bi.lpszTitle = "Select your Quake II directory.";
	bi.ulFlags = BIF_RETURNONLYFSDIRS;
	bi.lpfn = NULL;
	bi.lParam = 0;
	
	LPITEMIDLIST pidlNew = SHBrowseForFolder(&bi);

	if(pidlNew)
	{

		// get path from the id list
		char szPathName[MAX_PATH];
		SHGetPathFromIDList(pidlNew, szPathName);
		
		
		if (AfxMessageBox("Add all subdirectories as separate Texture Groups?", MB_YESNO | MB_ICONQUESTION) == IDYES)
		//if (!strcmpi("\\textures", &szPathName[strlen(szPathName) - strlen("\\textures")]))
		{
			char szNewPath[MAX_PATH];
			V_strcpy_safe(szNewPath, szPathName);
			V_strcat_safe(szNewPath, "\\*.*");
			WIN32_FIND_DATA FindData;
			HANDLE hFile = FindFirstFile(szNewPath, &FindData);

			if (hFile != INVALID_HANDLE_VALUE) do
			{
				if ((FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
						&&(FindData.cFileName[0] != '.'))
				{
					V_sprintf_safe(szNewPath, "%s\\%s", szPathName, FindData.cFileName);
					strlwr(szNewPath);
					if (m_TextureFiles.FindStringExact(-1, szNewPath) == CB_ERR)
						m_TextureFiles.AddString(szNewPath);
				}
			} while (FindNextFile(hFile, &FindData));

		}
		else
		{
			strlwr(szPathName);
			if (m_TextureFiles.FindStringExact(-1, szPathName) == CB_ERR)
				m_TextureFiles.AddString(strlwr(szPathName));
		}
		SetModified();

		// free the previous return value from SHBrowseForFolder
		CoTaskMemFree(pidlNew);

	}
}


static char s_szStartFolder[MAX_PATH];
static int CALLBACK BrowseCallbackProc( HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData )
{
	switch ( uMsg )
	{
		case BFFM_INITIALIZED:
		{
			if ( lpData )
			{
				SendMessage( hwnd, BFFM_SETSELECTION, TRUE, ( LPARAM ) s_szStartFolder );
			}
			break;
		}

		default:
		{
		   break;
		}
	}
         
	return 0;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pszTitle - 
//			*pszDirectory - 
// Output : Returns TRUE on success, FALSE on failure.
//-----------------------------------------------------------------------------
BOOL COPTTextures::BrowseForFolder( char *pszTitle, char *pszDirectory )
{
	static bool s_bFirst = true;
	if ( s_bFirst )
	{
		APP()->GetDirectory( DIR_MATERIALS, s_szStartFolder );
		s_bFirst = false;
	}

	LPITEMIDLIST pidlStartFolder = NULL;

	se::win::com::com_ptr<IShellFolder> shDesktop;
	if ( s_bFirst && SUCCEEDED( SHGetDesktopFolder( &shDesktop ) ) )
	{
		wchar_t wszStartFolder[std::size(s_szStartFolder)];
		Q_UTF8ToUTF16(s_szStartFolder, wszStartFolder, sizeof(wszStartFolder));

		shDesktop->ParseDisplayName( NULL, NULL, wszStartFolder, NULL, &pidlStartFolder, NULL );
	}
	
	char szTmp[MAX_PATH];

	BROWSEINFO bi = {};
	bi.hwndOwner = m_hWnd;
	bi.pidlRoot = pidlStartFolder;
	bi.pszDisplayName = szTmp;
	bi.lpszTitle = pszTitle;
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_BROWSEFILEJUNCTIONS /*| BIF_NEWDIALOGSTYLE*/;
	bi.lpfn = BrowseCallbackProc;
	bi.lParam = TRUE;

	LPITEMIDLIST idl = SHBrowseForFolder( &bi );
	if ( idl == NULL )
	{
		return FALSE;
	}

	if ( SHGetPathFromIDList( idl, pszDirectory ) )
	{
		// Start in this folder next time.
		V_strcpy_safe( s_szStartFolder, pszDirectory );
	}

	CoTaskMemFree( idl );
	if ( pidlStartFolder )
	{
		CoTaskMemFree( pidlStartFolder );
	}

	return TRUE;
}


//-----------------------------------------------------------------------------
// Purpose: intercept this call and update the "material" configuration pointer
//          if it is out of sync with the game configuration "parent" (on the
//          "Options Configs" page)
//  Output: returns TRUE on success, FALSE on failure
//-----------------------------------------------------------------------------
BOOL COPTTextures::OnSetActive( void )
{
	//
	// get the current game configuration from the "Options Configs" page
	//
	COptionProperties *pOptProps = ( COptionProperties* )GetParent();
	if( !pOptProps )
		return FALSE;

	CGameConfig *pConfig = pOptProps->Configs.GetCurrentConfig();
	if( !pConfig )
		return FALSE;

	// compare for a change
	if( m_pMaterialConfig != pConfig )
	{
		// update the material config
		m_pMaterialConfig = pConfig;

		// update the all config specific material data on this page
		MaterialExcludeUpdate();

		// update the last material config
		m_pMaterialConfig = pConfig;
	}

	return __super::OnSetActive();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COPTTextures::MaterialExcludeUpdate( void )
{
	// remove all of the data in the current "material exclude" list box
	m_MaterialExcludeList.ResetContent();

	//
	// add the data from the current material config
	//
	for( int i = 0; i < m_pMaterialConfig->m_MaterialExcludeCount; i++ )
	{
		int result = m_MaterialExcludeList.AddString( m_pMaterialConfig->m_MaterialExclusions[i].szDirectory );
		m_MaterialExcludeList.SetItemData( result, 1 );
		if( ( result == LB_ERR ) || ( result == LB_ERRSPACE ) )
			return;
	}
	if (pGD != NULL)	
	{
		for( int i = 0; i < pGD->m_FGDMaterialExclusions.Count(); i++ )
		{
			char szFolder[MAX_PATH];
			V_strcpy_safe( szFolder, pGD->m_FGDMaterialExclusions[i].szDirectory );
			V_strcat_safe( szFolder, " (default)" );
			int result = m_MaterialExcludeList.AddString( szFolder );
			m_MaterialExcludeList.SetItemData( result, 0 );
			if( ( result == LB_ERR ) || ( result == LB_ERRSPACE ) )
				return;
		}
	}
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
template<intp nameSize>
void StripOffMaterialDirectory( const char *pszDirectoryName, char (&pszName)[nameSize] )
{
	// clear name
	if (nameSize)
		pszName[0] = '\0';

	// create a lower case version of the string
	char *pLowerCase = _strlwr( _strdup( pszDirectoryName ) );
	char *pAtMat = strstr( pLowerCase, "materials" );
	if( !pAtMat )
		return;

	// move the pointer ahead 10 spaces = "materials\"
	pAtMat += 10;

	// copy the rest to the name string
	V_strcpy_safe( pszName, pAtMat );

	// free duplicated string's memory
	free( pLowerCase );
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COPTTextures::OnMaterialExcludeAdd( void )
{
	//
	// get the directory path to exclude
	//
	char szTmp[MAX_PATH];
	if( !BrowseForFolder( "Select Material Directory To Exclude", szTmp ) )
		return;

	// strip off the material directory
	char szSubDirName[MAX_PATH];
	StripOffMaterialDirectory( szTmp, szSubDirName );
	if( szSubDirName[0] == '\0' )
		return;

	//
	// add directory to list box
	//
	int result = m_MaterialExcludeList.AddString( szSubDirName );
	m_MaterialExcludeList.SetItemData( result, 1 );
	if( ( result == LB_ERR ) || ( result == LB_ERRSPACE ) )
		return;
	
	//
	// add name of directory to the global exclusion list
	//
	int ndx = m_pMaterialConfig->m_MaterialExcludeCount;
	if( ndx >= MAX_DIRECTORY_SIZE )
		return;
	m_pMaterialConfig->m_MaterialExcludeCount++;

	intp index = m_pMaterialConfig->m_MaterialExclusions.AddToTail();
	Q_strncpy( m_pMaterialConfig->m_MaterialExclusions[index].szDirectory, szSubDirName, sizeof ( m_pMaterialConfig->m_MaterialExclusions[index].szDirectory ) );

}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COPTTextures::OnMaterialExcludeRemove( void )
{
	//
	// get the directory to remove
	//
	int ndxSel = m_MaterialExcludeList.GetCurSel();
	if( ndxSel == LB_ERR )
		return;

	char szTmp[MAX_PATH];
	m_MaterialExcludeList.GetText( ndxSel, &szTmp[0] );

	//
	// remove directory from the list box
	//
	int result = m_MaterialExcludeList.DeleteString( ndxSel );
	if( result == LB_ERR )
		return;

	//
	// remove the name of the directory from the global exclusion list
	//
	for( int i = 0; i < m_pMaterialConfig->m_MaterialExcludeCount; i++ )
	{
		if( !strcmp( szTmp, m_pMaterialConfig->m_MaterialExclusions[i].szDirectory ) )
		{
			// remove the directory
			if( i != ( m_pMaterialConfig->m_MaterialExcludeCount - 1 ) )
			{
				m_pMaterialConfig->m_MaterialExclusions.Remove( i );
			}

			// decrement count
			m_pMaterialConfig->m_MaterialExcludeCount--;
		}
	}
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
void COPTTextures::OnMaterialExcludeListSel( void )
{
	int ndxSel = m_MaterialExcludeList.GetCurSel();
	if( ndxSel == LB_ERR )
		return;

	char szTmp[MAX_PATH];
	m_MaterialExcludeList.GetText( ndxSel, &szTmp[0] );

	// Item data of 0 = FGD exclusion, 1 = user-created exclusion
	DWORD dwData = static_cast<DWORD>(m_MaterialExcludeList.GetItemData( ndxSel ));
	GetDlgItem( ID_MATERIALEXCLUDE_REM )->EnableWindow( dwData ? TRUE : FALSE );
}

