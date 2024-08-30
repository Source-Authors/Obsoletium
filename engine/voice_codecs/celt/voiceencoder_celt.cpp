// Copyright Valve Corporation, All rights reserved.

#include "ivoicecodec.h"
#include "iframeencoder.h"

#include <cstdio>

#include "celt.h"

// NOTE: This has to be the last file included!
#include "tier0/memdbgon.h"

namespace {

struct CeltPreset {
  int sample_rate;
  int frame_size;
  int packet_size;
};

constexpr CeltPreset kCeltPresets[]{
    // Vocoder (mostly for comfortable noise)
    {8000, 120, 60},
    // Very noticeable artifacts/noise, good intelligibility.
    {8000, 240, 60},
    // Artifacts/noise sometimes noticeable with / without headphones.
    {16000, 256, 120},
    // Need good headphones to tell the difference or hard to tell even with
    // good headphones.
    {32000, 256, 120},
    // Completely transparent for voice, good quality music.
    {48000, 512, 240}};

constexpr inline int kEncoderDecoderChannelsNo{1};

}  // namespace

extern IVoiceCodec *CreateVoiceCodec_Frame(IFrameEncoder *pEncoder);

namespace se::audio::voice_codecs::celt {

class VoiceEncoder_Celt : public IFrameEncoder {
 public:
  VoiceEncoder_Celt();
  virtual ~VoiceEncoder_Celt();

  // Interfaces IFrameDecoder

  bool Init(VoiceCodecQuality quality, int &raw_frame_size,
            int &encoded_frame_size) override;
  void Release() override;
  void DecodeFrame(const char *in, char *out) override;
  void EncodeFrame(const char *in, char *out) override;
  bool ResetState() override;

 private:
  bool InitStates();
  void TermStates();

  CELTEncoder *encoder_state_;  // Celt internal encoder state
  CELTDecoder *decoder_state_;  // Celt internal decoder state
  CELTMode *mode_;

  CeltPreset preset_;
};

VoiceEncoder_Celt::VoiceEncoder_Celt() {
  encoder_state_ = nullptr;
  decoder_state_ = nullptr;
  mode_ = nullptr;

  memset(&preset_, 0, sizeof(preset_));
}

VoiceEncoder_Celt::~VoiceEncoder_Celt() {
  TermStates();

  Msg("Shut down CELT codec %u (bit stream version)\n", CELT_GET_BITSTREAM_VERSION);
}

bool VoiceEncoder_Celt::Init(VoiceCodecQuality quality, int &raw_frame_size,
                             int &encoded_frame_size) {
  static_assert(to_underlying(VoiceCodecQuality::Perfect) <
                ssize(kCeltPresets));
  if (to_underlying(quality) < 0 ||
      to_underlying(quality) >= ssize(kCeltPresets)) {
    return false;
  }

  preset_ = kCeltPresets[to_underlying(quality)];
  raw_frame_size = preset_.frame_size * BYTES_PER_SAMPLE;

  Msg("Start up CELT codec %u  (bit stream version)\n", CELT_GET_BITSTREAM_VERSION);

  int rc = 0;

  mode_ = celt_mode_create(preset_.sample_rate, preset_.frame_size, &rc);
  if (!mode_) {
    Warning(
        "CELT codec (%d Hz, %d channel(s), %d samples/channel) startup "
        "failure: "
        "%s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        celt_strerror(rc));
    return false;
  }

  encoder_state_ =
      celt_encoder_create_custom(mode_, kEncoderDecoderChannelsNo, &rc);
  if (!encoder_state_) {
    Warning(
        "CELT encoder (%d Hz, %d channel(s), %d samples/channel) startup "
        "failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        celt_strerror(rc));
    return false;
  }

  decoder_state_ =
      celt_decoder_create_custom(mode_, kEncoderDecoderChannelsNo, &rc);
  if (!decoder_state_) {
    Warning(
        "CELT decoder (%d Hz, %d channel(s), %d samples/channel) startup "
        "failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        celt_strerror(rc));
    return false;
  }

  if (!InitStates()) return false;

  encoded_frame_size = preset_.packet_size;

  DevMsg("CELT codec (%d Hz, %d channel(s), %d samples/channel) started up\n",
         preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size);

  return true;
}

void VoiceEncoder_Celt::Release() { delete this; }

void VoiceEncoder_Celt::EncodeFrame(const char *in, char *out) {
  unsigned char output[1024];

  Assert(ssize(output) >= preset_.packet_size);

  const int rc{celt_encode(encoder_state_,
                           reinterpret_cast<const celt_int16 *>(in),
                           preset_.frame_size, output, preset_.packet_size)};
  if (rc < 0) {
    Warning(
        "CELT encoder (%d Hz, %d channel(s), %d samples/channel, %d max out "
        "bytes) failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        preset_.packet_size, celt_strerror(rc));
    // Skip.
    return;
  }

  // Encoded size should be packet one. We do not use VBR. If they do not match,
  // decoder will fail.
  if (rc != preset_.packet_size) {
    Warning(
        "CELT encoder (%d Hz, %d channel(s), %d samples/channel, %d max out "
        "bytes) failure: encoded size %d is not same as expected %d one\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        preset_.packet_size, rc, preset_.packet_size);
    // Skip.
    return;
  }

  for (int i = 0; i < preset_.packet_size; i++) {
    *out = static_cast<char>(output[i]);

    ++out;
  }
}

void VoiceEncoder_Celt::DecodeFrame(const char *in, char *out) {
  unsigned char output[1024];

  Assert(ssize(output) >= preset_.packet_size);

  if (!in) {
    const int rc{celt_decode(decoder_state_, nullptr, preset_.packet_size,
                             reinterpret_cast<celt_int16 *>(out),
                             preset_.frame_size)};
    if (rc) {
      Warning(
          "CELT decoder (%d Hz, %d channel(s), %d samples/channel, %d in "
          "bytes) "
          "zero data failure: %s\n",
          preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
          preset_.packet_size, celt_strerror(rc));
    }
    return;
  }

  const char *it{in};
  for (int i = 0; i < preset_.packet_size; i++) {
    const char s{*it};

    output[i] = static_cast<unsigned char>(s < 0 ? (s + 256) : s);
    ++it;
  }

  const int rc{celt_decode(decoder_state_, output, preset_.packet_size,
                           reinterpret_cast<celt_int16 *>(out),
                           preset_.frame_size)};
  if (rc < 0) {
    Warning(
        "CELT decoder (%d Hz, %d channel(s), %d samples/channel, %d in bytes) "
        "fill data failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        preset_.packet_size, celt_strerror(rc));
  }
}

bool VoiceEncoder_Celt::ResetState() {
  int rc{celt_encoder_ctl(encoder_state_, CELT_RESET_STATE_REQUEST, nullptr)};
  if (rc) {
    Warning(
        "CELT encoder (%d Hz, %d channel(s), %d samples/channel, %d max out "
        "bytes) reset failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        preset_.packet_size, celt_strerror(rc));
    return false;
  }

  rc = celt_decoder_ctl(decoder_state_, CELT_RESET_STATE_REQUEST, nullptr);
  if (rc) {
    Warning(
        "CELT decoder (%d Hz, %d channel(s), %d samples/channel, %d in bytes) "
        "reset failure: %s\n",
        preset_.sample_rate, kEncoderDecoderChannelsNo, preset_.frame_size,
        preset_.packet_size, celt_strerror(rc));
    return false;
  }

  return true;
}

bool VoiceEncoder_Celt::InitStates() {
  if (!encoder_state_ || !decoder_state_) return false;

  return ResetState();
}

void VoiceEncoder_Celt::TermStates() {
  if (encoder_state_) {
    celt_encoder_destroy(encoder_state_);
    encoder_state_ = nullptr;
  }

  if (decoder_state_) {
    celt_decoder_destroy(decoder_state_);
    decoder_state_ = nullptr;
  }

  if (mode_) {
    celt_mode_destroy(mode_);
    mode_ = nullptr;
  }
}

}  // namespace se::audio::voice_codecs::celt

namespace {

void *CreateCeltVoiceCodec() {
  return CreateVoiceCodec_Frame(
      new se::audio::voice_codecs::celt::VoiceEncoder_Celt);
}

}  // namespace

EXPOSE_INTERFACE_FN(CreateCeltVoiceCodec, IVoiceCodec, "vaudio_celt")
