#pragma once

// exception class
class CMPAException
{
public:
	
	enum ErrorIDs
	{
		ErrOpenFile,
		ErrSetPosition,
		ErrReadFile,
		NoVBRHeader,
		IncompleteVBRHeader,
		NoFrameInTolerance,
		EndOfFile,
		HeaderCorrupt,
		FreeBitrate,
		IncompatibleHeader,
		CorruptLyricsTag,
		NumIDs			// this specifies the total number of possible IDs
	};

	CMPAException(ErrorIDs ErrorID, LPCTSTR szFile = NULL, LPCTSTR szFunction = NULL, bool bGetLastError=false);
	// copy constructor (necessary because of LPSTR members)
	CMPAException(const CMPAException& Source);
	virtual ~CMPAException(void);

	ErrorIDs GetErrorID() const { return m_ErrorID; };
	LPCTSTR GetErrorDescription();
	void ShowError();

private:
	ErrorIDs m_ErrorID;
	bool m_bGetLastError;
	LPTSTR m_szFunction;
	LPTSTR m_szFile;
	LPTSTR m_szErrorMsg;

	static LPCTSTR m_szErrors[CMPAException::NumIDs];
};
