// MPEGAudioInfoDlg.h : header file
//
#pragma once

#include "mpafile.h"
#include "afxwin.h"

// CMPEGAudioInfoDlg dialog
class CMPEGAudioInfoDlg : public CDialog
{
// Construction
public:
	CMPEGAudioInfoDlg(CWnd* pParent = NULL);	// standard constructor
	CMPEGAudioInfoDlg::~CMPEGAudioInfoDlg();

// Dialog Data
	enum { IDD = IDD_MPEGAUDIOINFO_DIALOG };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support


// Implementation
private:
	HICON m_hIcon;
	CMPAFile* m_pMPAFile;
	CMPAFrame* m_pFrame;	// current frame
	DWORD m_dwFrameNo;

	void InvalidChange(LPCTSTR szWhat, DWORD dwFrame, LPCTSTR szOldValue, LPCTSTR szNewValue);

	// Generated message map functions
	DECLARE_MESSAGE_MAP()
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnBnClickedOpenFile();
	afx_msg void OnBnClickedNextframe();
	afx_msg void OnBnClickedPrevframe();
	afx_msg void OnBnClickedFirstframe();
	afx_msg void OnBnClickedLastframe();
	afx_msg void OnBnClickedCheckfile();
	afx_msg void OnBnClickedAbout();

	void LoadMPEGFile(LPCTSTR szFile);
	void GetFrameInfo(CMPAFrame* pFrame, DWORD dwFrameNo);
	CString IsVarTrue(bool bVar);

	CString m_strFile;
	CString m_strInfo1;
	CString m_strInfo2;
	
	CString m_strVBRInfo;
	CString m_strFrameNo;
	CString m_strOutput;
	
	CString m_strTagInfo;
	CString m_strFileInfo;
	
	CButton m_CtrlPrevFrame;
	CButton m_CtrlNextFrame;
	CButton m_CtrlLastFrame;
	CButton m_CtrlFirstFrame;
	CButton m_CtrlCheckFile;	

	//void CMPEGAudioInfoDlg::RecurseFindFile(LPCTSTR szPath);

public:
	afx_msg void OnDropFiles(HDROP hDropInfo);
};
