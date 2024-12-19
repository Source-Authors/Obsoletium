// Copyright Valve Corporation, All rights reserved.
//
// Compile process window.

#include "stdafx.h"
#include "ProcessWnd.h"

#include "hammer.h"
#include <wincon.h>

// memdbgon must be the last include file in a .cpp file!!!
#include <tier0/memdbgon.h>


#define IDC_PROCESSWND_EDIT    1
#define IDC_PROCESSWND_COPYALL 2


LPCTSTR GetErrorString();


CProcessWnd::CProcessWnd() : pEditBuf{nullptr}, uBufLen{0}, pFont{nullptr}
{
	Font.CreatePointFont(100, "Courier New");
}

CProcessWnd::~CProcessWnd()
{
}


BEGIN_MESSAGE_MAP(CProcessWnd, CWnd)
	ON_BN_CLICKED(IDC_PROCESSWND_COPYALL, OnCopyAll)
	//{{AFX_MSG_MAP(CProcessWnd)
	ON_BN_CLICKED(IDC_PROCESSWND_COPYALL, OnCopyAll)
	ON_WM_TIMER()
	ON_WM_CREATE()
	ON_WM_SIZE()
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CProcessWnd operations

int CProcessWnd::Execute(PRINTF_FORMAT_STRING LPCTSTR pszCmd, ...)
{
    CString strBuf;

	va_list vl;
	va_start(vl, pszCmd);

	while (true)
	{
		char *p = va_arg(vl, char*);
		if(!p)
			break;

		strBuf += p;
		strBuf += " ";
	}

	va_end(vl);

	return Execute(pszCmd, strBuf.GetString());
}

void CProcessWnd::Clear()
{
	m_EditText.Empty();
	Edit.SetWindowText("");
	Edit.RedrawWindow();
}

void CProcessWnd::Append(CString str)
{
    m_EditText += str;
	Edit.SetWindowText(m_EditText);
    Edit.LineScroll(Edit.GetLineCount());
	Edit.RedrawWindow();
}

int CProcessWnd::Execute(LPCTSTR pszCmd, LPCTSTR pszCmdLine)
{
	int rval = -1;
    SECURITY_ATTRIBUTES saAttr; 
	HANDLE hChildStdinRd_, hChildStdinWr, hChildStdoutRd_, hChildStdoutWr, hChildStderrWr; 

    // Set the bInheritHandle flag so pipe handles are inherited.
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 
 
    // Create a pipe for the child's STDOUT. 
    if(CreatePipe(&hChildStdoutRd_, &hChildStdoutWr, &saAttr, 0))
	{
		if(CreatePipe(&hChildStdinRd_, &hChildStdinWr, &saAttr, 0))
		{
			if (DuplicateHandle(GetCurrentProcess(),hChildStdoutWr, GetCurrentProcess(),&hChildStderrWr,0, TRUE,DUPLICATE_SAME_ACCESS))
			{
				/* Now create the child process. */ 
				STARTUPINFO si;
				memset(&si, 0, sizeof si);
				si.cb = sizeof(si);
				si.dwFlags = STARTF_USESTDHANDLES;
				si.hStdInput = hChildStdinRd_;
				si.hStdError = hChildStderrWr;
				si.hStdOutput = hChildStdoutWr;
				PROCESS_INFORMATION pi;
				CString str;
				str.Format("%s %s", pszCmd, pszCmdLine);
				if (CreateProcess(NULL, (char*) LPCTSTR(str), NULL, NULL, TRUE, 
					DETACHED_PROCESS, NULL, NULL, &si, &pi))
				{
					HANDLE hProcess = pi.hProcess;
					
#define BUFFER_SIZE 4096
					// read from pipe..
					char buffer[BUFFER_SIZE];
					BOOL bDone = FALSE;
					
					while(1)
					{
						DWORD dwCount = 0;
						DWORD dwRead = 0;
						
						// read from input handle
						PeekNamedPipe( hChildStdoutRd_, NULL, NULL, NULL, &dwCount, NULL);
						if (dwCount)
						{
							dwCount = min (dwCount, (DWORD)BUFFER_SIZE - 1);
							ReadFile( hChildStdoutRd_, buffer, dwCount, &dwRead, NULL);
						}
						if(dwRead)
						{
							buffer[dwRead] = 0;
							Append(buffer);
						}
						// check process termination
						else if(WaitForSingleObject(hProcess, 1000) != WAIT_TIMEOUT)
						{
							if(bDone)
								break;
							bDone = TRUE;	// next time we get it
						}
					}
					rval = 0;
				}
				else
				{
					SetForegroundWindow();
					CString strTmp;
					strTmp.Format("* Could not execute the command:\r\n   %s\r\n", str.GetBuffer());
					Append(strTmp);
					strTmp.Format("* Windows gave the error message:\r\n   \"%s\"\r\n", GetErrorString());
					Append(strTmp);
				}
				
				CloseHandle(hChildStderrWr);
			}
			CloseHandle(hChildStdinRd_);
			CloseHandle(hChildStdinWr);
		}
		CloseHandle(hChildStdoutRd_);
		CloseHandle(hChildStdoutWr);
	}

	return rval;
}

/////////////////////////////////////////////////////////////////////////////
// CProcessWnd message handlers


void CProcessWnd::OnTimer(UINT_PTR nIDEvent) 
{
	__super::OnTimer(nIDEvent);
}

int CProcessWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	// create big CEdit in window
	CRect rctClient;
	GetClientRect(rctClient);

	CRect rctEdit;
	rctEdit = rctClient;
	rctEdit.bottom = rctClient.bottom - 20;

	Edit.Create(WS_CHILD | WS_BORDER | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, rctClient, this, IDC_PROCESSWND_EDIT);
	Edit.SetReadOnly(TRUE);
	Edit.SetFont(&Font);

	CRect rctButton;
	rctButton = rctClient;
	rctButton.top = rctClient.bottom - 20;

	m_btnCopyAll.Create("Copy to Clipboard", WS_CHILD | WS_VISIBLE, rctButton, this, IDC_PROCESSWND_COPYALL);
	m_btnCopyAll.SetButtonStyle(BS_PUSHBUTTON);
	
	return 0;
}

void CProcessWnd::OnSize(UINT nType, int cx, int cy) 
{
	__super::OnSize(nType, cx, cy);

	// create big CEdit in window
	CRect rctClient;
	GetClientRect(rctClient);

	CRect rctEdit;
	rctEdit = rctClient;
	rctEdit.bottom = rctClient.bottom - 20;
	Edit.MoveWindow(rctEdit);

	CRect rctButton;
	rctButton = rctClient;
	rctButton.top = rctClient.bottom - 20;
	m_btnCopyAll.MoveWindow(rctButton);
}


//-----------------------------------------------------------------------------
// Purpose: Prepare the process window for display. If it has not been created
//			yet, register the class and create it.
//-----------------------------------------------------------------------------
void CProcessWnd::GetReady(LPCTSTR pszDocName)
{
	if (!IsWindow(m_hWnd))
	{
		CString strClass = AfxRegisterWndClass(0, AfxGetApp()->LoadStandardCursor(IDC_ARROW), HBRUSH(GetStockObject(WHITE_BRUSH)));
		CString title;
		// dimhotepus: Add compiling map name to title.
		title.Format("Compile - [%s]", pszDocName);
		CreateEx(0, strClass, title.GetString(), WS_OVERLAPPEDWINDOW, 50, 50, 600, 400, AfxGetMainWnd()->GetSafeHwnd(), nullptr);
	}

	ShowWindow(SW_SHOW);
	SetActiveWindow();
	Clear();
}

BOOL CProcessWnd::PreTranslateMessage(MSG* pMsg) 
{
	// The edit control won't get keyboard commands from the window without this (at least in Win2k)
	// The right mouse context menu still will not work in w2k for some reason either, although
	// it is getting the CONTEXTMENU message (as seen in Spy++)
	::TranslateMessage(pMsg);
	::DispatchMessage(pMsg);

	return TRUE;
}

static void CopyToClipboard(const CString& text)
{
	if (::OpenClipboard(NULL))
	{
		if (::EmptyClipboard())
		{
			HGLOBAL hglbCopy = ::GlobalAlloc(GMEM_DDESHARE, text.GetLength() + sizeof(TCHAR) );
			if (hglbCopy)
			{
				LPTSTR tstrCopy = static_cast<LPTSTR>(::GlobalLock(hglbCopy));
				if (tstrCopy)
				{
					strcpy(tstrCopy, text.GetString());
				}

				::GlobalUnlock(hglbCopy);
				::SetClipboardData(CF_TEXT, hglbCopy);
			}
		}
		::CloseClipboard();
	}
}

void CProcessWnd::OnCopyAll()
{
	// Used to call m_Edit.SetSel(0,1); m_Edit.Copy(); m_Edit.Clear()
	// but in win9x the clipboard will only receive at most 64k of text from the control
	CopyToClipboard(m_EditText);
}
