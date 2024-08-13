//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef IMAGE_H
#define IMAGE_H

#ifdef _WIN32
#pragma once
#endif

#include <vgui/VGUI.h>
#include <Color.h>
#include <vgui/IImage.h>

namespace vgui
{

class Panel;

//-----------------------------------------------------------------------------
// Purpose: Basic image control
//-----------------------------------------------------------------------------
class Image : public IImage
{
public:
	Image();
	virtual ~Image();

	// Set the position of the image
	void SetPos( int x, int y ) override;
	// Get the position of the image
	virtual void GetPos( int &x, int &y );
	// Get the size of the image
	void GetSize( int &wide, int &tall ) override;
	void GetContentSize( int &wide, int &tall ) override;
	// Set the draw color 
	void SetColor( Color color ) override;
	// set the background color
	virtual void SetBkColor( Color color ) { DrawSetColor( color ); }
	// Get the draw color 
	virtual Color GetColor();
	bool Evict() override;
	int GetNumFrames() override;
	void SetFrame( int nFrame ) override;
	HTexture GetID() override;
	void SetRotation( int ) override { return; };

protected:
	void SetSize(int wide, int tall) override;
	virtual void DrawSetColor(Color color);
	virtual void DrawSetColor(int r, int g, int b, int a);
	virtual void DrawFilledRect(int x0, int y0, int x1, int y1);
	virtual void DrawOutlinedRect(int x0, int y0, int x1, int y1);
	virtual void DrawLine(int x0,int y0,int x1,int y1);
	virtual void DrawPolyLine(int *px, int *py, int numPoints);
	virtual void DrawSetTextFont(HFont font);
	virtual void DrawSetTextColor(Color color);
	virtual void DrawSetTextColor(int r, int g, int b, int a);
	virtual void DrawSetTextPos(int x,int y);
	virtual void DrawPrintText(const wchar_t *str, int strlen);
	virtual void DrawPrintText(int x, int y, const wchar_t *str, int strlen);
	virtual void DrawPrintChar(wchar_t ch);
	virtual void DrawPrintChar(int x, int y, wchar_t ch);
	virtual void DrawSetTexture(int id);
	virtual void DrawTexturedRect(int x0, int y0, int x1, int y1);
	virtual void Paint() = 0;

private:
	int _pos[2];
	int _size[2];
	Color _color;
};

} // namespace vgui

#endif // IMAGE_H
