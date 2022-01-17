#pragma once
#include "mpastream.h"

class CMPAFileStream :
	public CMPAStream
{
public:
	CMPAFileStream(LPCTSTR szFilename);
	CMPAFileStream(LPCTSTR szFilename, HANDLE hFile);
	virtual ~CMPAFileStream(void);

private:
	static const DWORD INIT_BUFFERSIZE;
	HANDLE m_hFile;
	bool m_bMustReleaseFile;

	// concerning read-buffer
	mutable BYTE* m_pBuffer;
	mutable DWORD m_dwOffset;	// offset (beginning if m_pBuffer)
	mutable DWORD m_dwBufferSize;

	void Init();
	DWORD Read(LPVOID pData, DWORD dwOffset, DWORD dwSize) const;
	bool FillBuffer(DWORD dwOffset, DWORD dwSize, bool bReverse) const;
	void SetPosition(DWORD dwOffset) const;

protected:
	// methods for file access (must be implemented by all derived classes)
	
	virtual BYTE* ReadBytes(DWORD dwSize, DWORD& dwOffset, bool bMoveOffset = true, bool bReverse = false) const;
	virtual DWORD GetSize() const;
};
