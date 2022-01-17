#pragma once

class CMPAStream
{
public:

	virtual ~CMPAStream(void);

	virtual DWORD GetSize() const = 0;
	virtual BYTE* ReadBytes(DWORD dwSize, DWORD& dwOffset, bool bMoveOffset = true, bool bReverse = false) const = 0;
	
	DWORD ReadBEValue(DWORD dwNumBytes, DWORD& dwOffset, bool bMoveOffset = true) const;
	DWORD ReadLEValue(DWORD dwNumBytes, DWORD& dwOffset, bool bMoveOffset = true) const;
	LPCTSTR GetFilename() const { return m_szFile; };

protected:
	LPTSTR m_szFile;

	CMPAStream(LPCTSTR szFilename);
};
