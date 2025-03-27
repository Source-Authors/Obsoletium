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
}

CProcessWnd::~CProcessWnd()
{
	delete pFont;
}


BEGIN_MESSAGE_MAP(CProcessWnd, CBaseWnd)
	//{{AFX_MSG_MAP(CProcessWnd)
	ON_BN_CLICKED(IDC_PROCESSWND_COPYALL, OnCopyAll)
	ON_WM_CREATE()
	ON_WM_SIZE()
	ON_MESSAGE(WM_DPICHANGED, OnDpiChanged)
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

	return Execute(pszCmd, strBuf);
}

void CProcessWnd::Clear()
{
	m_EditText.Empty();
	Edit.SetWindowText("");
	Edit.RedrawWindow();
}

void CProcessWnd::Append(CString str)
{
	// dimhotepus: Normalize output as ASAN reports for example dumps \n only but edit needs \r\n.
	str.Replace("\r\n", "\n");
	str.Replace("\n", "\r\n");

    m_EditText += str;
	Edit.SetWindowText(m_EditText);
    Edit.LineScroll(Edit.GetLineCount());
	Edit.RedrawWindow();
}

int CProcessWnd::Execute(LPCTSTR pszCmd, LPCTSTR pszCmdLine)
{
	int rval = -1;
	HANDLE hChildStdinRd_, hChildStdinWr, hChildStdoutRd_, hChildStdoutWr, hChildStderrWr; 

    // Set the bInheritHandle flag so pipe handles are inherited.
    SECURITY_ATTRIBUTES saAttr = {}; 
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES); 
    saAttr.bInheritHandle = TRUE; 
    saAttr.lpSecurityDescriptor = NULL; 
 
    // Create a pipe for the child's STDOUT. 
    if (::CreatePipe(&hChildStdoutRd_, &hChildStdoutWr, &saAttr, 0))
	{
		if (::CreatePipe(&hChildStdinRd_, &hChildStdinWr, &saAttr, 0))
		{
			if (::DuplicateHandle
				(
					::GetCurrentProcess(),
					hChildStdoutWr,
					::GetCurrentProcess(),
					&hChildStderrWr,
					0,
					TRUE,
					DUPLICATE_SAME_ACCESS
				))
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
				CString commandLine;
				commandLine.Format("%s %s", pszCmd, pszCmdLine);

				if (::CreateProcess(NULL, commandLine.GetBuffer(), NULL, NULL, TRUE, 
					DETACHED_PROCESS, NULL, NULL, &si, &pi))
				{
					HANDLE hProcess = pi.hProcess;
					
					constexpr DWORD BUFFER_SIZE{4096};
					// read from pipe..
					char buffer[BUFFER_SIZE];

					DWORD rc;
					
					while(1)
					{
						DWORD dwCount = 0, dwRead = 0;
						
						// read from input handle
						if (PeekNamedPipe( hChildStdoutRd_, NULL, NULL, NULL, &dwCount, NULL) && dwCount)
						{
							dwCount = min(dwCount, BUFFER_SIZE - 1);
							if (!ReadFile( hChildStdoutRd_, buffer, dwCount, &dwRead, NULL))
							{
								dwRead = 0;
							}
						}

						if (dwRead)
						{
							buffer[dwRead] = '\0';
							Append(buffer);
						}
						else if (WaitForSingleObject(hProcess, 1000) != WAIT_TIMEOUT)
						{
							// check process termination
							if ( ::GetExitCodeProcess(pi.hProcess, &rc) && rc != STILL_ACTIVE )
								break;
						}
					}

					// dimhotepus: Correctly process exit code for processes.
					rval = rc;

					::CloseHandle(pi.hThread);
					::CloseHandle(pi.hProcess);

					if (rval != 0)
					{
						SetForegroundWindow();

						// dimhotepus: Dump process exit code on failure.
						CString strTmp;
						strTmp.Format("\r\n\r\n** Failure during command execution:\r\n   %s\r\n** Command returned nonsuccess error code:   \"%d\"\r\n", commandLine.GetBuffer(), rval);
						Append(strTmp);
					}
				}
				else
				{
					const char *error{GetErrorString()};

					SetForegroundWindow();

					CString strTmp;
					strTmp.Format("** Could not execute the command:\r\n   %s\r\n** Last error message:\r\n   \"%s\"\r\n", commandLine.GetBuffer(), error);
					Append(strTmp);
				}
				
				::CloseHandle(hChildStderrWr);
			}
			::CloseHandle(hChildStdinRd_);
			::CloseHandle(hChildStdinWr);
		}
		::CloseHandle(hChildStdoutRd_);
		::CloseHandle(hChildStdoutWr);
	}

	return rval;
}

/////////////////////////////////////////////////////////////////////////////
// CProcessWnd message handlers

int CProcessWnd::OnCreate(LPCREATESTRUCT lpCreateStruct) 
{
	if (__super::OnCreate(lpCreateStruct) == -1)
		return -1;

	pFont = CreateFont();

	// create big CEdit in window
	CRect rctClient;
	GetClientRect(rctClient);

	CRect rctEdit = rctClient;
	rctEdit.bottom = rctClient.bottom - m_dpi_behavior.ScaleOnY(20);

	Edit.Create(WS_CHILD | WS_BORDER | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN, rctClient, this, IDC_PROCESSWND_EDIT);
	Edit.SetReadOnly(TRUE);
	Edit.SetFont(pFont);

	CRect rctButton = rctClient;
	rctButton.top = rctClient.bottom - m_dpi_behavior.ScaleOnY(20);

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

	CRect rctEdit = rctClient;
	rctEdit.bottom = rctClient.bottom - m_dpi_behavior.ScaleOnY(20);
	Edit.MoveWindow(rctEdit);

	CRect rctButton = rctClient;
	rctButton.top = rctClient.bottom - m_dpi_behavior.ScaleOnY(20);
	m_btnCopyAll.MoveWindow(rctButton);
}

DpiAwareFont* CProcessWnd::CreateFont()
{
	delete pFont;
	
	pFont = new DpiAwareFont{m_dpi_behavior.GetCurrentDpiY()};
	// load font
	pFont->CreatePointFont(10 * 10, "Courier New");

	return pFont;
}

//-----------------------------------------------------------------------------
// Purpose: Prepare the process window for display. If it has not been created
//			yet, register the class and create it.
//-----------------------------------------------------------------------------
void CProcessWnd::GetReady(LPCTSTR pszDocName)
{
	if (!IsWindow(m_hWnd))
	{
		CString windowClass = AfxRegisterWndClass
		(
			0,
			AfxGetApp()->LoadStandardCursor(IDC_ARROW),
			HBRUSH(GetStockObject(WHITE_BRUSH))
		);

		CString windowTitle;
		// dimhotepus: Add compiling map name to title.
		windowTitle.Format("Compile - [%s]", pszDocName);

		CreateEx(0, windowClass, windowTitle, WS_OVERLAPPEDWINDOW,
			50,	50,	600, 400,
			AfxGetMainWnd()->GetSafeHwnd(), nullptr);
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
					V_strncpy(tstrCopy, text.GetString(), text.GetLength() + sizeof(TCHAR));
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

LRESULT CProcessWnd::OnDpiChanged(WPARAM wParam, LPARAM lParam)
{
	const LRESULT rc{__super::OnDpiChanged(wParam, lParam)};

	pFont = CreateFont();

	return rc;
}
