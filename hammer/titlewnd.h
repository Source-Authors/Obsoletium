//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $Workfile:     $
// $Date:         $
//
//-----------------------------------------------------------------------------
// $Log: $
//
// $NoKeywords: $
//=============================================================================//

#ifndef TITLEWND_H
#define TITLEWND_H
#pragma once

#include "windows/base_wnd.h"
#include "windows/dpi_aware_font.h"

class CTitleWnd : public CBaseWnd
{
	public:

		static CTitleWnd *CreateTitleWnd(CWnd *pwndParent, UINT uID);

		void SetTitle(LPCTSTR pszTitle);

	private:

		char m_szTitle[256];
		// dimhotepus: No need for DPI aware as they do not use DPI features.
		static CFont m_FontNormal;
		static CFont m_FontActive;

	protected:

		CTitleWnd(void);

		void OnMouseButton(void);

		bool m_bMouseOver;
		bool m_bMenuOpen;

		afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
		afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
		afx_msg void OnPaint();
		afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
		afx_msg void OnMouseMove(UINT nFlags, CPoint point);
		afx_msg LRESULT OnMouseLeave(WPARAM wParam, LPARAM lParam);

		void CreateDpiDependentResources();

		DECLARE_MESSAGE_MAP()
};



#endif // TITLEWND_H
