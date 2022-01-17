#include "StdAfx.h"
#include "MPAEndOfFileException.h"

CMPAEndOfFileException::CMPAEndOfFileException(LPCTSTR szFile) :
	CMPAException(CMPAException::EndOfFile, szFile)
{
}

CMPAEndOfFileException::~CMPAEndOfFileException(void)
{
}
