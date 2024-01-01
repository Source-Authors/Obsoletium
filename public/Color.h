//========= Copyright Valve Corporation, All rights reserved. ============//
//
// Purpose: 
//
// $NoKeywords: $
//=============================================================================//

#ifndef COLOR_H
#define COLOR_H

#include "tier0/dbg.h"

#ifdef _WIN32
#pragma once
#endif

//-----------------------------------------------------------------------------
// Purpose: Basic handler for an rgb set of colors
//			This class is fully inline
//-----------------------------------------------------------------------------
class alignas(int) Color
{
	using component = unsigned char;
public:
	// constructors
	Color() : Color{0,0,0}
	{
	}
	Color(component r,component g,component b) : Color{r,g,b,0}
	{
	}
	Color(component r,component g,component b,component a)
	{
		SetColor(r, g, b, a);
	}
	
	// set the color
	// r - red component (0-255)
	// g - green component (0-255)
	// b - blue component (0-255)
	// a - alpha component, controls transparency (0 - transparent, 255 - opaque);
	void SetColor(component _r, component _g, component _b, component _a = 0)
	{
		_color[0] = _r;
		_color[1] = _g;
		_color[2] = _b;
		_color[3] = _a;
	}

	void GetColor(int &_r, int &_g, int &_b, int &_a) const
	{
		_r = _color[0];
		_g = _color[1];
		_b = _color[2];
		_a = _color[3];
	}

	void SetRawColor( int color32 )
	{
		*((int *)this) = color32;
	}

	int GetRawColor() const
	{
		return *((int *)this);
	}

	inline int r() const	{ return _color[0]; }
	inline int g() const	{ return _color[1]; }
	inline int b() const	{ return _color[2]; }
	inline int a() const	{ return _color[3]; }
	
	component &operator[](size_t index)
	{
		Assert( index <= std::size(_color) );
		return _color[index];
	}

	const component &operator[](size_t index) const
	{
		Assert( index <= std::size(_color) );
		return _color[index];
	}

	bool operator ==(const Color &rhs) const
	{
		return GetRawColor() == rhs.GetRawColor();
	}

	bool operator !=(const Color &rhs) const
	{
		return !(*this == rhs);
	}

	Color( const Color &rhs )
	{
		SetRawColor( rhs.GetRawColor() );
	}

	Color &operator=( const Color &rhs )
	{
		SetRawColor( rhs.GetRawColor() );
		return *this;
	}

private:
	component _color[4]; //-V112
};


#endif // COLOR_H
