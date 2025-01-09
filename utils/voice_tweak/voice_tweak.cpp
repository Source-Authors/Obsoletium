// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "voice_tweak.h"
#include "waveout.h"
#include "audio/private/voice_mixer_controls.h"
#include "voice_tweakDlg.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

// extern IVoiceRecord*	CreateVoiceRecord_WaveIn(int sampleRate);
extern IVoiceRecord* CreateVoiceRecord_DSound(int sampleRate);

typedef enum {
  LANGUAGE_ENGLISH = 0,
  LANGUAGE_SPANISH = 1,
  LANGUAGE_FRENCH = 2,
  LANGUAGE_ITALIAN = 3,
  LANGUAGE_GERMAN = 4,
  LANGUAGE_COUNT = 5
} VoiceTweakLanguageID;

VoiceTweakLanguageID g_CurrentLanguage = LANGUAGE_ENGLISH;

#define LANGENTRY(name)                                     \
  {IDS_##name##, IDS_SPANISH_##name##, IDS_FRENCH_##name##, \
   IDS_ITALIAN_##name##, IDS_GERMAN_##name##}

int g_StringIDs[][LANGUAGE_COUNT] = {
    LANGENTRY(HELPTEXT),
    LANGENTRY(ERROR),
    LANGENTRY(CANTFINDMICBOOST),
    LANGENTRY(CANTFINDMICVOLUME),
    LANGENTRY(CANTFINDMICMUTE),
    LANGENTRY(CANTCREATEWAVEIN),
    LANGENTRY(CANTLOADVOICEMODULE),
    LANGENTRY(CANTCREATEWAVEOUT),
    LANGENTRY(NODPLAYVOICE),
    LANGENTRY(WINDOWTITLE),
    LANGENTRY(OKAY),
    LANGENTRY(CANCEL),
    LANGENTRY(SYSTEMSETUP),
    LANGENTRY(HELP),
    LANGENTRY(VOICEINPUT),
    LANGENTRY(VOLUME),
    LANGENTRY(ENABLEGAIN),
};
#define NUM_STRINGIDS (sizeof(g_StringIDs) / sizeof(g_StringIDs[0]))

// Pass in the english string ID, and this returns the string ID in the current
// language.
int MapLanguageStringID(int idEnglish) {
  for (int i = 0; i < NUM_STRINGIDS; i++) {
    if (idEnglish == g_StringIDs[i][LANGUAGE_ENGLISH])
      return g_StringIDs[i][g_CurrentLanguage];
  }

  assert(!"MapLanguageStringID: unknown string ID");
  return 0;
}

/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp

BEGIN_MESSAGE_MAP(CVoiceTweakApp, CWinApp)
  //{{AFX_MSG_MAP(CVoiceTweakApp)
  // NOTE - the ClassWizard will add and remove mapping macros here.
  //    DO NOT EDIT what you see in these blocks of generated code!
  //}}AFX_MSG
  ON_COMMAND(ID_HELP, CWinApp::OnHelp)
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp construction

CVoiceTweakApp::CVoiceTweakApp() {
  m_pVoiceRecord = 0;
  m_pWaveOut = 0;

  m_pMixerControls = 0;
  ShutdownMixerControls();
}

CVoiceTweakApp::~CVoiceTweakApp() { StopDevices(); }

bool CVoiceTweakApp::StartDevices() {
  StopDevices();

  // Setup wave in.
  if (!(m_pVoiceRecord = CreateVoiceRecord_DSound(VOICE_TWEAK_SAMPLE_RATE))) {
    AfxMessageBox(MapLanguageStringID(IDS_CANTCREATEWAVEIN),
                  MB_OK | MB_ICONERROR);
    return false;
  }

  m_pVoiceRecord->RecordStart();

  if (!(m_pWaveOut = CreateWaveOut(VOICE_TWEAK_SAMPLE_RATE))) {
    AfxMessageBox(MapLanguageStringID(IDS_CANTCREATEWAVEOUT),
                  MB_OK | MB_ICONERROR);
    return false;
  }

  return true;
}

void CVoiceTweakApp::StopDevices() {
  if (m_pVoiceRecord) {
    m_pVoiceRecord->Release();
    m_pVoiceRecord = NULL;
  }

  if (m_pWaveOut) {
    m_pWaveOut->Release();
    m_pWaveOut = NULL;
  }
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CVoiceTweakApp object

CVoiceTweakApp theApp;

char const* FindArg(char const* pName) {
  for (int i = 0; i < __argc; i++)
    if (_stricmp(__argv[i], pName) == 0)
      return ((i + 1) < __argc) ? __argv[i + 1] : "";

  return NULL;
}

/////////////////////////////////////////////////////////////////////////////
// CVoiceTweakApp initialization
BOOL CVoiceTweakApp::InitInstance() {
  // Set the thread locale so it grabs the string resources for the right
  // language. If we don't have resources for the system default language, it
  // just uses English.
  if (FindArg("-french"))
    g_CurrentLanguage = LANGUAGE_FRENCH;
  else if (FindArg("-spanish"))
    g_CurrentLanguage = LANGUAGE_SPANISH;
  else if (FindArg("-italian"))
    g_CurrentLanguage = LANGUAGE_ITALIAN;
  else if (FindArg("-german"))
    g_CurrentLanguage = LANGUAGE_GERMAN;
  else
    g_CurrentLanguage = LANGUAGE_ENGLISH;

  InitMixerControls();
  m_pMixerControls = g_pMixerControls;

  // Initialize the mixer controls.
  float volume, mute;
  bool bFoundVolume = m_pMixerControls->GetValue_Float(
      IMixerControls::Control::MicVolume, volume);
  bool bFoundMute =
      m_pMixerControls->GetValue_Float(IMixerControls::Control::MicMute, mute);

  if (!bFoundVolume) {
    AfxMessageBox(MapLanguageStringID(IDS_CANTFINDMICVOLUME),
                  MB_OK | MB_ICONERROR);
    return FALSE;
  }

  if (!bFoundMute) {
    AfxMessageBox(MapLanguageStringID(IDS_CANTFINDMICMUTE),
                  MB_OK | MB_ICONERROR);
    return FALSE;
  }

  // Set mute and boost for them automatically.
  m_pMixerControls->SetValue_Float(IMixerControls::Control::MicMute, 1);

  // We cycle the mic boost because for some reason Windows misses the first
  // call to set it to 1, but if the user clicks the checkbox on and off again,
  // it works.
  m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 1);
  m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 0);
  m_pMixerControls->SetValue_Float(IMixerControls::Control::MicBoost, 1);

  // Enable the mic for wave input.
  m_pMixerControls->SelectMicrophoneForWaveInput();

  if (!StartDevices()) return false;

    // Standard initialization
    // If you are not using these features and wish to reduce the size
    //  of your final executable, you should remove from the following
    //  the specific initialization routines you do not need.

#ifdef _AFXDLL
  Enable3dControls();  // Call this when using MFC in a shared DLL
#endif

  CVoiceTweakDlg dlg;
  m_pMainWnd = &dlg;

  INT_PTR nResponse = dlg.DoModal();
  if (nResponse == IDOK) {
    // TODO: Place code here to handle when the dialog is
    //  dismissed with OK
  } else if (nResponse == IDCANCEL) {
    // TODO: Place code here to handle when the dialog is
    //  dismissed with Cancel
  }

  // Since the dialog has been closed, return FALSE so that we exit the
  //  application, rather than start the application's message pump.
  return FALSE;
}
