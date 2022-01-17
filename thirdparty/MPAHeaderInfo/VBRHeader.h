#pragma once

// class CMPAFrame must be included first and must be known here
class CMPAFrame;

class CVBRHeader
{
public:
	static CVBRHeader* FindHeader(const CMPAFrame* pFrame);
	virtual ~CVBRHeader(void);
	bool SeekPosition(float& fPercent, DWORD& dwSeekPoint) const;

	DWORD m_dwBytes;		// total number of bytes
	DWORD m_dwFrames;		// total number of frames

protected:	
	CVBRHeader(CMPAStream* pStream, DWORD dwOffset);

	static bool CheckID(CMPAStream* pStream, DWORD dwOffset, char ch0, char ch1, char ch2, char ch3);
	virtual DWORD SeekPosition(float& fPercent) const = 0;
	CMPAStream* m_pStream;

public:	
	DWORD m_dwOffset;
	DWORD m_dwQuality;		// quality (0..100)
	int* m_pnToc;			// TOC points for seeking (must be freed)
	DWORD m_dwTableSize;	// size of table (number of entries)	


	
};
