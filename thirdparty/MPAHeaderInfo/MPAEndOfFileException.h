#pragma once
#include "mpaexception.h"

class CMPAEndOfFileException :
	public CMPAException
{
public:
	CMPAEndOfFileException(LPCTSTR szFile);
	virtual ~CMPAEndOfFileException(void);
};
