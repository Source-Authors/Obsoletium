// Copyright Valve Corporation, All rights reserved.

#ifndef VPC_COLOR_H_
#define VPC_COLOR_H_

#include "tier0/basetypes.h"

// Purpose: Basic handler for an rgb set of colors
class alignas(int) Color {
 public:
  // constructors
  constexpr Color() : _color{0, 0, 0, 0} {}
  Color(int _r, int _g, int _b) { SetColor(_r, _g, _b, 0); }
  Color(int _r, int _g, int _b, int _a) { SetColor(_r, _g, _b, _a); }
  constexpr Color(const Color &rhs)
      : _color{rhs._color[0], rhs._color[1], rhs._color[2], rhs._color[3]} {}

  // set the color
  // r - red component (0-255)
  // g - green component (0-255)
  // b - blue component (0-255)
  // a - alpha component, controls transparency (0 - transparent, 255 - opaque);
  void SetColor(int _r, int _g, int _b, int _a = 0) {
    _color[0] = (unsigned char)_r;
    _color[1] = (unsigned char)_g;
    _color[2] = (unsigned char)_b;
    _color[3] = (unsigned char)_a;
  }

  void GetColor(int &_r, int &_g, int &_b, int &_a) const {
    _r = _color[0];
    _g = _color[1];
    _b = _color[2];
    _a = _color[3];
  }

  void SetRawColor(int color32) { *((int *)this) = color32; }

  int GetRawColor() const { return *((int *)this); }

  constexpr int r() const { return _color[0]; }
  constexpr int g() const { return _color[1]; }
  constexpr int b() const { return _color[2]; }
  constexpr int a() const { return _color[3]; }

  unsigned char &operator[](int index) { return _color[index]; }

  const unsigned char &operator[](int index) const { return _color[index]; }

  constexpr bool operator==(const Color &rhs) const {
    return _color[0] == rhs._color[0] && _color[1] == rhs._color[1] &&
           _color[2] == rhs._color[2] && _color[3] == rhs._color[3];
  }

  constexpr bool operator!=(const Color &rhs) const {
    return !(operator==(rhs));
  }

  constexpr Color &operator=(const Color &rhs) {
    _color[0] = rhs._color[0];
    _color[1] = rhs._color[1];
    _color[2] = rhs._color[2];
    _color[3] = rhs._color[3];
    return *this;
  }

  Color &operator=(const color32 &rhs) {
    _color[0] = rhs.r;
    _color[1] = rhs.g;
    _color[2] = rhs.b;
    _color[3] = rhs.a;
    return *this;
  }

  color32 ToColor32() const {
    return {_color[0], _color[1], _color[2], _color[3]};
  }

 private:
  unsigned char _color[4];
};

#endif  // VPC_COLOR_H_
