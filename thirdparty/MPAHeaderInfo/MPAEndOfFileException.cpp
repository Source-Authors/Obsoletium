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

#include "MPAEndOfFileException.h"

CMPAEndOfFileException::CMPAEndOfFileException(LPCTSTR szFile)
    : CMPAException{CMPAException::ErrorIDs::EndOfFile, szFile} {}

CMPAEndOfFileException::~CMPAEndOfFileException() {}
