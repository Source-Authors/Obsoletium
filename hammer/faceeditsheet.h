//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef FACEEDITSHEET_H
#define FACEEDITSHEET_H
#pragma once

#include <afxtempl.h>
#include "FaceEdit_MaterialPage.h"
#include "FaceEdit_DispPage.h"
#include "windows/base_property_sheet.h"

class CMapFace;
class CMapSolid;

//=============================================================================
//
// Face Edit Sheet
//
class CFaceEditSheet : public CBasePropertySheet
{
	DECLARE_DYNAMIC( CFaceEditSheet )

public:

	// solid/face selection structure
	typedef struct
	{
		CMapDoc *pMapDoc;	// From whence it came, so if we close the CMapDoc we can unselect it.
		CMapFace *pMapFace;
		CMapSolid *pMapSolid;
	} StoredFace_t;

	//=========================================================================
	//
	// Creation/Destruction
	//
					CFaceEditSheet( LPCTSTR pszCaption, CWnd *pParentWnd = NULL, UINT iSelectPage = 0 );
	virtual			~CFaceEditSheet();

	BOOL			Create( CWnd *pParentWnd );
	void			Setup( void );

	//=========================================================================
	//
	// Update
	//
	void			SetVisibility( bool bVisible );
	inline void		NotifyGraphicsChanged( void );

	void			CloseAllPageDialogs( void );

	void			UpdateControls( void );
	
	// Called when the CMapDoc goes away. Don't hang onto old face pointers that come from this doc.
	void			ClearFaceListByMapDoc( CMapDoc *pDoc );

	//=========================================================================
	//
	// Selection
	//
	enum { FACE_LIST_SIZE = 32 };

	enum
	{
		id_SwitchModeStart = 0x100,
		ModeLiftSelect,
		ModeLift,
		ModeSelect,
		ModeApply,
		ModeApplyAll,
		ModeApplyLightmapScale,
		ModeAlignToView,
		id_SwitchModeEnd
	};

	enum
	{
		cfToggle    = 0x01,			// toggle - if selected, then unselect
		cfSelect    = 0x02,			// select
		cfUnselect  = 0x04,			// unselect
		cfClear     = 0x08,			// clear face list
		cfEdgeAlign = 0x10			// align face texture coordinates to 3d view alignment - should be here???
	};

	void ClickFace( CMapSolid *pSolid, int faceIndex, int cmd, int clickMode = -1 );
	inline void SetClickMode( int mode );
	inline int GetClickMode( void );

	void EnableUpdate( bool bEnable );
	inline bool HasUpdateEnabled( void );
	
	inline intp GetFaceListCount( void );
	inline CMapFace *GetFaceListDataFace( intp index );
	inline CMapSolid *GetFaceListDataSolid( intp index );

	// Called when a new material is detected.
	void NotifyNewMaterial( IEditorTexture *pTex );
	
	//=========================================================================
	//
	// Virtual Overrides
	//
	//{{AFX_VIRTUAL( CFaceEditSheet )
	virtual BOOL PreTranslateMessage( MSG *pMsg );
	//}}AFX_VIRTUAL

	CFaceEditMaterialPage				m_MaterialPage;			// material "page"
	CFaceEditDispPage					m_DispPage;				// displacement "page"

protected:

	CUtlVector<StoredFace_t>			m_Faces;				// face list

//	CFaceEditMaterialPage				m_MaterialPage;			// material "page"
//	CFaceEditDispPage					m_DispPage;				// displacement "page"

	int									m_ClickMode;			// lame this is here!!!! -- see CMapDoc (SelectFace)

	bool								m_bEnableUpdate;		// update the dialog (true/false)

	//=========================================================================
	//
	// Creation/Destruction
	//
	void SetupPages( void );
	void SetupButtons( void );

	//=========================================================================
	//
	// Selection
	//
	void ClearFaceList( void );
	intp FindFaceInList( CMapFace *pFace );

	//=========================================================================
	//
	// Message Map
	//
	//{{AFX_MSG( CFaceEditSheet )
	afx_msg void OnClose( void );
	//}}AFX_MSG

	DECLARE_MESSAGE_MAP()
};


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CFaceEditSheet::NotifyGraphicsChanged( void )
{
	m_MaterialPage.NotifyGraphicsChanged();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline void CFaceEditSheet::SetClickMode( int mode )
{
	m_ClickMode = mode;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline int CFaceEditSheet::GetClickMode( void )
{ 
	return m_ClickMode; 
}

//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline intp CFaceEditSheet::GetFaceListCount( void )
{
	return m_Faces.Count();
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CMapFace *CFaceEditSheet::GetFaceListDataFace( intp index )
{
	return m_Faces[index].pMapFace;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline CMapSolid *CFaceEditSheet::GetFaceListDataSolid( intp index )
{
	return m_Faces[index].pMapSolid;
}


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
inline bool CFaceEditSheet::HasUpdateEnabled( void )
{
	return m_bEnableUpdate;
}


#endif // FACEEDITSHEET_H