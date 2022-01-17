#pragma once
#include "mpastream.h"
#include "mpaheader.h"
#include "vbrheader.h"
#include "vbriheader.h"
#include "xingheader.h"

class CMPAFrame
{
public:
	CMPAFrame(CMPAStream* pStream, DWORD& dwOffset, bool bFindSubsequentFrame, bool bExactOffset, bool bReverse, CMPAHeader* pCompareHeader);
	~CMPAFrame(void);

	CVBRHeader* FindVBRHeader() const;
	
	DWORD GetSubsequentHeaderOffset() const { return m_dwOffset + m_dwFrameSize; };
	bool CheckCRC() const;
	bool IsLast() const { return m_bIsLast; };
	
public:
	CMPAHeader* m_pHeader;
	CMPAStream* m_pStream;

	DWORD m_dwOffset;	// offset in bytes where frame begins
	DWORD m_dwFrameSize;// calculated frame size
	
private:
	static const DWORD m_dwProtectedBitsLayer2[5][2];
	static WORD CalcCRC16(BYTE* pBuffer, DWORD dwSize);
	bool m_bIsLast;		// true, if it is last frame
};
