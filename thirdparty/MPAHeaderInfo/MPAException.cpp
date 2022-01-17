#include "stdafx.h"
#include ".\mpaexception.h"

/// CMPAException: exception class
//////////////////////////////////////////////

CMPAException::CMPAException(ErrorIDs ErrorID, LPCTSTR szFile, LPCTSTR szFunction, bool bGetLastError) :
	m_ErrorID(ErrorID), m_bGetLastError(bGetLastError), m_szErrorMsg(NULL)
{
	m_szFile = _tcsdup(szFile);
	m_szFunction = _tcsdup(szFunction);
}

// copy constructor (necessary for exception throwing without pointers)
CMPAException::CMPAException(const CMPAException& Source)
{
	m_ErrorID = Source.m_ErrorID;
	m_bGetLastError = Source.m_bGetLastError;
	m_szFile = _tcsdup(Source.m_szFile);
	m_szFunction = _tcsdup(Source.m_szFunction);
}

// destructor
CMPAException::~CMPAException()
{
	if (m_szFile)
		free(m_szFile);
	if (m_szFunction)
		free(m_szFunction);
	if (m_szErrorMsg)
		delete[] m_szErrorMsg;
}

// should be in resource file for multi language applications
LPCTSTR CMPAException::m_szErrors[CMPAException::NumIDs] = 
{
	_T("Can't open the file."), 
	_T("Can't set file position."),
	_T("Can't read from file."),
	_T("No VBR Header found."),
	_T("Incomplete VBR Header."),
	_T("No frame found within tolerance range."),
	_T("No frame found before end of file was reached."),
	_T("Header corrupt"),
	_T("Free Bitrate currently not supported"),
	_T("Incompatible Header"),
	_T("Corrupt Lyrics3 Tag")
};

#define MAX_ERR_LENGTH 256
void CMPAException::ShowError()
{
	LPCTSTR pErrorMsg = GetErrorDescription();
	// show error message
	::MessageBox (NULL, pErrorMsg, _T("MPAFile Error"), MB_OK);
}

LPCTSTR CMPAException::GetErrorDescription()
{
	if (!m_szErrorMsg)
	{
		m_szErrorMsg = new TCHAR[MAX_ERR_LENGTH];
		m_szErrorMsg[0] = '\0';
		TCHAR szHelp[MAX_ERR_LENGTH];

		// this is not buffer-overflow-proof!
		if (m_szFunction)
		{
			_stprintf_s(szHelp, MAX_ERR_LENGTH, _T("%s: "), m_szFunction);
			_tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, szHelp );
		}
		if (m_szFile)
		{
			_stprintf_s(szHelp, MAX_ERR_LENGTH, _T("'%s'\n"), m_szFile);
			_tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, szHelp);
		}
		_tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, m_szErrors[m_ErrorID]);

		if (m_bGetLastError)
		{
			// get error message of last system error id
			LPVOID pMsgBuf;
			if (FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
								NULL,
								GetLastError(),
								MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
								(LPTSTR) &pMsgBuf,
								0,
								NULL))
			{
				_tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, _T("\n"));
				_tcscat_s(m_szErrorMsg, MAX_ERR_LENGTH, (LPCTSTR)pMsgBuf);
				LocalFree(pMsgBuf);
			}
		}

		// make sure string is null-terminated
		m_szErrorMsg[MAX_ERR_LENGTH-1] = '\0';
	}
	return m_szErrorMsg;
}