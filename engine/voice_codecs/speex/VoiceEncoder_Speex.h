// Copyright © 1996-2005, Valve Corporation, All rights reserved.

#ifndef VOICEENCODER_SPEEX_H
#define VOICEENCODER_SPEEX_H

#include "iframeencoder.h"

#include "speex/speex.h"

namespace se::audio::voice_codecs::speex {

class VoiceEncoder_Speex : public IFrameEncoder {
 public:
  VoiceEncoder_Speex();
  virtual ~VoiceEncoder_Speex();

  bool Init(VoiceCodecQuality quality, int &rawFrameSize,
            int &encodedFrameSize) override;
  void Release() override;
  void DecodeFrame(const char *pCompressed, char *pDecompressedBytes) override;
  void EncodeFrame(const char *pUncompressedBytes, char *pCompressed) override;
  bool ResetState() override;

 private:
  bool InitStates();
  void TermStates();

  int quality_;          // voice codec quality ( 0,2,4,6,8 )
  void *encoder_state_;  // speex internal encoder state
  void *decoder_state_;  // speex internal decoder state

  SpeexBits bits_;  // helpful bit buffer structure

  [[nodiscard]] bool EncoderCtl(int request, void *arg, const char *arg_description);
  [[nodiscard]] bool DecoderCtl(int request, void *arg, const char *arg_description);
};

}  // namespace se::audio::voice_codecs::speex

#endif  // !VOICEENCODER_SPEEX_H
