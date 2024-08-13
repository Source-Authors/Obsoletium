// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_COLOR_H_
#define SRC_COLOR_H_

#include <utility>
#include "tier0/dbg.h"

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
	Color(int r,int g,int b) : Color{r,g,b,0}
	{
	}
	Color(int r,int g,int b,int a)
	{
		SetColor(r, g, b, a);
	}
	
	// set the color
	// r - red component (0-255)
	// g - green component (0-255)
	// b - blue component (0-255)
	// a - alpha component, controls transparency (0 - transparent, 255 - opaque);
	void SetColor(int _r, int _g, int _b, int _a = 0)
	{
		Assert( _r >= 0 && _r <= 255 &&
				_g >= 0 && _g <= 255 &&
				_b >= 0 && _b <= 255 &&
				_a >= 0 && _a <= 255
		);

		_color[0] = static_cast<component>( max( min( _r, 255 ), 0 ) );
		_color[1] = static_cast<component>( max( min( _g, 255 ), 0 ) );
		_color[2] = static_cast<component>( max( min( _b, 255 ), 0 ) );
		_color[3] = static_cast<component>( max( min( _a, 255 ), 0 ) );
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
		std::memcpy( this, &color32, sizeof(*this) );
	}

	int GetRawColor() const
	{
		int raw;
		std::memcpy( &raw, this, sizeof(raw) );
		return raw;
	}

	inline int r() const	{ return _color[0]; }
	inline int g() const	{ return _color[1]; }
	inline int b() const	{ return _color[2]; }
	inline int a() const	{ return _color[3]; }
	
	component &operator[](size_t index)
	{
		Assert( index < std::size(_color) );
		return _color[index];
	}

	const component &operator[](size_t index) const
	{
		Assert( index < std::size(_color) );
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

struct color24
{
	byte r, g, b;
};

struct color32_s
{
	[[nodiscard]] constexpr bool operator!=( color32_s other ) const
	{
		return !(*this == other);
	}

	[[nodiscard]] constexpr bool operator==( color32_s other ) const
	{
		return r == other.r && g == other.g && b == other.b && a == other.a;
	}

	byte r, g, b, a;
};

using color32 = color32_s;

struct colorVec
{
	unsigned r, g, b, a;
};

#endif  // !SRC_COLOR_H_
