#pragma once
#include "mpaexception.h"
#include "mpafilestream.h"
#include "mpaframe.h"
#include "tags.h"

class CMPAFile
{
public:
	CMPAFile(LPCTSTR szFile);
	CMPAFile(CMPAStream* pStream);
	~CMPAFile(void);

	DWORD GetBegin() const { return m_pTags->GetBegin(); };
	DWORD GetEnd() const { return m_pTags->GetEnd(); };
	DWORD GetFileSize() const { return (GetEnd() - GetBegin()); };
	DWORD GetLengthSec() const { return (GetFileSize() / m_dwBytesPerSec); };
	
	enum GetType
	{
		First,
		Last,
		Next,
		Prev,
		Resync
	};

	CMPAFrame* GetFrame(GetType Type, CMPAFrame* pFrame = NULL, bool bDeleteOldFrame = true, DWORD dwOffset = 0);
	
private:
	void CalcBytesPerSec();

	CMPAStream* m_pStream;
	DWORD m_dwBytesPerSec;
	
public:	
	CTags* m_pTags;					// contain list of present tags
	CMPAFrame* m_pFirstFrame;	// always first frame
	CVBRHeader* m_pVBRHeader;		// XING or VBRI or NULL
};
