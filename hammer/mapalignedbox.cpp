//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#include "stdafx.h"
#include "Box3D.h"
#include "fgdlib/HelperInfo.h"
#include "MapAlignedBox.h"
#include "MapEntity.h"
#include "Options.h"
#include "Render2D.h"
#include "Render3D.h"

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


IMPLEMENT_MAPCLASS(CMapAlignedBox)


//-----------------------------------------------------------------------------
// Purpose: Factory function. Used for creating a CMapAlignedBox from a set
//			of string parameters from the FGD file.
// Input  : *pInfo - Pointer to helper info class which gives us information
//				about how to create the class.
// Output : Returns a pointer to the class, NULL if an error occurs.
//-----------------------------------------------------------------------------
CMapClass *CMapAlignedBox::Create(CHelperInfo *pHelperInfo, CMapEntity *pParent)
{
	if (stricmp(pHelperInfo->GetName(), "wirebox") == 0)
	{
		const char *pMinsKeyName, *pMaxsKeyName;

		if (((pMinsKeyName = pHelperInfo->GetParameter(0)) != NULL) &&
			((pMaxsKeyName = pHelperInfo->GetParameter(1)) != NULL))
		{
			CMapAlignedBox *pBox = new CMapAlignedBox(pMinsKeyName, pMaxsKeyName);
			pBox->m_bWireframe = true;
			return pBox;
		}
		else
		{
			return NULL;
		}
	}
	else
	{
		//
		// Extract the box mins and maxs from the input parameter list.
		//
		Vector Mins;
		Vector Maxs;
		int nParam = 0;

		intp nCount = pHelperInfo->GetParameterCount();
		for (intp i = 0; i < nCount; i++)
		{
			const char *pszParam = pHelperInfo->GetParameter(i);

			if (nParam < 3)
			{
				// dimhoteous: atof -> V_atof.
				Mins[nParam] = V_atof(pszParam);
			}
			else if (nParam < 6)
			{
				// dimhoteous: atof -> V_atof.
				Maxs[nParam % 3] = V_atof(pszParam);
			}
			else
			{
				break;
			}

			nParam++;
		}

		CMapAlignedBox *pBox = NULL;

		//
		// If we got enough parameters to define the box, create it.
		//
		if (nParam >= 6)
		{
			pBox = new CMapAlignedBox(Mins, Maxs);
		}

		return(pBox);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAlignedBox::CMapAlignedBox(void)
{
	m_bWireframe = false;
	m_bUseKeyName = false;

	m_MinsKeyName[0] = '\0';
	m_MaxsKeyName[0] = '\0';

	m_Mins.Init();
	m_Maxs.Init();
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pfMins - 
//			pfMaxs - 
//-----------------------------------------------------------------------------
CMapAlignedBox::CMapAlignedBox(Vector &Mins, Vector &Maxs)
{
	m_bUseKeyName = false;
	m_bWireframe = false;

	m_MinsKeyName[0] = '\0';
	m_MaxsKeyName[0] = '\0';

	m_Mins = Mins;
	m_Maxs = Maxs;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : *pMinsKeyName - 
//			*pMaxsKeyName - 
//-----------------------------------------------------------------------------
CMapAlignedBox::CMapAlignedBox(const char *pMinsKeyName, const char *pMaxsKeyName)
{
	m_bUseKeyName = true;
	m_bWireframe = false;
	
	V_strcpy_safe(m_MinsKeyName, pMinsKeyName);
	V_strcpy_safe(m_MaxsKeyName, pMaxsKeyName);
}


//-----------------------------------------------------------------------------
// Purpose: 
//-----------------------------------------------------------------------------
CMapAlignedBox::~CMapAlignedBox(void)
{
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : bFullUpdate - 
//-----------------------------------------------------------------------------
void CMapAlignedBox::CalcBounds(BOOL bFullUpdate)
{
	Vector AbsMins = m_Origin + m_Mins;
	Vector AbsMaxs = m_Origin + m_Maxs;

	m_Render2DBox.ResetBounds();
	m_Render2DBox.UpdateBounds(AbsMins, AbsMaxs);

	m_CullBox = m_Render2DBox;
	m_BoundingBox = m_Render2DBox;
}


//-----------------------------------------------------------------------------
// Purpose: 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapAlignedBox::Copy(bool bUpdateDependencies)
{
	CMapAlignedBox *pCopy = new CMapAlignedBox;

	if (pCopy != NULL)
	{
		pCopy->CopyFrom(this, bUpdateDependencies);
	}

	return(pCopy);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pObject - 
// Output : CMapClass
//-----------------------------------------------------------------------------
CMapClass *CMapAlignedBox::CopyFrom(CMapClass *pObject, bool bUpdateDependencies)
{
	Assert(pObject->IsMapClass(MAPCLASS_TYPE(CMapAlignedBox)));
	CMapAlignedBox *pFrom = (CMapAlignedBox *)pObject;

	CMapClass::CopyFrom(pObject, bUpdateDependencies);

	m_bUseKeyName = pFrom->m_bUseKeyName;
	m_bWireframe = pFrom->m_bWireframe;

	V_strcpy_safe(m_MinsKeyName, pFrom->m_MinsKeyName);
	V_strcpy_safe(m_MaxsKeyName, pFrom->m_MaxsKeyName);

	m_Mins = pFrom->m_Mins;
	m_Maxs = pFrom->m_Maxs;

	return(this);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapAlignedBox::Render2D(CRender2D *pRender)
{
	Vector vecMins, vecMaxs;
	GetRender2DBox(vecMins, vecMaxs);

 	if (!IsSelected())
	{
 	    pRender->SetDrawColor( r, g, b);
		pRender->SetHandleColor( r, g, b);
	}
	else
	{
	    pRender->SetDrawColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
		pRender->SetHandleColor( GetRValue(Options.colors.clrSelection), GetGValue(Options.colors.clrSelection), GetBValue(Options.colors.clrSelection) );
	}

	// Draw the bounding box.
	pRender->DrawBox( vecMins, vecMaxs );

	//
	// Draw center handle.
	//

	if ( pRender->IsActiveView() )
	{
		Vector2D pt, pt2;
		pRender->TransformPoint(pt, vecMins);
		pRender->TransformPoint(pt2, vecMaxs);

		int sizex = abs(pt.x - pt2.x)+1;
		int sizey = abs(pt.y - pt2.y)+1;
		
		// dont draw handle if object is too small
		if ( sizex > 6 && sizey > 6 )
		{
			pRender->SetHandleStyle( HANDLE_RADIUS, CRender::HANDLE_CROSS );
			pRender->DrawHandle( (vecMins+vecMaxs)/2 );
		}
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : pRender - 
//-----------------------------------------------------------------------------
void CMapAlignedBox::Render3D(CRender3D *pRender)
{
	if (!m_bWireframe)
	{
		pRender->BeginRenderHitTarget(this);
		pRender->RenderBox(m_CullBox.bmins, m_CullBox.bmaxs, r, g, b, GetSelectionState());
		pRender->EndRenderHitTarget();
	}
	else if (GetSelectionState() != SELECT_NONE)
	{
		pRender->RenderWireframeBox(m_CullBox.bmins, m_CullBox.bmaxs, 255, 255, 0);
	}
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapAlignedBox::SerializeRMF(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : File - 
//			bRMF - 
// Output : int
//-----------------------------------------------------------------------------
int CMapAlignedBox::SerializeMAP(std::fstream &File, BOOL bRMF)
{
	return(0);
}


//-----------------------------------------------------------------------------
// Purpose: 
// Input  : key - 
//			value - 
//-----------------------------------------------------------------------------
void CMapAlignedBox::OnParentKeyChanged( const char* key, const char* value )
{
	if (!m_bUseKeyName)
	{
		return;
	}

	if (stricmp(key, m_MinsKeyName) == 0)
	{
		sscanf(value, "%f %f %f", &m_Mins[0], &m_Mins[1], &m_Mins[2]);
	}
	else if (stricmp(key, m_MaxsKeyName) == 0)
	{
		sscanf(value, "%f %f %f", &m_Maxs[0], &m_Maxs[1], &m_Maxs[2]);
	}

	PostUpdate(Notify_Changed);
}




