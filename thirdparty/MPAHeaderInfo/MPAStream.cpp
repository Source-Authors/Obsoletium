// GNU LESSER GENERAL PUBLIC LICENSE
// Version 3, 29 June 2007
//
// Copyright (C) 2007 Free Software Foundation, Inc. <http://fsf.org/>
//
// Everyone is permitted to copy and distribute verbatim copies of this license
// document, but changing it is not allowed.
//
// This version of the GNU Lesser General Public License incorporates the terms
// and conditions of version 3 of the GNU General Public License, supplemented
// by the additional permissions listed below.

#include "stdafx.h"

#include "MPAStream.h"
#include "MPAException.h"

// constructor
CMPAStream::CMPAStream(LPCTSTR file_name) {
  assert(file_name);

  // save filename
  m_szFile = _tcsdup(file_name);
}

CMPAStream::~CMPAStream() { free(m_szFile); }

unsigned CMPAStream::ReadLEValue(unsigned num_bytes, unsigned& offset,
                                 bool move_offset) const {
  assert(num_bytes > 0);
  assert(num_bytes <= 4);  // max 4 byte

  const unsigned char* buffer{ReadBytes(num_bytes, offset, move_offset)};

  // little endian extract (least significant byte first) (will work on little
  // and big-endian computers)
  unsigned num_bytes_shifts = 0U;
  unsigned result = 0U;

  for (unsigned n = 0; n < num_bytes; n++) {
    result |= buffer[n] << 8U * num_bytes_shifts++;
  }

  return result;
}

// convert from big endian to native format (Intel=little endian) and return as
// unsigned (32bit)
unsigned CMPAStream::ReadBEValue(unsigned num_bytes, unsigned& offset,
                                 bool move_offset) const {
  assert(num_bytes > 0);
  assert(num_bytes <= 4U);  // max 4 byte

  const unsigned char* buffer{ReadBytes(num_bytes, offset, move_offset)};

  // big endian extract (most significant byte first) (will work on little and
  // big-endian computers)
  unsigned num_bytes_shifts{num_bytes - 1U};
  unsigned result = 0;

  for (unsigned n = 0; n < num_bytes; n++) {
    // the bit shift will do the correct byte order for you
    result |= buffer[n] << 8U * num_bytes_shifts--;
  }

  return result;
}