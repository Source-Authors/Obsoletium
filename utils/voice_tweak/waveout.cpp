// Copyright Valve Corporation, All rights reserved.

#include "stdafx.h"

#include "waveout.h"

#include "ivoicecodec.h"

struct WaveOutHdr {
  WAVEHDR m_Hdr;
  WaveOutHdr *m_pNext;
  char m_Data[1];
};

class CWaveOut : public IWaveOut {
 public:
  CWaveOut();
  virtual ~CWaveOut();

  void Release() override;
  bool PutSamples(short *pSamples, int nSamples) override;
  void Idle() override;
  int GetNumBufferedSamples() override;

 public:
  bool Init(unsigned long sampleRate);
  void Term();

 private:
  void KillOldHeaders();

 private:
  HWAVEOUT m_hWaveOut;
  WaveOutHdr m_Headers;  // Head of a linked list of WAVEHDRs.
  int m_nBufferedSamples;
};

CWaveOut::CWaveOut() : m_hWaveOut{nullptr}, m_nBufferedSamples{0} {
  memset(&m_Headers, 0, sizeof(m_Headers));
}

CWaveOut::~CWaveOut() { Term(); }

void CWaveOut::Release() { delete this; }

bool CWaveOut::PutSamples(short *pInSamples, int nInSamples) {
  constexpr int granularity = 2048;

  while (nInSamples) {
    int nSamples = (nInSamples > granularity) ? granularity : nInSamples;

    short *pSamples = pInSamples;

    nInSamples -= nSamples;
    pInSamples += nSamples;

    if (!m_hWaveOut) return false;

    // Kill any old headers..
    KillOldHeaders();

    // Allocate a header..
    auto *pHdr = (WaveOutHdr *)malloc(sizeof(WaveOutHdr) - 1 + nSamples * 2);
    if (!pHdr) return false;

    // Make a new one.
    memset(&pHdr->m_Hdr, 0, sizeof(pHdr->m_Hdr));
    pHdr->m_Hdr.lpData = pHdr->m_Data;
    pHdr->m_Hdr.dwBufferLength = nSamples * 2;
    memcpy(pHdr->m_Data, pSamples, nSamples * 2);

    MMRESULT rc =
        waveOutPrepareHeader(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
    if (rc != MMSYSERR_NOERROR) return false;

    rc = waveOutWrite(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
    Assert(rc == MMSYSERR_NOERROR);

    if (rc != MMSYSERR_NOERROR) {
      free(pHdr);

      rc =
          waveOutUnprepareHeader(m_hWaveOut, &pHdr->m_Hdr, sizeof(pHdr->m_Hdr));
      Assert(rc == MMSYSERR_NOERROR);

      return false;
    }

    m_nBufferedSamples += nSamples;

    // Queue up this header until waveOut is done with it.
    pHdr->m_pNext = m_Headers.m_pNext;

    m_Headers.m_pNext = pHdr;
  }

  return true;
}

void CWaveOut::Idle() { KillOldHeaders(); }

int CWaveOut::GetNumBufferedSamples() { return m_nBufferedSamples; }

bool CWaveOut::Init(unsigned long sampleRate) {
  Term();

  WAVEFORMATEX format = {WAVE_FORMAT_PCM,                // wFormatTag
                         1,                              // nChannels
                         sampleRate,                     // nSamplesPerSec
                         sampleRate * BYTES_PER_SAMPLE,  // nAvgBytesPerSec
                         BYTES_PER_SAMPLE,               // nBlockAlign
                         BYTES_PER_SAMPLE * 8,           // wBitsPerSample
                         sizeof(WAVEFORMATEX)};

  const MMRESULT rc = waveOutOpen(&m_hWaveOut, 0, &format, 0, 0, CALLBACK_NULL);
  Assert(rc == MMSYSERR_NOERROR);

  return rc == MMSYSERR_NOERROR;
}

void CWaveOut::Term() {
  if (m_hWaveOut) {
    [[maybe_unused]] const MMRESULT rc = waveOutClose(m_hWaveOut);
    Assert(rc == MMSYSERR_NOERROR);

    m_hWaveOut = nullptr;
  }
}

void CWaveOut::KillOldHeaders() {
  // Look for any headers windows is done with.
  WaveOutHdr *next{nullptr};
  WaveOutHdr **prev{&m_Headers.m_pNext};

  for (auto *it = m_Headers.m_pNext; it; it = next) {
    next = it->m_pNext;

    if (it->m_Hdr.dwFlags & WHDR_DONE) {
      m_nBufferedSamples -= (int)(it->m_Hdr.dwBufferLength / 2);
      Assert(m_nBufferedSamples >= 0);

      [[maybe_unused]] const MMRESULT rc =
          waveOutUnprepareHeader(m_hWaveOut, &it->m_Hdr, sizeof(it->m_Hdr));
      Assert(rc == MMSYSERR_NOERROR);

      *prev = it->m_pNext;
      free(it);
    } else {
      prev = &it->m_pNext;
    }
  }
}

IWaveOut *CreateWaveOut(unsigned long sampleRate) {
  auto *out = new CWaveOut;
  if (out && out->Init(sampleRate)) return out;

  delete out;
  return nullptr;
}
